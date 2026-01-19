#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <string>
#include <memory>
#include <unordered_map>
#include <functional>
#include <thread>
#include <atomic>
#include <shared/protocol.h>

namespace AndroidStreamManager {

// Tipos para handlers HTTP
using HttpRequestHandler = std::function<std::string(const std::string& method,
                                                    const std::string& path,
                                                    const std::string& body,
                                                    const std::unordered_map<std::string, std::string>& headers)>;

using HttpAuthCallback = std::function<bool(const std::string& token,
                                           std::string& operatorId,
                                           std::vector<std::string>& permissions)>;

// Estrutura para resposta HTTP
struct HttpResponse {
    int statusCode;
    std::string contentType;
    std::string body;
    std::unordered_map<std::string, std::string> headers;

    HttpResponse(int code = 200, const std::string& type = "application/json")
        : statusCode(code), contentType(type) {}
};

// Classe do servidor HTTP/HTTPS
class HttpServer {
public:
    HttpServer();
    ~HttpServer();

    // Configuração
    bool initialize(int port = 8443,
                   const std::string& certPath = "",
                   const std::string& keyPath = "");

    // Lifecycle
    bool start();
    void stop();
    bool isRunning() const;

    // Registro de rotas
    void addRoute(const std::string& method,
                 const std::string& path,
                 HttpRequestHandler handler);

    void addRoute(const std::string& method,
                 const std::string& path,
                 const std::string& response);

    // Autenticação
    void setAuthCallback(HttpAuthCallback callback);
    void setRequireAuth(bool require);

    // Middleware
    void addMiddleware(std::function<bool(const std::string& path,
                                        const std::unordered_map<std::string, std::string>& headers)> middleware);

    // Configuração CORS
    void enableCors(bool enable);
    void addAllowedOrigin(const std::string& origin);

    // Estatísticas
    struct HttpStats {
        uint64_t totalRequests;
        uint64_t activeConnections;
        uint64_t errorCount;
        std::chrono::seconds uptime;
    };
    HttpStats getStats() const;

private:
    // Configurações
    int port_;
    std::string certPath_;
    std::string keyPath_;
    bool httpsEnabled_;
    bool requireAuth_;
    bool corsEnabled_;

    // Estado
    std::atomic<bool> running_;
    std::chrono::system_clock::time_point startTime_;

    // Rotas
    std::unordered_map<std::string, HttpRequestHandler> routes_;
    std::vector<std::string> allowedOrigins_;

    // Autenticação
    HttpAuthCallback authCallback_;

    // Middleware
    std::vector<std::function<bool(const std::string&, const std::unordered_map<std::string, std::string>&)>> middlewares_;

    // Threads
    std::thread serverThread_;

    // Estatísticas
    mutable std::mutex statsMutex_;
    uint64_t totalRequests_;
    uint64_t activeConnections_;
    uint64_t errorCount_;

    // Métodos internos
    void serverLoop();
    HttpResponse processRequest(const std::string& method,
                              const std::string& path,
                              const std::string& body,
                              const std::unordered_map<std::string, std::string>& headers);

    bool authenticateRequest(const std::unordered_map<std::string, std::string>& headers,
                           std::string& operatorId,
                           std::vector<std::string>& permissions);

    HttpResponse createErrorResponse(int code, const std::string& message);
    HttpResponse createJsonResponse(const std::string& json);
    HttpResponse createHtmlResponse(const std::string& html);

    // Utilitários
    std::string urlDecode(const std::string& str) const;
    std::unordered_map<std::string, std::string> parseQueryString(const std::string& query) const;
    std::unordered_map<std::string, std::string> parseHeaders(const std::string& headerStr) const;
    std::string generateCorsHeaders(const std::string& origin) const;

    void logRequest(const std::string& method, const std::string& path, int statusCode);
};

} // namespace AndroidStreamManager

#endif // HTTP_SERVER_H