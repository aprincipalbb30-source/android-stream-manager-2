#include "prometheus_exporter.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstring>
#include <chrono>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#endif

namespace AndroidStreamManager {

PrometheusExporter::PrometheusExporter()
    : running_(false)
    , port_(9090)
    , bindAddress_("0.0.0.0") {
    std::cout << "PrometheusExporter criado" << std::endl;
}

PrometheusExporter::~PrometheusExporter() {
    shutdown();
    std::cout << "PrometheusExporter destruído" << std::endl;
}

bool PrometheusExporter::initialize(int port, const std::string& bindAddress) {
    port_ = port;
    bindAddress_ = bindAddress;

    httpServer_ = std::make_unique<MetricsHttpServer>();

    std::cout << "PrometheusExporter inicializado na porta " << port_ << std::endl;
    return true;
}

void PrometheusExporter::shutdown() {
    stop();
}

bool PrometheusExporter::start() {
    if (running_) {
        std::cout << "PrometheusExporter já está rodando" << std::endl;
        return true;
    }

    if (!httpServer_->start(port_, bindAddress_)) {
        std::cerr << "Falha ao iniciar servidor HTTP de métricas" << std::endl;
        return false;
    }

    running_ = true;
    std::cout << "PrometheusExporter iniciado em " << bindAddress_ << ":" << port_ << std::endl;
    std::cout << "Endpoint de métricas: " << getMetricsEndpoint() << std::endl;

    return true;
}

void PrometheusExporter::stop() {
    if (!running_) {
        return;
    }

    running_ = false;

    if (httpServer_) {
        httpServer_->stop();
    }

    std::cout << "PrometheusExporter parado" << std::endl;
}

bool PrometheusExporter::isRunning() const {
    return running_;
}

void PrometheusExporter::addCustomMetric(const std::string& name, const std::string& help,
                                       MetricType type, const std::unordered_map<std::string, std::string>& labels) {
    std::lock_guard<std::mutex> lock(customMetricsMutex_);
    customMetrics_[name] = std::make_tuple(help, type, labels, 0.0);
    std::cout << "Métrica customizada adicionada: " << name << std::endl;
}

void PrometheusExporter::updateCustomMetric(const std::string& name, double value) {
    std::lock_guard<std::mutex> lock(customMetricsMutex_);
    auto it = customMetrics_.find(name);
    if (it != customMetrics_.end()) {
        std::get<3>(it->second) = value;
    }
}

void PrometheusExporter::removeCustomMetric(const std::string& name) {
    std::lock_guard<std::mutex> lock(customMetricsMutex_);
    customMetrics_.erase(name);
    std::cout << "Métrica customizada removida: " << name << std::endl;
}

std::string PrometheusExporter::getMetricsEndpoint() const {
    return "http://" + bindAddress_ + ":" + std::to_string(port_) + "/metrics";
}

PrometheusExporter::ExporterStats PrometheusExporter::getStats() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    return stats_;
}

// ========== MetricsHttpServer Implementation ==========

PrometheusExporter::MetricsHttpServer::MetricsHttpServer()
    : serverSocket_(-1)
    , running_(false)
    , port_(9090) {
}

PrometheusExporter::MetricsHttpServer::~MetricsHttpServer() {
    stop();
}

bool PrometheusExporter::MetricsHttpServer::start(int port, const std::string& bindAddress) {
    port_ = port;
    bindAddress_ = bindAddress;

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "Falha ao inicializar Winsock" << std::endl;
        return false;
    }
#endif

    // Criar socket TCP
    serverSocket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket_ < 0) {
        std::cerr << "Erro ao criar socket" << std::endl;
        return false;
    }

    // Configurar socket para reutilização
    int opt = 1;
#ifdef _WIN32
    setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
#else
    setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    // Configurar endereço
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port_);

    if (bindAddress_ == "0.0.0.0") {
        serverAddr.sin_addr.s_addr = INADDR_ANY;
    } else {
        inet_pton(AF_INET, bindAddress_.c_str(), &serverAddr.sin_addr);
    }

    // Bind
    if (bind(serverSocket_, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Erro no bind da porta " << port_ << std::endl;
#ifdef _WIN32
        closesocket(serverSocket_);
#else
        close(serverSocket_);
#endif
        return false;
    }

    // Listen
    if (listen(serverSocket_, 10) < 0) {
        std::cerr << "Erro no listen" << std::endl;
#ifdef _WIN32
        closesocket(serverSocket_);
#else
        close(serverSocket_);
#endif
        return false;
    }

    running_ = true;
    serverThread_ = std::thread(&MetricsHttpServer::serverLoop, this);

    std::cout << "MetricsHttpServer ouvindo na porta " << port_ << std::endl;
    return true;
}

void PrometheusExporter::MetricsHttpServer::stop() {
    if (!running_) {
        return;
    }

    running_ = false;

    if (serverSocket_ >= 0) {
#ifdef _WIN32
        closesocket(serverSocket_);
#else
        close(serverSocket_);
#endif
        serverSocket_ = -1;
    }

    if (serverThread_.joinable()) {
        serverThread_.join();
    }

#ifdef _WIN32
    WSACleanup();
#endif

    std::cout << "MetricsHttpServer parado" << std::endl;
}

bool PrometheusExporter::MetricsHttpServer::isRunning() const {
    return running_;
}

void PrometheusExporter::MetricsHttpServer::serverLoop() {
    std::cout << "MetricsHttpServer loop iniciado" << std::endl;

    while (running_) {
        sockaddr_in clientAddr{};
#ifdef _WIN32
        int clientAddrLen = sizeof(clientAddr);
#else
        socklen_t clientAddrLen = sizeof(clientAddr);
#endif

        // Accept
        int clientSocket = accept(serverSocket_, (struct sockaddr*)&clientAddr, &clientAddrLen);
        if (clientSocket < 0) {
            if (running_) {
                std::cerr << "Erro no accept" << std::endl;
            }
            continue;
        }

        // Processar requisição em uma nova thread
        std::thread(&MetricsHttpServer::handleClient, this, clientSocket).detach();
    }

    std::cout << "MetricsHttpServer loop finalizado" << std::endl;
}

void PrometheusExporter::MetricsHttpServer::handleClient(int clientSocket) {
    try {
        // Receber requisição HTTP
        char buffer[4096];
        ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);

        if (bytesRead <= 0) {
#ifdef _WIN32
            closesocket(clientSocket);
#else
            close(clientSocket);
#endif
            return;
        }

        buffer[bytesRead] = '\0';
        std::string request(buffer);

        // Parse da primeira linha
        std::istringstream requestStream(request);
        std::string method, path, version;
        std::getline(requestStream, method, ' ');
        std::getline(requestStream, path, ' ');
        std::getline(requestStream, version);

        // Remover query string se presente
        size_t queryPos = path.find('?');
        if (queryPos != std::string::npos) {
            path = path.substr(0, queryPos);
        }

        // Processar requisição
        std::string response;
        if (path == "/metrics") {
            response = handleMetricsRequest();
        } else if (path == "/health") {
            response = handleHealthRequest();
        } else {
            response = handleNotFound();
        }

        // Enviar resposta
        send(clientSocket, response.c_str(), response.length(), 0);

    } catch (const std::exception& e) {
        std::cerr << "Erro no handleClient: " << e.what() << std::endl;
    }

#ifdef _WIN32
    closesocket(clientSocket);
#else
    close(clientSocket);
#endif
}

std::string PrometheusExporter::MetricsHttpServer::handleMetricsRequest() {
    std::ostringstream response;

    // Headers HTTP
    response << "HTTP/1.1 200 OK\r\n";
    response << "Content-Type: text/plain; version=0.0.4; charset=utf-8\r\n";
    response << "Connection: close\r\n";
    response << "\r\n";

    // Métricas do sistema
    auto systemMetrics = MetricsCollector::getInstance().getSystemMetrics();
    response << "# HELP android_stream_manager_cpu_usage_percent CPU usage percentage\n";
    response << "# TYPE android_stream_manager_cpu_usage_percent gauge\n";
    response << "android_stream_manager_cpu_usage_percent " << systemMetrics.cpu_usage_percent << "\n";

    response << "# HELP android_stream_manager_memory_used_bytes Memory used in bytes\n";
    response << "# TYPE android_stream_manager_memory_used_bytes gauge\n";
    response << "android_stream_manager_memory_used_bytes " << systemMetrics.memory_used_bytes << "\n";

    response << "# HELP android_stream_manager_disk_used_bytes Disk used in bytes\n";
    response << "# TYPE android_stream_manager_disk_used_bytes gauge\n";
    response << "android_stream_manager_disk_used_bytes " << systemMetrics.disk_used_bytes << "\n";

    // Métricas de dispositivos
    auto deviceMetrics = MetricsCollector::getInstance().getDeviceMetrics();
    for (const auto& device : deviceMetrics) {
        std::string deviceLabel = "{device_id=\"" + device.device_id + "\"}";

        response << "# HELP android_stream_manager_device_connected Device connection status\n";
        response << "# TYPE android_stream_manager_device_connected gauge\n";
        response << "android_stream_manager_device_connected" << deviceLabel << " "
                << (device.connected ? 1 : 0) << "\n";

        response << "# HELP android_stream_manager_device_battery_level Device battery level\n";
        response << "# TYPE android_stream_manager_device_battery_level gauge\n";
        response << "android_stream_manager_device_battery_level" << deviceLabel << " "
                << device.battery_level << "\n";
    }

    // Métricas de aplicação
    auto appMetrics = MetricsCollector::getInstance().getApplicationMetrics();
    response << "# HELP android_stream_manager_requests_total Total requests\n";
    response << "# TYPE android_stream_manager_requests_total counter\n";
    response << "android_stream_manager_requests_total " << appMetrics.total_requests << "\n";

    response << "# HELP android_stream_manager_active_connections Active connections\n";
    response << "# TYPE android_stream_manager_active_connections gauge\n";
    response << "android_stream_manager_active_connections " << appMetrics.active_connections << "\n";

    response << "# HELP android_stream_manager_cache_hit_rate Cache hit rate\n";
    response << "# TYPE android_stream_manager_cache_hit_rate gauge\n";
    response << "android_stream_manager_cache_hit_rate " << appMetrics.cache_hit_rate << "\n";

    return response.str();
}

std::string PrometheusExporter::MetricsHttpServer::handleHealthRequest() {
    std::ostringstream response;

    response << "HTTP/1.1 200 OK\r\n";
    response << "Content-Type: application/json\r\n";
    response << "Connection: close\r\n";
    response << "\r\n";

    response << R"({
  "status": "healthy",
  "service": "Android Stream Manager Metrics Exporter",
  "version": "1.0.0",
  "timestamp": ")" << std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count() << "\"}";

    return response.str();
}

std::string PrometheusExporter::MetricsHttpServer::handleNotFound() {
    std::ostringstream response;

    response << "HTTP/1.1 404 Not Found\r\n";
    response << "Content-Type: text/plain\r\n";
    response << "Connection: close\r\n";
    response << "\r\n";

    response << "404 Not Found\n";
    response << "Available endpoints:\n";
    response << "  GET /metrics - Prometheus metrics\n";
    response << "  GET /health  - Health check\n";

    return response.str();
}

} // namespace AndroidStreamManager