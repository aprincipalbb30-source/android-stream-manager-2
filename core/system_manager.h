#ifndef SYSTEM_MANAGER_H
#define SYSTEM_MANAGER_H

#include "security/tls_manager.h"
#include "security/jwt_manager.h"
#include "security/apk_signer.h"
#include "compliance/compliance_manager.h"
#include "optimization/thread_pool.h"
#include "optimization/build_cache.h"
#include "optimization/stream_optimizer.h"

namespace AndroidStreamManager {

class SystemManager {
public:
    static SystemManager& getInstance();
    
    bool initialize(const std::string& configPath);
    void shutdown();
    
    // Componentes do sistema
    ThreadPool& getThreadPool() { return threadPool; }
    BuildCache& getBuildCache() { return buildCache; }
    StreamOptimizer& getStreamOptimizer() { return streamOptimizer; }
    
    // Segurança
    bool validateSession(const std::string& token,
                        const std::string& deviceId);
    
    // Build com cache
    BuildResult buildApkWithCache(const ApkConfig& config,
                                 const std::string& operatorId);
    
    // Streaming otimizado
    bool streamData(const std::string& deviceId,
                   StreamData& data);
    
    // Estatísticas do sistema
    struct SystemStats {
        size_t activeSessions;
        size_t buildsInCache;
        size_t threadsActive;
        double cacheHitRate;
        std::chrono::seconds uptime;
    };
    
    SystemStats getStats() const;
    
private:
    SystemManager();
    ~SystemManager();
    
    // Componentes
    ThreadPool threadPool;
    BuildCache buildCache;
    StreamOptimizer streamOptimizer;
    
    // Estado do sistema
    std::atomic<bool> initialized{false};
    std::chrono::system_clock::time_point startTime;
    
    // Sessões ativas
    struct ActiveSession {
        std::string deviceId;
        std::string operatorId;
        std::chrono::system_clock::time_point startedAt;
        std::shared_ptr<SecureTlsClient> connection;
    };
    
    std::unordered_map<std::string, ActiveSession> activeSessions;
    mutable std::mutex sessionsMutex;
};

} // namespace AndroidStreamManager

#endif // SYSTEM_MANAGER_H