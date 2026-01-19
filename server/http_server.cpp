#include "http_server.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstring>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

namespace AndroidStreamManager {

HttpServer::HttpServer()
    : port_(8443)
    , httpsEnabled_(false)
    , requireAuth_(true)
    , corsEnabled_(false)
    , running_(false)
    , totalRequests_(0)
    , activeConnections_(0)
    , errorCount_(0) {
    std::cout << "HttpServer criado" << std::endl;
}

HttpServer::~HttpServer() {
    stop();
    std::cout << "HttpServer destruído" << std::endl;
}

bool HttpServer::initialize(int port, const std::string& certPath, const std::string& keyPath) {
    port_ = port;
    certPath_ = certPath;
    keyPath_ = keyPath;
    httpsEnabled_ = !certPath.empty() && !keyPath.empty();

    if (httpsEnabled_) {
        std::cout << "HTTPS habilitado nas portas " << port_ << std::endl;
    } else {
        std::cout << "HTTP habilitado na porta " << port_ << std::endl;
    }

    return true;
}

bool HttpServer::start() {
    if (running_) {
        std::cout << "HttpServer já está rodando" << std::endl;
        return true;
    }

    try {
        running_ = true;
        startTime_ = std::chrono::system_clock::now();

        serverThread_ = std::thread(&HttpServer::serverLoop, this);

        std::cout << "HttpServer iniciado na porta " << port_ << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "Erro ao iniciar HttpServer: " << e.what() << std::endl;
        running_ = false;
        return false;
    }
}

void HttpServer::stop() {
    if (!running_) {
        return;
    }

    running_ = false;

    if (serverThread_.joinable()) {
        serverThread_.join();
    }

    std::cout << "HttpServer parado" << std::endl;
}

bool HttpServer::isRunning() const {
    return running_;
}

void HttpServer::addRoute(const std::string& method, const std::string& path, HttpRequestHandler handler) {
    std::string key = method + " " + path;
    routes_[key] = handler;
    std::cout << "Rota registrada: " << key << std::endl;
}

void HttpServer::addRoute(const std::string& method, const std::string& path, const std::string& response) {
    addRoute(method, path, [response](const std::string&, const std::string&, const std::string&,
                                    const std::unordered_map<std::string, std::string>&) {
        return response;
    });
}

void HttpServer::setAuthCallback(HttpAuthCallback callback) {
    authCallback_ = callback;
}

void HttpServer::setRequireAuth(bool require) {
    requireAuth_ = require;
}

void HttpServer::enableCors(bool enable) {
    corsEnabled_ = enable;
}

void HttpServer::addAllowedOrigin(const std::string& origin) {
    allowedOrigins_.push_back(origin);
}

HttpServer::HttpStats HttpServer::getStats() const {
    std::lock_guard<std::mutex> lock(statsMutex_);

    HttpStats stats;
    stats.totalRequests = totalRequests_;
    stats.activeConnections = activeConnections_;
    stats.errorCount = errorCount_;
    stats.uptime = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now() - startTime_);

    return stats;
}

void HttpServer::serverLoop() {
    // Criar socket TCP
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        std::cerr << "Erro ao criar socket" << std::endl;
        return;
    }

    // Configurar socket para reutilização
    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Configurar endereço
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port_);

    // Bind
    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Erro no bind da porta " << port_ << std::endl;
        close(serverSocket);
        return;
    }

    // Listen
    if (listen(serverSocket, 10) < 0) {
        std::cerr << "Erro no listen" << std::endl;
        close(serverSocket);
        return;
    }

    std::cout << "HttpServer ouvindo na porta " << port_ << std::endl;

    while (running_) {
        sockaddr_in clientAddr{};
        socklen_t clientAddrLen = sizeof(clientAddr);

        // Accept (com timeout)
        int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);
        if (clientSocket < 0) {
            if (running_) {
                std::cerr << "Erro no accept" << std::endl;
            }
            continue;
        }

        // Incrementar conexões ativas
        {
            std::lock_guard<std::mutex> lock(statsMutex_);
            activeConnections_++;
        }

        // Processar requisição em uma nova thread
        std::thread(&HttpServer::handleClient, this, clientSocket).detach();
    }

    close(serverSocket);
}

void HttpServer::handleClient(int clientSocket) {
    try {
        // Receber requisição HTTP
        char buffer[8192];
        ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);

        if (bytesRead <= 0) {
            close(clientSocket);
            return;
        }

        buffer[bytesRead] = '\0';
        std::string request(buffer);

        // Parse da requisição
        std::istringstream requestStream(request);
        std::string requestLine;
        std::getline(requestStream, requestLine);

        std::istringstream lineStream(requestLine);
        std::string method, path, version;
        lineStream >> method >> path >> version;

        // Parse headers e body
        std::unordered_map<std::string, std::string> headers;
        std::string body;
        bool inBody = false;

        std::string line;
        while (std::getline(requestStream, line)) {
            if (line.empty() || line == "\r") {
                inBody = true;
                continue;
            }

            if (!inBody) {
                size_t colonPos = line.find(':');
                if (colonPos != std::string::npos) {
                    std::string headerName = line.substr(0, colonPos);
                    std::string headerValue = line.substr(colonPos + 2); // +2 para ": "
                    // Remove \r se presente
                    if (!headerValue.empty() && headerValue.back() == '\r') {
                        headerValue.pop_back();
                    }
                    headers[headerName] = headerValue;
                }
            } else {
                body += line + "\n";
            }
        }

        // Processar requisição
        HttpResponse response = processRequest(method, path, body, headers);

        // Enviar resposta
        std::ostringstream responseStream;
        responseStream << "HTTP/1.1 " << response.statusCode << " OK\r\n";
        responseStream << "Content-Type: " << response.contentType << "\r\n";
        responseStream << "Content-Length: " << response.body.length() << "\r\n";
        responseStream << "Connection: close\r\n";

        // Headers adicionais
        for (const auto& header : response.headers) {
            responseStream << header.first << ": " << header.second << "\r\n";
        }

        responseStream << "\r\n";
        responseStream << response.body;

        std::string responseStr = responseStream.str();
        send(clientSocket, responseStr.c_str(), responseStr.length(), 0);

    } catch (const std::exception& e) {
        std::cerr << "Erro no handleClient: " << e.what() << std::endl;
        {
            std::lock_guard<std::mutex> lock(statsMutex_);
            errorCount_++;
        }
    }

    // Decrementar conexões ativas
    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        activeConnections_--;
    }

    close(clientSocket);
}

HttpResponse HttpServer::processRequest(const std::string& method,
                                      const std::string& path,
                                      const std::string& body,
                                      const std::unordered_map<std::string, std::string>& headers) {

    // Incrementar contador de requisições
    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        totalRequests_++;
    }

    // Verificar autenticação se necessário
    std::string operatorId;
    std::vector<std::string> permissions;

    if (requireAuth_ && !authenticateRequest(headers, operatorId, permissions)) {
        return createErrorResponse(401, "Unauthorized");
    }

    // Verificar CORS
    auto originIt = headers.find("Origin");
    if (corsEnabled_ && originIt != headers.end()) {
        std::string corsHeaders = generateCorsHeaders(originIt->second);
        HttpResponse corsResponse(200);
        corsResponse.headers["Access-Control-Allow-Origin"] = originIt->second;
        corsResponse.headers["Access-Control-Allow-Methods"] = "GET, POST, PUT, DELETE, OPTIONS";
        corsResponse.headers["Access-Control-Allow-Headers"] = "Content-Type, Authorization";
        return corsResponse;
    }

    // Procurar rota
    std::string routeKey = method + " " + path;
    auto routeIt = routes_.find(routeKey);

    if (routeIt != routes_.end()) {
        try {
            std::string responseBody = routeIt->second(method, path, body, headers);
            return createJsonResponse(responseBody);
        } catch (const std::exception& e) {
            std::cerr << "Erro no handler da rota " << routeKey << ": " << e.what() << std::endl;
            return createErrorResponse(500, "Internal Server Error");
        }
    }

    // Rota OPTIONS para CORS
    if (method == "OPTIONS") {
        HttpResponse optionsResponse(200);
        optionsResponse.headers["Access-Control-Allow-Origin"] = "*";
        optionsResponse.headers["Access-Control-Allow-Methods"] = "GET, POST, PUT, DELETE, OPTIONS";
        optionsResponse.headers["Access-Control-Allow-Headers"] = "Content-Type, Authorization";
        return optionsResponse;
    }

    // Rota não encontrada
    return createErrorResponse(404, "Not Found");
}

bool HttpServer::authenticateRequest(const std::unordered_map<std::string, std::string>& headers,
                                   std::string& operatorId,
                                   std::vector<std::string>& permissions) {

    auto authIt = headers.find("Authorization");
    if (authIt == headers.end()) {
        return false;
    }

    std::string authHeader = authIt->second;
    if (authHeader.substr(0, 7) != "Bearer ") {
        return false;
    }

    std::string token = authHeader.substr(7);

    if (!authCallback_) {
        return false;
    }

    return authCallback_(token, operatorId, permissions);
}

HttpResponse HttpServer::createErrorResponse(int code, const std::string& message) {
    HttpResponse response(code, "application/json");
    response.body = "{\"error\": \"" + message + "\", \"code\": " + std::to_string(code) + "}";
    return response;
}

HttpResponse HttpServer::createJsonResponse(const std::string& json) {
    HttpResponse response(200, "application/json");
    response.body = json;
    return response;
}

HttpResponse HttpServer::createHtmlResponse(const std::string& html) {
    HttpResponse response(200, "text/html");
    response.body = html;
    return response;
}

std::string HttpServer::urlDecode(const std::string& str) const {
    std::string result;
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '%') {
            if (i + 2 < str.length()) {
                std::string hex = str.substr(i + 1, 2);
                char decoded = static_cast<char>(std::stoi(hex, nullptr, 16));
                result += decoded;
                i += 2;
            }
        } else if (str[i] == '+') {
            result += ' ';
        } else {
            result += str[i];
        }
    }
    return result;
}

std::string HttpServer::generateCorsHeaders(const std::string& origin) const {
    // Verificar se origin é permitido
    if (!allowedOrigins_.empty()) {
        bool allowed = false;
        for (const auto& allowedOrigin : allowedOrigins_) {
            if (allowedOrigin == "*" || allowedOrigin == origin) {
                allowed = true;
                break;
            }
        }
        if (!allowed) {
            return "";
        }
    }

    return "Access-Control-Allow-Origin: " + origin + "\r\n" +
           "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n" +
           "Access-Control-Allow-Headers: Content-Type, Authorization\r\n";
}

void HttpServer::logRequest(const std::string& method, const std::string& path, int statusCode) {
    std::cout << "[" << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())
              << "] " << method << " " << path << " -> " << statusCode << std::endl;
}

} // namespace AndroidStreamManager