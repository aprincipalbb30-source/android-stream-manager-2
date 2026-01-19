#include "alerts_manager.h"
#include "metrics_collector.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <random>

namespace AndroidStreamManager {

AlertsManager& AlertsManager::getInstance() {
    static AlertsManager instance;
    return instance;
}

AlertsManager::AlertsManager()
    : globalCooldown_(std::chrono::seconds(60))
    , maxActiveAlerts_(100) {
    std::cout << "AlertsManager criado" << std::endl;
}

bool AlertsManager::initialize() {
    // Configurar alertas padrão
    setupDefaultAlertRules();

    std::cout << "AlertsManager inicializado com " << alertRules_.size() << " regras de alerta" << std::endl;
    return true;
}

void AlertsManager::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    activeAlerts_.clear();
    resolvedAlerts_.clear();
    alertRules_.clear();
    lastAlertTime_.clear();

    std::cout << "AlertsManager finalizado" << std::endl;
}

void AlertsManager::setupDefaultAlertRules() {
    // Alerta de CPU alta
    AlertRule cpuRule;
    cpuRule.name = "cpu_usage_high";
    cpuRule.description = "Uso de CPU acima do limite";
    cpuRule.type = AlertType::CPU_USAGE_HIGH;
    cpuRule.severity = AlertSeverity::HIGH;
    cpuRule.condition = AlertCondition::GREATER_THAN;
    cpuRule.threshold = 90.0; // 90%
    cpuRule.checkInterval = std::chrono::seconds(30);
    cpuRule.cooldownPeriod = std::chrono::seconds(300); // 5 minutos
    addAlertRule(cpuRule);

    // Alerta de memória alta
    AlertRule memoryRule;
    memoryRule.name = "memory_usage_high";
    memoryRule.description = "Uso de memória acima do limite";
    memoryRule.type = AlertType::MEMORY_USAGE_HIGH;
    memoryRule.severity = AlertSeverity::HIGH;
    memoryRule.condition = AlertCondition::GREATER_THAN;
    memoryRule.threshold = 85.0; // 85%
    memoryRule.checkInterval = std::chrono::seconds(60);
    memoryRule.cooldownPeriod = std::chrono::seconds(600); // 10 minutos
    addAlertRule(memoryRule);

    // Alerta de espaço em disco baixo
    AlertRule diskRule;
    diskRule.name = "disk_space_low";
    diskRule.description = "Espaço em disco abaixo do limite";
    diskRule.type = AlertType::DISK_SPACE_LOW;
    diskRule.severity = AlertSeverity::MEDIUM;
    diskRule.condition = AlertCondition::GREATER_THAN;
    diskRule.threshold = 90.0; // 90% usado
    diskRule.checkInterval = std::chrono::seconds(300); // 5 minutos
    diskRule.cooldownPeriod = std::chrono::seconds(1800); // 30 minutos
    addAlertRule(diskRule);

    // Alerta de bateria baixa do dispositivo
    AlertRule batteryRule;
    batteryRule.name = "device_battery_low";
    batteryRule.description = "Bateria do dispositivo baixa";
    batteryRule.type = AlertType::DEVICE_BATTERY_LOW;
    batteryRule.severity = AlertSeverity::MEDIUM;
    batteryRule.condition = AlertCondition::LESS_THAN;
    batteryRule.threshold = 20.0; // 20%
    batteryRule.checkInterval = std::chrono::seconds(120); // 2 minutos
    batteryRule.cooldownPeriod = std::chrono::seconds(900); // 15 minutos
    addAlertRule(batteryRule);

    // Alerta de dispositivo desconectado
    AlertRule disconnectRule;
    disconnectRule.name = "device_disconnected";
    disconnectRule.description = "Dispositivo desconectado inesperadamente";
    disconnectRule.type = AlertType::DEVICE_DISCONNECTED;
    disconnectRule.severity = AlertSeverity::HIGH;
    disconnectRule.condition = AlertCondition::EQUAL;
    disconnectRule.threshold = 0.0; // Não conectado
    disconnectRule.checkInterval = std::chrono::seconds(30);
    disconnectRule.cooldownPeriod = std::chrono::seconds(60); // 1 minuto
    addAlertRule(disconnectRule);

    // Alerta de alta latência de streaming
    AlertRule latencyRule;
    latencyRule.name = "stream_high_latency";
    latencyRule.description = "Latência de streaming muito alta";
    latencyRule.type = AlertType::STREAM_HIGH_LATENCY;
    latencyRule.severity = AlertSeverity::MEDIUM;
    latencyRule.condition = AlertCondition::GREATER_THAN;
    latencyRule.threshold = 100.0; // 100ms
    latencyRule.checkInterval = std::chrono::seconds(30);
    latencyRule.cooldownPeriod = std::chrono::seconds(180); // 3 minutos
    addAlertRule(latencyRule);

    // Alerta de alta taxa de erro
    AlertRule errorRule;
    errorRule.name = "high_error_rate";
    errorRule.description = "Taxa de erro da aplicação muito alta";
    errorRule.type = AlertType::HIGH_ERROR_RATE;
    errorRule.severity = AlertSeverity::HIGH;
    errorRule.condition = AlertCondition::GREATER_THAN;
    errorRule.threshold = 5.0; // 5% de erros
    errorRule.checkInterval = std::chrono::seconds(60);
    errorRule.cooldownPeriod = std::chrono::seconds(300); // 5 minutos
    addAlertRule(errorRule);
}

void AlertsManager::addAlertRule(const AlertRule& rule) {
    std::lock_guard<std::mutex> lock(mutex_);
    alertRules_[rule.name] = rule;
    std::cout << "Regra de alerta adicionada: " << rule.name << std::endl;
}

void AlertsManager::removeAlertRule(const std::string& ruleName) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = alertRules_.find(ruleName);
    if (it != alertRules_.end()) {
        alertRules_.erase(it);
        std::cout << "Regra de alerta removida: " << ruleName << std::endl;
    }
}

void AlertsManager::enableAlertRule(const std::string& ruleName, bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = alertRules_.find(ruleName);
    if (it != alertRules_.end()) {
        it->second.enabled = enabled;
        std::cout << "Regra de alerta " << (enabled ? "habilitada" : "desabilitada") << ": " << ruleName << std::endl;
    }
}

std::vector<AlertRule> AlertsManager::getAlertRules() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<AlertRule> rules;
    rules.reserve(alertRules_.size());

    for (const auto& pair : alertRules_) {
        rules.push_back(pair.second);
    }

    return rules;
}

void AlertsManager::checkSystemAlerts() {
    checkCpuUsageAlert();
    checkMemoryUsageAlert();
    checkDiskSpaceAlert();
}

void AlertsManager::checkDeviceAlerts() {
    checkDeviceConnectivityAlert();
    checkDeviceBatteryAlert();
}

void AlertsManager::checkStreamingAlerts() {
    checkStreamLatencyAlert();
}

void AlertsManager::checkApplicationAlerts() {
    checkErrorRateAlert();
}

void AlertsManager::checkAllAlerts() {
    checkSystemAlerts();
    checkDeviceAlerts();
    checkStreamingAlerts();
    checkApplicationAlerts();
}

void AlertsManager::checkCpuUsageAlert() {
    auto ruleIt = alertRules_.find("cpu_usage_high");
    if (ruleIt == alertRules_.end() || !ruleIt->second.enabled) return;

    auto metrics = MetricsCollector::getInstance().getSystemMetrics();
    double currentValue = metrics.cpu_usage_percent;

    if (shouldTriggerAlert(ruleIt->second, currentValue)) {
        std::ostringstream oss;
        oss << "Uso de CPU alto: " << std::fixed << std::setprecision(1) << currentValue << "%";

        triggerAlert(ruleIt->second, currentValue, oss.str(), "system");
    }
}

void AlertsManager::checkMemoryUsageAlert() {
    auto ruleIt = alertRules_.find("memory_usage_high");
    if (ruleIt == alertRules_.end() || !ruleIt->second.enabled) return;

    auto metrics = MetricsCollector::getInstance().getSystemMetrics();
    double usagePercent = 0.0;

    if (metrics.memory_total_bytes > 0) {
        usagePercent = (static_cast<double>(metrics.memory_used_bytes) / metrics.memory_total_bytes) * 100.0;
    }

    if (shouldTriggerAlert(ruleIt->second, usagePercent)) {
        std::ostringstream oss;
        oss << "Uso de memória alto: " << std::fixed << std::setprecision(1) << usagePercent << "%";

        triggerAlert(ruleIt->second, usagePercent, oss.str(), "system");
    }
}

void AlertsManager::checkDiskSpaceAlert() {
    auto ruleIt = alertRules_.find("disk_space_low");
    if (ruleIt == alertRules_.end() || !ruleIt->second.enabled) return;

    auto metrics = MetricsCollector::getInstance().getSystemMetrics();
    double usagePercent = 0.0;

    if (metrics.disk_total_bytes > 0) {
        usagePercent = (static_cast<double>(metrics.disk_used_bytes) / metrics.disk_total_bytes) * 100.0;
    }

    if (shouldTriggerAlert(ruleIt->second, usagePercent)) {
        std::ostringstream oss;
        oss << "Espaço em disco baixo: " << std::fixed << std::setprecision(1) << usagePercent << "% usado";

        triggerAlert(ruleIt->second, usagePercent, oss.str(), "system");
    }
}

void AlertsManager::checkDeviceConnectivityAlert() {
    auto ruleIt = alertRules_.find("device_disconnected");
    if (ruleIt == alertRules_.end() || !ruleIt->second.enabled) return;

    auto deviceMetrics = MetricsCollector::getInstance().getDeviceMetrics();

    for (const auto& device : deviceMetrics) {
        if (!device.connected && shouldTriggerAlert(ruleIt->second, 0.0)) {
            std::ostringstream oss;
            oss << "Dispositivo desconectado: " << device.device_id;

            std::unordered_map<std::string, std::string> labels;
            labels["device_id"] = device.device_id;

            triggerAlert(ruleIt->second, 0.0, oss.str(), "device", labels);
        }
    }
}

void AlertsManager::checkDeviceBatteryAlert() {
    auto ruleIt = alertRules_.find("device_battery_low");
    if (ruleIt == alertRules_.end() || !ruleIt->second.enabled) return;

    auto deviceMetrics = MetricsCollector::getInstance().getDeviceMetrics();

    for (const auto& device : deviceMetrics) {
        if (device.connected && device.battery_level < ruleIt->second.threshold) {
            std::ostringstream oss;
            oss << "Bateria baixa no dispositivo " << device.device_id << ": " << device.battery_level << "%";

            std::unordered_map<std::string, std::string> labels;
            labels["device_id"] = device.device_id;

            triggerAlert(ruleIt->second, device.battery_level, oss.str(), "device", labels);
        }
    }
}

void AlertsManager::checkStreamLatencyAlert() {
    auto ruleIt = alertRules_.find("stream_high_latency");
    if (ruleIt == alertRules_.end() || !ruleIt->second.enabled) return;

    auto streamingMetrics = MetricsCollector::getInstance().getStreamingMetrics();
    double currentLatency = streamingMetrics.average_latency_ms;

    if (shouldTriggerAlert(ruleIt->second, currentLatency)) {
        std::ostringstream oss;
        oss << "Latência de streaming alta: " << std::fixed << std::setprecision(1) << currentLatency << "ms";

        triggerAlert(ruleIt->second, currentLatency, oss.str(), "streaming");
    }
}

void AlertsManager::checkErrorRateAlert() {
    auto ruleIt = alertRules_.find("high_error_rate");
    if (ruleIt == alertRules_.end() || !ruleIt->second.enabled) return;

    auto appMetrics = MetricsCollector::getInstance().getApplicationMetrics();
    double errorRate = 0.0;

    if (appMetrics.total_requests > 0) {
        errorRate = (static_cast<double>(appMetrics.error_count) / appMetrics.total_requests) * 100.0;
    }

    if (shouldTriggerAlert(ruleIt->second, errorRate)) {
        std::ostringstream oss;
        oss << "Taxa de erro alta: " << std::fixed << std::setprecision(1) << errorRate << "%";

        triggerAlert(ruleIt->second, errorRate, oss.str(), "application");
    }
}

std::vector<ActiveAlert> AlertsManager::getActiveAlerts() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ActiveAlert> alerts;

    for (const auto& pair : activeAlerts_) {
        alerts.push_back(pair.second);
    }

    return alerts;
}

std::vector<ActiveAlert> AlertsManager::getResolvedAlerts(int limit) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ActiveAlert> alerts = resolvedAlerts_;

    // Ordenar por data de resolução (mais recentes primeiro)
    std::sort(alerts.begin(), alerts.end(),
              [](const ActiveAlert& a, const ActiveAlert& b) {
                  return a.resolvedAt > b.resolvedAt;
              });

    if (static_cast<int>(alerts.size()) > limit) {
        alerts.resize(limit);
    }

    return alerts;
}

bool AlertsManager::acknowledgeAlert(const std::string& alertId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = activeAlerts_.find(alertId);
    if (it == activeAlerts_.end()) {
        return false;
    }

    it->second.status = AlertStatus::ACKNOWLEDGED;
    it->second.lastUpdated = std::chrono::system_clock::now();

    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        stats_.acknowledgedAlerts++;
    }

    if (alertAcknowledgedCallback_) {
        alertAcknowledgedCallback_(it->second);
    }

    std::cout << "Alerta reconhecido: " << alertId << std::endl;
    return true;
}

bool AlertsManager::resolveAlert(const std::string& alertId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = activeAlerts_.find(alertId);
    if (it == activeAlerts_.end()) {
        return false;
    }

    it->second.status = AlertStatus::RESOLVED;
    it->second.resolvedAt = std::chrono::system_clock::now();
    it->second.lastUpdated = it->second.resolvedAt;

    // Mover para resolved alerts
    resolvedAlerts_.push_back(it->second);
    activeAlerts_.erase(it);

    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        stats_.activeAlerts--;
        stats_.resolvedAlerts++;
    }

    if (alertResolvedCallback_) {
        alertResolvedCallback_(it->second);
    }

    std::cout << "Alerta resolvido: " << alertId << std::endl;
    return true;
}

void AlertsManager::clearResolvedAlerts(int olderThanDays) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto cutoff = std::chrono::system_clock::now() -
                  std::chrono::hours(24 * olderThanDays);

    resolvedAlerts_.erase(
        std::remove_if(resolvedAlerts_.begin(), resolvedAlerts_.end(),
                      [cutoff](const ActiveAlert& alert) {
                          return alert.resolvedAt < cutoff;
                      }),
        resolvedAlerts_.end()
    );

    std::cout << "Alertas resolvidos antigos removidos (>" << olderThanDays << " dias)" << std::endl;
}

void AlertsManager::setAlertTriggeredCallback(AlertTriggeredCallback callback) {
    alertTriggeredCallback_ = callback;
}

void AlertsManager::setAlertResolvedCallback(AlertResolvedCallback callback) {
    alertResolvedCallback_ = callback;
}

void AlertsManager::setAlertAcknowledgedCallback(AlertAcknowledgedCallback callback) {
    alertAcknowledgedCallback_ = callback;
}

AlertsManager::AlertStats AlertsManager::getStats() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    return stats_;
}

void AlertsManager::setGlobalCooldown(std::chrono::seconds cooldown) {
    globalCooldown_ = cooldown;
}

void AlertsManager::setMaxActiveAlerts(size_t maxAlerts) {
    maxActiveAlerts_ = maxAlerts;
}

bool AlertsManager::shouldTriggerAlert(const AlertRule& rule, double currentValue) {
    if (!rule.enabled) return false;

    // Verificar cooldown
    if (isOnCooldown(rule)) return false;

    // Verificar condição
    bool conditionMet = false;
    switch (rule.condition) {
        case AlertCondition::GREATER_THAN:
            conditionMet = currentValue > rule.threshold;
            break;
        case AlertCondition::LESS_THAN:
            conditionMet = currentValue < rule.threshold;
            break;
        case AlertCondition::EQUAL:
            conditionMet = currentValue == rule.threshold;
            break;
        case AlertCondition::NOT_EQUAL:
            conditionMet = currentValue != rule.threshold;
            break;
        case AlertCondition::GREATER_EQUAL:
            conditionMet = currentValue >= rule.threshold;
            break;
        case AlertCondition::LESS_EQUAL:
            conditionMet = currentValue <= rule.threshold;
            break;
    }

    return conditionMet;
}

bool AlertsManager::isOnCooldown(const AlertRule& rule) {
    auto it = lastAlertTime_.find(rule.name);
    if (it == lastAlertTime_.end()) {
        return false;
    }

    auto timeSinceLastAlert = std::chrono::system_clock::now() - it->second;
    return timeSinceLastAlert < rule.cooldownPeriod;
}

void AlertsManager::triggerAlert(const AlertRule& rule, double currentValue,
                                const std::string& message, const std::string& source,
                                const std::unordered_map<std::string, std::string>& labels) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Verificar limite de alertas ativos
    if (activeAlerts_.size() >= maxActiveAlerts_) {
        std::cout << "Limite de alertas ativos atingido, pulando alerta: " << rule.name << std::endl;
        return;
    }

    // Criar alerta ativo
    ActiveAlert alert;
    alert.alertId = generateAlertId();
    alert.type = rule.type;
    alert.severity = rule.severity;
    alert.message = message;
    alert.source = source;
    alert.currentValue = currentValue;
    alert.threshold = rule.threshold;
    alert.status = AlertStatus::ACTIVE;
    alert.createdAt = std::chrono::system_clock::now();
    alert.lastUpdated = alert.createdAt;
    alert.labels = labels;

    // Adicionar à lista de alertas ativos
    activeAlerts_[alert.alertId] = alert;

    // Atualizar cooldown
    updateAlertCooldown(rule);

    // Atualizar estatísticas
    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        stats_.totalAlertsTriggered++;
        stats_.activeAlerts++;
        stats_.alertsBySeverity[alert.severity]++;
        stats_.alertsByType[alert.type]++;
    }

    // Chamar callback
    if (alertTriggeredCallback_) {
        alertTriggeredCallback_(alert);
    }

    std::cout << "Alerta disparado: [" << severityToString(alert.severity) << "] "
              << alert.message << " (ID: " << alert.alertId << ")" << std::endl;
}

void AlertsManager::updateAlertCooldown(const AlertRule& rule) {
    lastAlertTime_[rule.name] = std::chrono::system_clock::now();
}

std::string AlertsManager::generateAlertId() const {
    static std::atomic<uint64_t> counter(0);
    uint64_t id = counter++;

    std::ostringstream oss;
    oss << "alert_" << std::hex << id;
    return oss.str();
}

std::string AlertsManager::alertTypeToString(AlertType type) const {
    switch (type) {
        case AlertType::CPU_USAGE_HIGH: return "CPU_USAGE_HIGH";
        case AlertType::MEMORY_USAGE_HIGH: return "MEMORY_USAGE_HIGH";
        case AlertType::DISK_SPACE_LOW: return "DISK_SPACE_LOW";
        case AlertType::DEVICE_DISCONNECTED: return "DEVICE_DISCONNECTED";
        case AlertType::DEVICE_BATTERY_LOW: return "DEVICE_BATTERY_LOW";
        case AlertType::STREAM_HIGH_LATENCY: return "STREAM_HIGH_LATENCY";
        case AlertType::HIGH_ERROR_RATE: return "HIGH_ERROR_RATE";
        default: return "UNKNOWN";
    }
}

std::string AlertsManager::severityToString(AlertSeverity severity) const {
    switch (severity) {
        case AlertSeverity::LOW: return "LOW";
        case AlertSeverity::MEDIUM: return "MEDIUM";
        case AlertSeverity::HIGH: return "HIGH";
        case AlertSeverity::CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

std::string AlertsManager::statusToString(AlertStatus status) const {
    switch (status) {
        case AlertStatus::ACTIVE: return "ACTIVE";
        case AlertStatus::RESOLVED: return "RESOLVED";
        case AlertStatus::ACKNOWLEDGED: return "ACKNOWLEDGED";
        default: return "UNKNOWN";
    }
}

} // namespace AndroidStreamManager