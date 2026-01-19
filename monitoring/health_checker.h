#ifndef HEALTH_CHECKER_H
#define HEALTH_CHECKER_H

#include <string>
#include <vector>
#include <chrono>
#include <functional>
#include <unordered_map>
#include <memory>

namespace AndroidStreamManager {

// Status de saúde
enum class HealthStatus {
    HEALTHY,      // Tudo funcionando
    DEGRADED,     // Alguns componentes com problemas
    UNHEALTHY,    // Problemas críticos
    UNKNOWN       // Status desconhecido
};

// Resultado de uma verificação de saúde
struct HealthCheckResult {
    std::string checkName;
    HealthStatus status;
    std::string message;
    std::chrono::milliseconds duration;
    std::chrono::system_clock::time_point timestamp;
    std::unordered_map<std::string, std::string> details;

    HealthCheckResult(const std::string& name)
        : checkName(name)
        , status(HealthStatus::UNKNOWN)
        , timestamp(std::chrono::system_clock::now()) {}
};

// Callback para notificações de mudança de saúde
using HealthStatusCallback = std::function<void(HealthStatus oldStatus, HealthStatus newStatus,
                                               const std::vector<HealthCheckResult>& results)>;

// Classe principal do verificador de saúde
class HealthChecker {
public:
    static HealthChecker& getInstance();

    // Lifecycle
    bool initialize();
    void shutdown();

    // Verificações manuais
    HealthStatus performHealthCheck();
    std::vector<HealthCheckResult> getLastCheckResults() const;

    // Verificações automáticas
    void enableAutoCheck(std::chrono::seconds interval);
    void disableAutoCheck();

    // Callbacks
    void setHealthStatusCallback(HealthStatusCallback callback);

    // Configuração de checks customizados
    void addCustomHealthCheck(const std::string& name,
                             std::function<HealthCheckResult()> checkFunction);
    void removeCustomHealthCheck(const std::string& name);

    // Status atual
    HealthStatus getCurrentStatus() const;
    std::string getStatusDescription(HealthStatus status) const;

    // Estatísticas
    struct HealthStats {
        uint64_t totalChecks;
        uint64_t healthyChecks;
        uint64_t degradedChecks;
        uint64_t unhealthyChecks;
        std::chrono::system_clock::time_point lastCheckTime;
        std::chrono::milliseconds averageCheckDuration;
    };
    HealthStats getStats() const;

private:
    HealthChecker();

    // Verificações padrão
    HealthCheckResult checkSystemResources();
    HealthCheckResult checkDatabaseConnectivity();
    HealthCheckResult checkNetworkConnectivity();
    HealthCheckResult checkDeviceConnectivity();
    HealthCheckResult checkStreamingHealth();
    HealthCheckResult checkDiskSpace();
    HealthCheckResult checkMemoryUsage();

    // Helpers
    HealthStatus calculateOverallStatus(const std::vector<HealthCheckResult>& results) const;
    void updateStats(const std::vector<HealthCheckResult>& results,
                    std::chrono::milliseconds duration);
    void notifyStatusChange(HealthStatus oldStatus, HealthStatus newStatus,
                           const std::vector<HealthCheckResult>& results);

    // Thread de verificação automática
    void autoCheckLoop();
    void stopAutoCheck();

    // Estado interno
    mutable std::mutex mutex_;
    HealthStatus currentStatus_;
    std::vector<HealthCheckResult> lastResults_;

    // Configuração
    bool autoCheckEnabled_;
    std::chrono::seconds autoCheckInterval_;
    HealthStatusCallback statusCallback_;

    // Checks customizados
    std::unordered_map<std::string, std::function<HealthCheckResult()>> customChecks_;

    // Thread
    std::thread autoCheckThread_;
    std::atomic<bool> running_;

    // Estatísticas
    mutable std::mutex statsMutex_;
    HealthStats stats_;

    // Non-copyable
    HealthChecker(const HealthChecker&) = delete;
    HealthChecker& operator=(const HealthChecker&) = delete;
};

} // namespace AndroidStreamManager

#endif // HEALTH_CHECKER_H