#ifndef WEBSOCKET_SESSION_H
#define WEBSOCKET_SESSION_H

#include <string>
#include <memory>
#include <atomic>
#include <chrono>
#include <mutex>
#include <queue>
#include <thread>
#include <shared/protocol.h>

namespace AndroidStreamManager {

// Estados da sessão WebSocket
enum class SessionState {
    CONNECTING,
    AUTHENTICATING,
    AUTHENTICATED,
    ACTIVE,
    CLOSING,
    CLOSED
};

// Classe que representa uma sessão WebSocket individual
class WebSocketSession : public std::enable_shared_from_this<WebSocketSession> {
public:
    WebSocketSession();
    ~WebSocketSession();

    // Lifecycle
    void start();
    void stop();

    // Estado
    SessionState getState() const;
    bool isActive() const;
    std::string getDeviceId() const;
    std::string getSessionId() const;
    std::chrono::system_clock::time_point getLastActivity() const;

    // Comunicação
    void sendMessage(const std::string& message);
    void sendMessage(const ControlMessage& message);
    void sendMessage(const StreamData& data);

    // Callbacks para eventos
    void setMessageCallback(std::function<void(const std::string&)> callback);
    void setCloseCallback(std::function<void()> callback);
    void setErrorCallback(std::function<void(const std::string&)> callback);

    // Configuração
    void setDeviceId(const std::string& deviceId);
    void setAuthenticated(bool authenticated);

private:
    // Estado interno
    std::string sessionId_;
    std::string deviceId_;
    SessionState state_;
    bool authenticated_;

    // Timestamps
    std::chrono::system_clock::time_point createdAt_;
    std::chrono::system_clock::time_point lastActivity_;
    std::chrono::system_clock::time_point authenticatedAt_;

    // Comunicação
    mutable std::mutex sendMutex_;
    std::queue<std::string> sendQueue_;
    std::atomic<bool> running_;

    // Threads
    std::thread sendThread_;
    std::thread receiveThread_;

    // Callbacks
    std::function<void(const std::string&)> messageCallback_;
    std::function<void()> closeCallback_;
    std::function<void(const std::string&)> errorCallback_;

    // Métodos internos
    void sendLoop();
    void receiveLoop();
    void processMessage(const std::string& message);
    void handleAuthentication(const std::string& authData);
    void updateActivity();

    // Utilitários
    std::string generateSessionId() const;
    bool validateMessage(const std::string& message) const;
    void logEvent(const std::string& event) const;
};

} // namespace AndroidStreamManager

#endif // WEBSOCKET_SESSION_H