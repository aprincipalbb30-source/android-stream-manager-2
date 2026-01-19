#ifndef ALERTS_MANAGER_H
#define ALERTS_MANAGER_H

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <chrono>
#include <mutex>
#include <memory>

namespace AndroidStreamManager {

enum class AlertSeverity {
    LOW,      // Informação
    MEDIUM,   // Aviso
    HIGH,     // Crítico
    CRITICAL  // Emergência
};

enum class AlertStatus {
    ACTIVE,     // Alerta ativo
    RESOLVED,   // Alerta resolvido
    ACKNOWLEDGED // Alerta reconhecido
};

enum class AlertCondition {
    GREATER_THAN,     // Valor > threshold
    LESS_THAN,        // Valor < threshold
    EQUAL,            // Valor == threshold
    NOT_EQUAL,        // Valor != threshold
    GREATER_EQUAL,    // Valor >= threshold
    LESS_EQUAL        // Valor <= threshold
};

enum class AlertType {
    // Sistema
    CPU_USAGE_HIGH,
    MEMORY_USAGE_HIGH,
    DISK_SPACE_LOW,
    NETWORK_ERROR,

    // Dispositivos
    DEVICE_DISCONNECTED,
    DEVICE_BATTERY_LOW,
    DEVICE_HIGH_CPU,
    DEVICE_HIGH_MEMORY,

    // Streaming
    STREAM_FAILED,
    STREAM_HIGH_LATENCY,
    STREAM_LOW_BITRATE,
    STREAM_DROPPED_FRAMES,

    // Aplicação
    HIGH_ERROR_RATE,
    DATABASE_CONNECTION_FAILED,
    CACHE_MISS_RATE_HIGH,
    RESPONSE_TIME_HIGH,

    // Segurança
    UNAUTHORIZED_ACCESS,
    SUSPICIOUS_ACTIVITY,
    CERTIFICATE_EXPIRING
};

// Estrutura de configuração de alerta
struct AlertRule {
    std::string name;
    std::string description;
    AlertType type;
    AlertSeverity severity;
    AlertCondition condition;
    double threshold;
    std::chrono::seconds checkInterval;
    std::chrono::seconds cooldownPeriod; // Tempo mínimo entre alertas do mesmo tipo
    bool enabled;
    std::unordered_map<std::string, std::string> labels;

    AlertRule()
        : severity(AlertSeverity::MEDIUM)
        , condition(AlertCondition::GREATER_THAN)
        , threshold(0.0)
        , checkInterval(std::chrono::seconds(60))
        , cooldownPeriod(std::chrono::seconds(300))
        , enabled(true) {}
};

// Instância de alerta ativo
struct ActiveAlert {
    std::string alertId;
    AlertType type;
    AlertSeverity severity;
    std::string message;
    std::string source;
    double currentValue;
    double threshold;
    AlertStatus status;
    std::chrono::system_clock::time_point createdAt;
    std::chrono::system_clock::time_point lastUpdated;
    std::chrono::system_clock::time_point resolvedAt;
    std::unordered_map<std::string, std::string> labels;

    ActiveAlert()
        : severity(AlertSeverity::MEDIUM)
        , currentValue(0.0)
        , threshold(0.0)
        , status(AlertStatus::ACTIVE) {}
};

// Callbacks para notificações de alertas
using AlertTriggeredCallback = std::function<void(const ActiveAlert& alert)>;
using AlertResolvedCallback = std::function<void(const ActiveAlert& alert)>;
using AlertAcknowledgedCallback = std::function<void(const ActiveAlert& alert)>;

// Classe principal do gerenciador de alertas
class AlertsManager {
public:
    static AlertsManager& getInstance();

    // Lifecycle
    bool initialize();
    void shutdown();

    // Configuração de regras
    void addAlertRule(const AlertRule& rule);
    void removeAlertRule(const std::string& ruleName);
    void enableAlertRule(const std::string& ruleName, bool enabled);
    std::vector<AlertRule> getAlertRules() const;

    // Verificação de alertas
    void checkSystemAlerts();
    void checkDeviceAlerts();
    void checkStreamingAlerts();
    void checkApplicationAlerts();
    void checkAllAlerts();

    // Gerenciamento de alertas ativos
    std::vector<ActiveAlert> getActiveAlerts() const;
    std::vector<ActiveAlert> getResolvedAlerts(int limit = 100) const;
    bool acknowledgeAlert(const std::string& alertId);
    bool resolveAlert(const std::string& alertId);
    void clearResolvedAlerts(int olderThanDays = 7);

    // Callbacks
    void setAlertTriggeredCallback(AlertTriggeredCallback callback);
    void setAlertResolvedCallback(AlertResolvedCallback callback);
    void setAlertAcknowledgedCallback(AlertAcknowledgedCallback callback);

    // Estatísticas
    struct AlertStats {
        uint64_t totalAlertsTriggered;
        uint64_t activeAlerts;
        uint64_t resolvedAlerts;
        uint64_t acknowledgedAlerts;
        std::unordered_map<AlertSeverity, uint64_t> alertsBySeverity;
        std::unordered_map<AlertType, uint64_t> alertsByType;
    };
    AlertStats getStats() const;

    // Configuração
    void setGlobalCooldown(std::chrono::seconds cooldown);
    void setMaxActiveAlerts(size_t maxAlerts);

private:
    AlertsManager();

    // Helpers para verificação
    void checkCpuUsageAlert();
    void checkMemoryUsageAlert();
    void checkDiskSpaceAlert();
    void checkDeviceConnectivityAlert();
    void checkDeviceBatteryAlert();
    void checkStreamLatencyAlert();
    void checkErrorRateAlert();

    // Gerenciamento interno
    std::string generateAlertId() const;
    bool shouldTriggerAlert(const AlertRule& rule, double currentValue);
    bool isOnCooldown(const AlertRule& rule);
    void triggerAlert(const AlertRule& rule, double currentValue, const std::string& message,
                     const std::string& source, const std::unordered_map<std::string, std::string>& labels = {});
    void updateAlertCooldown(const AlertRule& rule);

    // Utilitários
    std::string alertTypeToString(AlertType type) const;
    std::string severityToString(AlertSeverity severity) const;
    std::string statusToString(AlertStatus status) const;

    // Estado interno
    mutable std::mutex mutex_;
    std::unordered_map<std::string, AlertRule> alertRules_;
    std::unordered_map<std::string, ActiveAlert> activeAlerts_;
    std::vector<ActiveAlert> resolvedAlerts_;
    std::unordered_map<std::string, std::chrono::system_clock::time_point> lastAlertTime_;

    // Configurações
    std::chrono::seconds globalCooldown_;
    size_t maxActiveAlerts_;

    // Callbacks
    AlertTriggeredCallback alertTriggeredCallback_;
    AlertResolvedCallback alertResolvedCallback_;
    AlertAcknowledgedCallback alertAcknowledgedCallback_;

    // Estatísticas
    mutable std::mutex statsMutex_;
    AlertStats stats_;

    // Non-copyable
    AlertsManager(const AlertsManager&) = delete;
    AlertsManager& operator=(const AlertsManager&) = delete;
};

} // namespace AndroidStreamManager

#endif // ALERTS_MANAGER_H