#include "websocket_session.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <random>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

namespace AndroidStreamManager {

WebSocketSession::WebSocketSession()
    : state_(SessionState::CONNECTING)
    , authenticated_(false)
    , running_(false)
    , createdAt_(std::chrono::system_clock::now())
    , lastActivity_(createdAt_) {

    sessionId_ = generateSessionId();
    std::cout << "WebSocketSession criada: " << sessionId_ << std::endl;
}

WebSocketSession::~WebSocketSession() {
    stop();
    std::cout << "WebSocketSession destruída: " << sessionId_ << std::endl;
}

void WebSocketSession::start() {
    if (running_) {
        return;
    }

    running_ = true;
    state_ = SessionState::CONNECTING;

    // Iniciar threads de envio e recebimento
    sendThread_ = std::thread(&WebSocketSession::sendLoop, this);
    receiveThread_ = std::thread(&WebSocketSession::receiveLoop, this);

    std::cout << "WebSocketSession iniciada: " << sessionId_ << std::endl;
}

void WebSocketSession::stop() {
    if (!running_) {
        return;
    }

    running_ = false;
    state_ = SessionState::CLOSED;

    // Notificar callbacks
    if (closeCallback_) {
        closeCallback_();
    }

    // Aguardar threads terminarem
    if (sendThread_.joinable()) {
        sendThread_.join();
    }
    if (receiveThread_.joinable()) {
        receiveThread_.join();
    }

    std::cout << "WebSocketSession parada: " << sessionId_ << std::endl;
}

SessionState WebSocketSession::getState() const {
    return state_;
}

bool WebSocketSession::isActive() const {
    return running_ && state_ == SessionState::ACTIVE;
}

std::string WebSocketSession::getDeviceId() const {
    return deviceId_;
}

std::string WebSocketSession::getSessionId() const {
    return sessionId_;
}

std::chrono::system_clock::time_point WebSocketSession::getLastActivity() const {
    return lastActivity_;
}

void WebSocketSession::sendMessage(const std::string& message) {
    if (!running_) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(sendMutex_);
        sendQueue_.push(message);
    }

    updateActivity();
}

void WebSocketSession::sendMessage(const ControlMessage& message) {
    // Serializar ControlMessage para JSON
    std::ostringstream oss;
    oss << "{";
    oss << "\"type\": \"control\",";
    oss << "\"deviceId\": \"" << message.deviceId << "\",";
    oss << "\"operatorId\": \"" << message.operatorId << "\",";
    oss << "\"timestamp\": " << message.timestamp << ",";
    oss << "\"action\": \"";

    switch (message.type) {
        case ControlMessage::Type::START_STREAM: oss << "START_STREAM"; break;
        case ControlMessage::Type::PAUSE_STREAM: oss << "PAUSE_STREAM"; break;
        case ControlMessage::Type::STOP_STREAM: oss << "STOP_STREAM"; break;
        case ControlMessage::Type::RESTART: oss << "RESTART"; break;
        case ControlMessage::Type::TAKE_SCREENSHOT: oss << "TAKE_SCREENSHOT"; break;
        case ControlMessage::Type::START_AUDIO: oss << "START_AUDIO"; break;
        case ControlMessage::Type::STOP_AUDIO: oss << "STOP_AUDIO"; break;
        case ControlMessage::Type::UPDATE_SETTINGS: oss << "UPDATE_SETTINGS"; break;
    }

    oss << "\"";
    oss << "}";

    sendMessage(oss.str());
}

void WebSocketSession::sendMessage(const StreamData& data) {
    // Serializar StreamData para JSON (simplificado)
    std::ostringstream oss;
    oss << "{";
    oss << "\"type\": \"stream_data\",";
    oss << "\"deviceId\": \"" << data.deviceId << "\",";
    oss << "\"frameNumber\": " << data.frameNumber << ",";
    oss << "\"timestamp\": " << data.timestamp << ",";
    oss << "\"dataType\": \"";

    switch (data.dataType) {
        case StreamData::DataType::VIDEO_H264: oss << "VIDEO_H264"; break;
        case StreamData::DataType::VIDEO_H265: oss << "VIDEO_H265"; break;
        case StreamData::DataType::AUDIO_AAC: oss << "AUDIO_AAC"; break;
        case StreamData::DataType::AUDIO_OPUS: oss << "AUDIO_OPUS"; break;
        case StreamData::DataType::SENSOR_DATA: oss << "SENSOR_DATA"; break;
        case StreamData::DataType::DEVICE_INFO: oss << "DEVICE_INFO"; break;
    }

    oss << "\",";
    oss << "\"dataSize\": " << data.data.size();
    oss << "}";

    sendMessage(oss.str());
}

void WebSocketSession::setMessageCallback(std::function<void(const std::string&)> callback) {
    messageCallback_ = callback;
}

void WebSocketSession::setCloseCallback(std::function<void()> callback) {
    closeCallback_ = callback;
}

void WebSocketSession::setErrorCallback(std::function<void(const std::string&)> callback) {
    errorCallback_ = callback;
}

void WebSocketSession::setDeviceId(const std::string& deviceId) {
    deviceId_ = deviceId;
    updateActivity();
}

void WebSocketSession::setAuthenticated(bool authenticated) {
    authenticated_ = authenticated;
    if (authenticated) {
        state_ = SessionState::AUTHENTICATED;
        authenticatedAt_ = std::chrono::system_clock::now();
    }
    updateActivity();
}

void WebSocketSession::sendLoop() {
    std::cout << "Send thread iniciado para sessão: " << sessionId_ << std::endl;

    while (running_) {
        std::string message;

        {
            std::lock_guard<std::mutex> lock(sendMutex_);
            if (!sendQueue_.empty()) {
                message = sendQueue_.front();
                sendQueue_.pop();
            }
        }

        if (!message.empty()) {
            // Aqui seria enviado via WebSocket real
            // Por enquanto apenas simula o envio
            std::cout << "Enviando mensagem para " << deviceId_ << ": "
                      << message.substr(0, 100) << (message.length() > 100 ? "..." : "") << std::endl;

            updateActivity();
        }

        // Pequena pausa para não consumir CPU
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "Send thread finalizado para sessão: " << sessionId_ << std::endl;
}

void WebSocketSession::receiveLoop() {
    std::cout << "Receive thread iniciado para sessão: " << sessionId_ << std::endl;

    // Simulação de recebimento de mensagens
    // Em uma implementação real, isso seria um loop de leitura do socket WebSocket

    while (running_) {
        // Simular recebimento de heartbeat
        if (state_ == SessionState::AUTHENTICATED || state_ == SessionState::ACTIVE) {
            // Simular mensagem de heartbeat a cada 30 segundos
            static auto lastHeartbeat = std::chrono::system_clock::now();
            auto now = std::chrono::system_clock::now();

            if (std::chrono::duration_cast<std::chrono::seconds>(now - lastHeartbeat).count() >= 30) {
                std::string heartbeat = R"({"type": "heartbeat", "deviceId": ")" + deviceId_ + R"(", "timestamp": )" +
                                      std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                                          now.time_since_epoch()).count()) + "}";

                processMessage(heartbeat);
                lastHeartbeat = now;
            }
        }

        // Simular processamento de mensagens recebidas
        // Em implementação real, seria leitura do socket

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "Receive thread finalizado para sessão: " << sessionId_ << std::endl;
}

void WebSocketSession::processMessage(const std::string& message) {
    if (!messageCallback_) {
        return;
    }

    try {
        // Processamento básico de mensagens JSON
        if (message.find("\"type\": \"authenticate\"") != std::string::npos) {
            handleAuthentication(message);
        } else if (message.find("\"type\": \"heartbeat\"") != std::string::npos) {
            // Heartbeat - apenas atualizar atividade
            updateActivity();
        } else {
            // Outras mensagens são repassadas
            messageCallback_(message);
        }

    } catch (const std::exception& e) {
        std::cerr << "Erro ao processar mensagem na sessão " << sessionId_ << ": " << e.what() << std::endl;
        if (errorCallback_) {
            errorCallback_("Erro ao processar mensagem: " + std::string(e.what()));
        }
    }
}

void WebSocketSession::handleAuthentication(const std::string& authData) {
    // Simulação de autenticação
    // Em implementação real, validaria JWT e outras credenciais

    if (authData.find("\"deviceId\"") != std::string::npos) {
        // Extrair deviceId da mensagem (simplificado)
        size_t deviceIdStart = authData.find("\"deviceId\": \"");
        if (deviceIdStart != std::string::npos) {
            deviceIdStart += 13; // Tamanho de "\"deviceId\": \""
            size_t deviceIdEnd = authData.find("\"", deviceIdStart);
            if (deviceIdEnd != std::string::npos) {
                deviceId_ = authData.substr(deviceIdStart, deviceIdEnd - deviceIdStart);
            }
        }

        setAuthenticated(true);
        state_ = SessionState::ACTIVE;

        std::cout << "Dispositivo autenticado: " << deviceId_ << " na sessão " << sessionId_ << std::endl;
    }
}

void WebSocketSession::updateActivity() {
    lastActivity_ = std::chrono::system_clock::now();
}

std::string WebSocketSession::generateSessionId() const {
    // Gerar ID único para a sessão
    static std::atomic<uint64_t> counter(0);
    uint64_t id = counter++;

    auto now = std::chrono::system_clock::now();
    uint64_t timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    std::ostringstream oss;
    oss << "ws_" << std::hex << timestamp << "_" << id;
    return oss.str();
}

bool WebSocketSession::validateMessage(const std::string& message) const {
    // Validação básica de JSON
    if (message.empty()) {
        return false;
    }

    // Verificar se é JSON válido (simplificado)
    return message.front() == '{' && message.back() == '}';
}

void WebSocketSession::logEvent(const std::string& event) const {
    std::cout << "[" << sessionId_ << "] " << event << std::endl;
}

} // namespace AndroidStreamManager