#ifndef STREAM_SERVER_H
#define STREAM_SERVER_H

#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <atomic>
#include <functional>
#include <shared/protocol.h>
#include <core/device_manager.h>

namespace AndroidStreamManager {

class WebSocketSession;
class HttpServer;

// Tipos de callback para eventos do servidor
using DeviceConnectedCallback = std::function<void(const std::string& deviceId, const DeviceInfo& info)>;
using DeviceDisconnectedCallback = std::function<void(const std::string& deviceId)>;
using MessageReceivedCallback = std::function<void(const std::string& deviceId, const ControlMessage& message)>;
using StreamDataCallback = std::function<void(const std::string& deviceId, const StreamData& data)>;

// Classe principal do servidor
class StreamServer {
public:
    StreamServer();
    ~StreamServer();

    // Configuração e inicialização
    bool initialize(int port = 8443, const std::string& certPath = "", const std::string& keyPath = "");
    void shutdown();

    // Controle do servidor
    bool start();
    void stop();
    bool isRunning() const;

    // Configuração de callbacks
    void setDeviceConnectedCallback(DeviceConnectedCallback callback);
    void setDeviceDisconnectedCallback(DeviceDisconnectedCallback callback);
    void setMessageReceivedCallback(MessageReceivedCallback callback);
    void setStreamDataCallback(StreamDataCallback callback);

    // Comunicação com dispositivos
    bool sendMessage(const std::string& deviceId, const ControlMessage& message);
    bool broadcastMessage(const ControlMessage& message);
    bool disconnectDevice(const std::string& deviceId);

    // Estatísticas
    struct ServerStats {
        size_t connectedDevices;
        size_t activeStreams;
        uint64_t totalMessagesReceived;
        uint64_t totalMessagesSent;
        std::chrono::seconds uptime;
    };
    ServerStats getStats() const;

    // Configuração
    void setMaxConnections(size_t maxConn);
    void setHeartbeatTimeout(std::chrono::seconds timeout);
    void enableCors(bool enable);

private:
    // Componentes do servidor
    std::unique_ptr<HttpServer> httpServer_;
    std::unordered_map<std::string, std::shared_ptr<WebSocketSession>> activeSessions_;

    // Configurações
    int port_;
    std::string certPath_;
    std::string keyPath_;
    size_t maxConnections_;
    std::chrono::seconds heartbeatTimeout_;
    bool corsEnabled_;

    // Estado
    std::atomic<bool> running_;
    std::chrono::system_clock::time_point startTime_;

    // Callbacks
    DeviceConnectedCallback deviceConnectedCallback_;
    DeviceDisconnectedCallback deviceDisconnectedCallback_;
    MessageReceivedCallback messageReceivedCallback_;
    StreamDataCallback streamDataCallback_;

    // Sincronização
    mutable std::mutex mutex_;

    // Threads
    std::thread serverThread_;
    std::thread cleanupThread_;

    // Métodos internos
    void serverLoop();
    void cleanupLoop();
    void handleNewConnection(std::shared_ptr<WebSocketSession> session);
    void handleConnectionClosed(const std::string& deviceId);
    void handleMessage(const std::string& deviceId, const std::string& message);

    // Autenticação e autorização
    bool authenticateDevice(const std::string& token, DeviceInfo& deviceInfo);
    bool authorizeAction(const std::string& deviceId, const ControlMessage& message);

    // Utilitários
    std::string generateSessionId() const;
    void logConnectionEvent(const std::string& deviceId, const std::string& event);
};

} // namespace AndroidStreamManager

#endif // STREAM_SERVER_H