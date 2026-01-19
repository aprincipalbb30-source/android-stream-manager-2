#include "stream_server.h"
#include "websocket_session.h"
#include "http_server.h"
#include <iostream>
#include <algorithm>
#include <random>
#include <sstream>
#include <iomanip>

namespace AndroidStreamManager {

StreamServer::StreamServer()
    : port_(8443)
    , maxConnections_(1000)
    , heartbeatTimeout_(std::chrono::seconds(60))
    , corsEnabled_(true)
    , running_(false) {
    std::cout << "StreamServer criado" << std::endl;
}

StreamServer::~StreamServer() {
    shutdown();
    std::cout << "StreamServer destruído" << std::endl;
}

bool StreamServer::initialize(int port, const std::string& certPath, const std::string& keyPath) {
    port_ = port;
    certPath_ = certPath;
    keyPath_ = keyPath;

    try {
        // Inicializar HTTP server
        httpServer_ = std::make_unique<HttpServer>();
        if (!httpServer_->initialize(port, certPath, keyPath)) {
            std::cerr << "Falha ao inicializar HTTP server" << std::endl;
            return false;
        }

        // Configurar rotas HTTP
        setupHttpRoutes();

        std::cout << "StreamServer inicializado na porta " << port_ << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "Erro ao inicializar StreamServer: " << e.what() << std::endl;
        return false;
    }
}

void StreamServer::setupHttpRoutes() {
    if (!httpServer_) {
        return;
    }

    // Rota de saúde
    httpServer_->addRoute("GET", "/api/health", [](const std::string&, const std::string&, const std::string&,
                                                 const std::unordered_map<std::string, std::string>&) {
        return R"(
        {
            "status": "ok",
            "service": "Android Stream Manager",
            "version": "1.0.0",
            "timestamp": ")" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()) + "\"}";
    });

    // Rota de estatísticas
    httpServer_->addRoute("GET", "/api/stats", [this](const std::string&, const std::string&, const std::string&,
                                                    const std::unordered_map<std::string, std::string>&) {
        auto stats = getStats();
        std::ostringstream oss;
        oss << R"(
        {
            "connectedDevices": )" << stats.connectedDevices << R"(,
            "activeStreams": )" << stats.activeStreams << R"(,
            "totalMessagesReceived": )" << stats.totalMessagesReceived << R"(,
            "totalMessagesSent": )" << stats.totalMessagesSent << R"(,
            "uptime": )" << stats.uptime.count() << R"(
        })";
        return oss.str();
    });

    // Rota para listar dispositivos
    httpServer_->addRoute("GET", "/api/devices", [this](const std::string&, const std::string&, const std::string&,
                                                      const std::unordered_map<std::string, std::string>&) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::ostringstream oss;
        oss << "[\n";
        bool first = true;
        for (const auto& pair : activeSessions_) {
            if (!first) oss << ",\n";
            const auto& session = pair.second;
            oss << "  {\n";
            oss << "    \"deviceId\": \"" << session->getDeviceId() << "\",\n";
            oss << "    \"sessionId\": \"" << session->getSessionId() << "\",\n";
            oss << "    \"active\": " << (session->isActive() ? "true" : "false") << "\n";
            oss << "  }";
            first = false;
        }
        oss << "\n]";
        return oss.str();
    });

    // Rota para controle de dispositivo
    httpServer_->addRoute("POST", "/api/devices/{deviceId}/control", [this](const std::string& method, const std::string& path,
                                                                          const std::string& body,
                                                                          const std::unordered_map<std::string, std::string>& headers) {
        // Extrair deviceId do path (simplificado)
        size_t deviceStart = path.find("/api/devices/") + 13;
        size_t deviceEnd = path.find("/control", deviceStart);
        if (deviceEnd == std::string::npos) {
            return R"({"error": "Invalid device ID"})";
        }

        std::string deviceId = path.substr(deviceStart, deviceEnd - deviceStart);

        // Parse do comando do body JSON
        ControlMessage message;
        message.deviceId = deviceId;
        message.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        if (body.find("START_STREAM") != std::string::npos) {
            message.type = ControlMessage::Type::START_STREAM;
        } else if (body.find("STOP_STREAM") != std::string::npos) {
            message.type = ControlMessage::Type::STOP_STREAM;
        } else if (body.find("TAKE_SCREENSHOT") != std::string::npos) {
            message.type = ControlMessage::Type::TAKE_SCREENSHOT;
        }

        if (sendMessage(deviceId, message)) {
            return R"({"status": "ok", "message": "Command sent"})";
        } else {
            return R"({"error": "Failed to send command"})";
        }
    });

    // Habilitar CORS
    httpServer_->enableCors(true);
}

void StreamServer::shutdown() {
    stop();

    if (httpServer_) {
        httpServer_.reset();
    }

    activeSessions_.clear();
    std::cout << "StreamServer finalizado" << std::endl;
}

bool StreamServer::start() {
    if (running_) {
        std::cout << "StreamServer já está rodando" << std::endl;
        return true;
    }

    try {
        running_ = true;
        startTime_ = std::chrono::system_clock::now();

        // Iniciar HTTP server
        if (!httpServer_->start()) {
            std::cerr << "Falha ao iniciar HTTP server" << std::endl;
            running_ = false;
            return false;
        }

        // Iniciar threads de manutenção
        serverThread_ = std::thread(&StreamServer::serverLoop, this);
        cleanupThread_ = std::thread(&StreamServer::cleanupLoop, this);

        std::cout << "StreamServer iniciado com sucesso" << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "Erro ao iniciar StreamServer: " << e.what() << std::endl;
        running_ = false;
        return false;
    }
}

void StreamServer::stop() {
    if (!running_) {
        return;
    }

    running_ = false;

    // Parar HTTP server
    if (httpServer_) {
        httpServer_->stop();
    }

    // Fechar todas as sessões WebSocket
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& pair : activeSessions_) {
            pair.second->stop();
        }
        activeSessions_.clear();
    }

    // Aguardar threads terminarem
    if (serverThread_.joinable()) {
        serverThread_.join();
    }
    if (cleanupThread_.joinable()) {
        cleanupThread_.join();
    }

    std::cout << "StreamServer parado" << std::endl;
}

bool StreamServer::isRunning() const {
    return running_;
}

void StreamServer::setDeviceConnectedCallback(DeviceConnectedCallback callback) {
    deviceConnectedCallback_ = callback;
}

void StreamServer::setDeviceDisconnectedCallback(DeviceDisconnectedCallback callback) {
    deviceDisconnectedCallback_ = callback;
}

void StreamServer::setMessageReceivedCallback(MessageReceivedCallback callback) {
    messageReceivedCallback_ = callback;
}

void StreamServer::setStreamDataCallback(StreamDataCallback callback) {
    streamDataCallback_ = callback;
}

bool StreamServer::sendMessage(const std::string& deviceId, const ControlMessage& message) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = activeSessions_.find(deviceId);
    if (it == activeSessions_.end()) {
        std::cerr << "Dispositivo não encontrado: " << deviceId << std::endl;
        return false;
    }

    it->second->sendMessage(message);
    return true;
}

bool StreamServer::broadcastMessage(const ControlMessage& message) {
    std::lock_guard<std::mutex> lock(mutex_);

    bool success = true;
    for (const auto& pair : activeSessions_) {
        if (!pair.second->sendMessage(message)) {
            success = false;
        }
    }

    return success;
}

bool StreamServer::disconnectDevice(const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = activeSessions_.find(deviceId);
    if (it == activeSessions_.end()) {
        return false;
    }

    it->second->stop();
    activeSessions_.erase(it);

    if (deviceDisconnectedCallback_) {
        deviceDisconnectedCallback_(deviceId);
    }

    return true;
}

StreamServer::ServerStats StreamServer::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);

    ServerStats stats;
    stats.connectedDevices = activeSessions_.size();

    stats.activeStreams = 0; // TODO: implementar contagem de streams ativos

    stats.totalMessagesReceived = 0; // TODO: implementar contadores
    stats.totalMessagesSent = 0;

    stats.uptime = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now() - startTime_);

    return stats;
}

void StreamServer::setMaxConnections(size_t maxConn) {
    maxConnections_ = maxConn;
}

void StreamServer::setHeartbeatTimeout(std::chrono::seconds timeout) {
    heartbeatTimeout_ = timeout;
}

void StreamServer::enableCors(bool enable) {
    corsEnabled_ = enable;
    if (httpServer_) {
        httpServer_->enableCors(enable);
    }
}

void StreamServer::serverLoop() {
    std::cout << "Server loop iniciado" << std::endl;

    while (running_) {
        // Simulação de aceite de conexões WebSocket
        // Em implementação real, seria um servidor WebSocket real

        // Verificar se há novas conexões (simulado)
        // handleNewWebSocketConnection();

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "Server loop finalizado" << std::endl;
}

void StreamServer::cleanupLoop() {
    std::cout << "Cleanup loop iniciado" << std::endl;

    while (running_) {
        {
            std::lock_guard<std::mutex> lock(mutex_);

            auto now = std::chrono::system_clock::now();

            // Verificar sessões inativas
            std::vector<std::string> inactiveSessions;
            for (const auto& pair : activeSessions_) {
                auto timeSinceActivity = now - pair.second->getLastActivity();
                if (timeSinceActivity > heartbeatTimeout_) {
                    inactiveSessions.push_back(pair.first);
                }
            }

            // Remover sessões inativas
            for (const std::string& deviceId : inactiveSessions) {
                std::cout << "Removendo sessão inativa: " << deviceId << std::endl;
                activeSessions_[deviceId]->stop();
                activeSessions_.erase(deviceId);

                if (deviceDisconnectedCallback_) {
                    deviceDisconnectedCallback_(deviceId);
                }
            }
        }

        // Executar limpeza a cada 30 segundos
        std::this_thread::sleep_for(std::chrono::seconds(30));
    }

    std::cout << "Cleanup loop finalizado" << std::endl;
}

void StreamServer::handleNewConnection(std::shared_ptr<WebSocketSession> session) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (activeSessions_.size() >= maxConnections_) {
        std::cerr << "Limite de conexões atingido" << std::endl;
        session->stop();
        return;
    }

    // Configurar callbacks da sessão
    session->setMessageCallback([this](const std::string& message) {
        handleMessage(message);
    });

    session->setCloseCallback([this, session]() {
        std::string deviceId = session->getDeviceId();
        if (!deviceId.empty()) {
            handleConnectionClosed(deviceId);
        }
    });

    session->setErrorCallback([this](const std::string& error) {
        std::cerr << "Erro na sessão WebSocket: " << error << std::endl;
    });

    // Adicionar à lista de sessões ativas
    activeSessions_[session->getSessionId()] = session;

    session->start();
}

void StreamServer::handleConnectionClosed(const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = activeSessions_.find(deviceId);
    if (it != activeSessions_.end()) {
        activeSessions_.erase(it);

        if (deviceDisconnectedCallback_) {
            deviceDisconnectedCallback_(deviceId);
        }

        logConnectionEvent(deviceId, "disconnected");
    }
}

void StreamServer::handleMessage(const std::string& message) {
    try {
        // Parse da mensagem (simplificado)
        if (message.find("\"type\": \"authenticate\"") != std::string::npos) {
            handleAuthentication(message);
        } else if (message.find("\"type\": \"stream_data\"") != std::string::npos) {
            handleStreamData(message);
        } else if (message.find("\"type\": \"control\"") != std::string::npos) {
            handleControlMessage(message);
        } else if (message.find("\"type\": \"video_frame\"") != std::string::npos) {
            handleVideoFrame(message);
        } else if (message.find("\"type\": \"app_monitoring\"") != std::string::npos) {
            handleAppMonitoringCommand(message);
        } else if (message.find("\"type\": \"screen_lock\"") != std::string::npos) {
            handleScreenLockCommand(message);
        }

        if (messageReceivedCallback_) {
            // Parse message into ControlMessage (simplified)
            ControlMessage msg;
            messageReceivedCallback_("unknown_device", msg);
        }

    } catch (const std::exception& e) {
        std::cerr << "Erro ao processar mensagem: " << e.what() << std::endl;
    }
}

void StreamServer::handleAuthentication(const std::string& authMessage) {
    // Simulação de autenticação
    DeviceInfo deviceInfo;
    deviceInfo.deviceId = "simulated_device_" + std::to_string(activeSessions_.size());

    if (authenticateDevice("simulated_token", deviceInfo)) {
        // Notificar callbacks
        if (deviceConnectedCallback_) {
            deviceConnectedCallback_(deviceInfo.deviceId, deviceInfo);
        }

        logConnectionEvent(deviceInfo.deviceId, "authenticated");
    }
}

void StreamServer::handleStreamData(const std::string& dataMessage) {
    // Simulação de processamento de dados de stream
    if (streamDataCallback_) {
        StreamData data;
        data.deviceId = "simulated_device";
        data.dataType = StreamData::DataType::VIDEO_H264;
        data.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        streamDataCallback_(data.deviceId, data);
    }
}

void StreamServer::handleVideoFrame(const std::string& frameMessage) {
    // Processar frame de vídeo H.264
    try {
        // Parse JSON (simplificado - em produção usar biblioteca JSON)
        std::string timestamp = extractJsonValue(frameMessage, "timestamp");
        std::string isKeyFrame = extractJsonValue(frameMessage, "isKeyFrame");
        std::string data = extractJsonValue(frameMessage, "data");

        if (!data.empty()) {
            // Decodificar Base64 (simplificado)
            std::vector<uint8_t> frameData = base64Decode(data);

            // Criar dados de stream
            StreamData streamData;
            streamData.deviceId = "android_device"; // TODO: Obter do contexto
            streamData.dataType = StreamData::DataType::VIDEO_H264;
            streamData.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            streamData.frameData = frameData;
            streamData.isKeyFrame = (isKeyFrame == "true");

            // Notificar callbacks
            if (streamDataCallback_) {
                streamDataCallback_(streamData.deviceId, streamData);
            }

            // Broadcast para dashboards conectados
            broadcastVideoFrame(streamData);
        }

    } catch (const std::exception& e) {
        std::cerr << "Erro ao processar frame de vídeo: " << e.what() << std::endl;
    }
}

void StreamServer::handleAppMonitoringCommand(const std::string& commandMessage) {
    // Processar comandos de monitoramento de apps
    std::cout << "Comando de monitoramento de apps: " << commandMessage << std::endl;

    try {
        std::string action = extractJsonValue(commandMessage, "action");
        std::string deviceId = "android_device"; // TODO: Obter do contexto

        if (action == "start_monitoring") {
            std::cout << "Iniciando monitoramento de apps para: " << deviceId << std::endl;
        } else if (action == "stop_monitoring") {
            std::cout << "Parando monitoramento de apps para: " << deviceId << std::endl;
        } else if (action == "get_stats") {
            std::cout << "Solicitando estatísticas de apps para: " << deviceId << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "Erro ao processar comando de monitoramento: " << e.what() << std::endl;
    }
}

void StreamServer::handleScreenLockCommand(const std::string& commandMessage) {
    // Processar comandos de bloqueio de tela
    std::cout << "Comando de bloqueio de tela: " << commandMessage << std::endl;

    try {
        std::string action = extractJsonValue(commandMessage, "action");
        std::string deviceId = "android_device"; // TODO: Obter do contexto

        if (action == "lock") {
            std::cout << "Bloqueando tela do dispositivo: " << deviceId << std::endl;
        } else if (action == "unlock") {
            std::cout << "Desbloqueando tela do dispositivo: " << deviceId << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "Erro ao processar comando de bloqueio: " << e.what() << std::endl;
    }
}

void StreamServer::broadcastVideoFrame(const StreamData& frameData) {
    // Broadcast frame para todos os dashboards conectados
    // TODO: Implementar broadcasting real via WebSocket
    std::cout << "Broadcasting video frame: " << frameData.frameData.size() << " bytes" << std::endl;
}

std::string StreamServer::extractJsonValue(const std::string& json, const std::string& key) {
    // Extração simplificada de valores JSON (em produção usar biblioteca adequada)
    std::string searchKey = "\"" + key + "\":";
    size_t keyPos = json.find(searchKey);
    if (keyPos == std::string::npos) return "";

    size_t valueStart = json.find_first_of("\"", keyPos + searchKey.length());
    if (valueStart == std::string::npos) return "";

    size_t valueEnd = json.find_first_of("\"", valueStart + 1);
    if (valueEnd == std::string::npos) return "";

    return json.substr(valueStart + 1, valueEnd - valueStart - 1);
}

std::vector<uint8_t> StreamServer::base64Decode(const std::string& input) {
    // Decodificação Base64 simplificada (em produção usar biblioteca)
    // TODO: Implementar decodificação Base64 real
    std::vector<uint8_t> result;
    result.reserve(input.size());
    for (char c : input) {
        result.push_back(static_cast<uint8_t>(c));
    }
    return result;
}

void StreamServer::handleControlMessage(const std::string& controlMessage) {
    // Processamento de mensagens de controle
    std::cout << "Mensagem de controle recebida: " << controlMessage << std::endl;
}

bool StreamServer::authenticateDevice(const std::string& token, DeviceInfo& deviceInfo) {
    // Simulação de autenticação JWT
    // Em implementação real, validaria token JWT

    if (token == "simulated_token") {
        deviceInfo.deviceModel = "Android Device";
        deviceInfo.androidVersion = "13.0";
        deviceInfo.appVersion = "1.0.0";
        deviceInfo.batteryLevel = 85;
        deviceInfo.isCharging = false;
        return true;
    }

    return false;
}

bool StreamServer::authorizeAction(const std::string& deviceId, const ControlMessage& message) {
    // Simulação de autorização
    // Em implementação real, verificaria permissões do operador

    return true;
}

void StreamServer::logConnectionEvent(const std::string& deviceId, const std::string& event) {
    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::cout << "[" << std::put_time(std::localtime(&now), "%Y-%m-%d %H:%M:%S")
              << "] Device " << deviceId << " " << event << std::endl;
}

std::string StreamServer::generateSessionId() const {
    static std::atomic<uint64_t> counter(0);
    uint64_t id = counter++;

    std::ostringstream oss;
    oss << "session_" << std::hex << id;
    return oss.str();
}

} // namespace AndroidStreamManager