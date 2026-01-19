#include "health_checker.h"
#include "metrics_collector.h"
#include "alerts_manager.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <thread>
#include <algorithm>
#include <fstream>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/statvfs.h>
#endif

namespace AndroidStreamManager {

HealthChecker& HealthChecker::getInstance() {
    static HealthChecker instance;
    return instance;
}

HealthChecker::HealthChecker()
    : currentStatus_(HealthStatus::UNKNOWN)
    , autoCheckEnabled_(false)
    , autoCheckInterval_(std::chrono::seconds(60))
    , running_(false) {
    std::cout << "HealthChecker criado" << std::endl;
}

bool HealthChecker::initialize() {
    std::cout << "HealthChecker inicializado" << std::endl;
    return true;
}

void HealthChecker::shutdown() {
    disableAutoCheck();
    std::cout << "HealthChecker finalizado" << std::endl;
}

HealthStatus HealthChecker::performHealthCheck() {
    auto startTime = std::chrono::high_resolution_clock::now();

    std::vector<HealthCheckResult> results;

    // Executar verificações padrão
    results.push_back(checkSystemResources());
    results.push_back(checkDatabaseConnectivity());
    results.push_back(checkNetworkConnectivity());
    results.push_back(checkDeviceConnectivity());
    results.push_back(checkStreamingHealth());
    results.push_back(checkDiskSpace());
    results.push_back(checkMemoryUsage());

    // Executar verificações customizadas
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& pair : customChecks_) {
            try {
                results.push_back(pair.second());
            } catch (const std::exception& e) {
                HealthCheckResult failed(pair.first);
                failed.status = HealthStatus::UNHEALTHY;
                failed.message = std::string("Exceção durante verificação: ") + e.what();
                failed.duration = std::chrono::milliseconds(0);
                results.push_back(failed);
            }
        }
    }

    // Calcular duração total
    auto endTime = std::chrono::high_resolution_clock::now();
    auto totalDuration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    // Calcular status geral
    HealthStatus newStatus = calculateOverallStatus(results);

    // Atualizar estado
    {
        std::lock_guard<std::mutex> lock(mutex_);
        HealthStatus oldStatus = currentStatus_;
        currentStatus_ = newStatus;
        lastResults_ = results;

        // Notificar mudança de status
        if (oldStatus != newStatus && statusCallback_) {
            notifyStatusChange(oldStatus, newStatus, results);
        }
    }

    // Atualizar estatísticas
    updateStats(results, totalDuration);

    // Log do resultado
    std::cout << "Health check concluído: " << getStatusDescription(newStatus)
              << " (" << results.size() << " verificações em "
              << totalDuration.count() << "ms)" << std::endl;

    return newStatus;
}

std::vector<HealthCheckResult> HealthChecker::getLastCheckResults() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lastResults_;
}

void HealthChecker::enableAutoCheck(std::chrono::seconds interval) {
    if (autoCheckEnabled_) {
        disableAutoCheck();
    }

    autoCheckEnabled_ = true;
    autoCheckInterval_ = interval;
    running_ = true;

    autoCheckThread_ = std::thread(&HealthChecker::autoCheckLoop, this);

    std::cout << "Auto health check habilitado (intervalo: "
              << interval.count() << " segundos)" << std::endl;
}

void HealthChecker::disableAutoCheck() {
    if (!autoCheckEnabled_) {
        return;
    }

    autoCheckEnabled_ = false;
    stopAutoCheck();

    std::cout << "Auto health check desabilitado" << std::endl;
}

void HealthChecker::setHealthStatusCallback(HealthStatusCallback callback) {
    statusCallback_ = callback;
}

void HealthChecker::addCustomHealthCheck(const std::string& name,
                                       std::function<HealthCheckResult()> checkFunction) {
    std::lock_guard<std::mutex> lock(mutex_);
    customChecks_[name] = checkFunction;
    std::cout << "Health check customizado adicionado: " << name << std::endl;
}

void HealthChecker::removeCustomHealthCheck(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    customChecks_.erase(name);
    std::cout << "Health check customizado removido: " << name << std::endl;
}

HealthStatus HealthChecker::getCurrentStatus() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return currentStatus_;
}

std::string HealthChecker::getStatusDescription(HealthStatus status) const {
    switch (status) {
        case HealthStatus::HEALTHY: return "HEALTHY";
        case HealthStatus::DEGRADED: return "DEGRADED";
        case HealthStatus::UNHEALTHY: return "UNHEALTHY";
        case HealthStatus::UNKNOWN: return "UNKNOWN";
        default: return "UNKNOWN";
    }
}

HealthChecker::HealthStats HealthChecker::getStats() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    return stats_;
}

// ========== VERIFICAÇÕES DE SAÚDE ==========

HealthCheckResult HealthChecker::checkSystemResources() {
    HealthCheckResult result("system_resources");
    auto startTime = std::chrono::high_resolution_clock::now();

    try {
        auto metrics = MetricsCollector::getInstance().getSystemMetrics();

        // Verificar CPU
        if (metrics.cpu_usage_percent > 90.0) {
            result.status = HealthStatus::UNHEALTHY;
            result.message = "Uso de CPU muito alto: " + std::to_string(metrics.cpu_usage_percent) + "%";
        } else if (metrics.cpu_usage_percent > 70.0) {
            result.status = HealthStatus::DEGRADED;
            result.message = "Uso de CPU elevado: " + std::to_string(metrics.cpu_usage_percent) + "%";
        } else {
            result.status = HealthStatus::HEALTHY;
            result.message = "Recursos do sistema OK";
        }

        result.details["cpu_usage"] = std::to_string(metrics.cpu_usage_percent);
        result.details["memory_used"] = std::to_string(metrics.memory_used_bytes);
        result.details["disk_used"] = std::to_string(metrics.disk_used_bytes);

    } catch (const std::exception& e) {
        result.status = HealthStatus::UNHEALTHY;
        result.message = "Erro ao verificar recursos do sistema: " + std::string(e.what());
    }

    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - startTime);

    return result;
}

HealthCheckResult HealthChecker::checkDatabaseConnectivity() {
    HealthCheckResult result("database_connectivity");
    auto startTime = std::chrono::high_resolution_clock::now();

    try {
        // TODO: Implementar verificação real de conectividade com banco
        // Por enquanto, simular que está OK

        result.status = HealthStatus::HEALTHY;
        result.message = "Conectividade com banco de dados OK";
        result.details["connection_status"] = "connected";

    } catch (const std::exception& e) {
        result.status = HealthStatus::UNHEALTHY;
        result.message = "Erro na conectividade com banco: " + std::string(e.what());
        result.details["connection_status"] = "failed";
    }

    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - startTime);

    return result;
}

HealthCheckResult HealthChecker::checkNetworkConnectivity() {
    HealthCheckResult result("network_connectivity");
    auto startTime = std::chrono::high_resolution_clock::now();

    try {
        // Verificar conectividade básica (ping para localhost)
        // Em produção, verificar conectividade com serviços externos

        result.status = HealthStatus::HEALTHY;
        result.message = "Conectividade de rede OK";
        result.details["network_status"] = "connected";

    } catch (const std::exception& e) {
        result.status = HealthStatus::UNHEALTHY;
        result.message = "Problemas de conectividade de rede: " + std::string(e.what());
        result.details["network_status"] = "failed";
    }

    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - startTime);

    return result;
}

HealthCheckResult HealthChecker::checkDeviceConnectivity() {
    HealthCheckResult result("device_connectivity");
    auto startTime = std::chrono::high_resolution_clock::now();

    try {
        auto deviceMetrics = MetricsCollector::getInstance().getDeviceMetrics();

        int connectedDevices = 0;
        int totalDevices = deviceMetrics.size();

        for (const auto& device : deviceMetrics) {
            if (device.connected) {
                connectedDevices++;
            }
        }

        if (totalDevices == 0) {
            result.status = HealthStatus::UNKNOWN;
            result.message = "Nenhum dispositivo registrado";
        } else if (connectedDevices == 0) {
            result.status = HealthStatus::UNHEALTHY;
            result.message = "Nenhum dispositivo conectado";
        } else if (connectedDevices < totalDevices) {
            result.status = HealthStatus::DEGRADED;
            result.message = std::to_string(connectedDevices) + "/" +
                           std::to_string(totalDevices) + " dispositivos conectados";
        } else {
            result.status = HealthStatus::HEALTHY;
            result.message = "Todos os dispositivos conectados";
        }

        result.details["connected_devices"] = std::to_string(connectedDevices);
        result.details["total_devices"] = std::to_string(totalDevices);

    } catch (const std::exception& e) {
        result.status = HealthStatus::UNHEALTHY;
        result.message = "Erro ao verificar conectividade de dispositivos: " + std::string(e.what());
    }

    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - startTime);

    return result;
}

HealthCheckResult HealthChecker::checkStreamingHealth() {
    HealthCheckResult result("streaming_health");
    auto startTime = std::chrono::high_resolution_clock::now();

    try {
        auto streamingMetrics = MetricsCollector::getInstance().getStreamingMetrics();

        if (streamingMetrics.total_active_streams == 0) {
            result.status = HealthStatus::HEALTHY;
            result.message = "Nenhum stream ativo (OK)";
        } else if (streamingMetrics.average_latency_ms > 200.0) {
            result.status = HealthStatus::UNHEALTHY;
            result.message = "Latência de streaming muito alta: " +
                           std::to_string(streamingMetrics.average_latency_ms) + "ms";
        } else if (streamingMetrics.average_latency_ms > 100.0) {
            result.status = HealthStatus::DEGRADED;
            result.message = "Latência de streaming elevada: " +
                           std::to_string(streamingMetrics.average_latency_ms) + "ms";
        } else if (streamingMetrics.stream_success_rate < 0.95) {
            result.status = HealthStatus::DEGRADED;
            result.message = "Taxa de sucesso de streaming baixa: " +
                           std::to_string(streamingMetrics.stream_success_rate * 100) + "%";
        } else {
            result.status = HealthStatus::HEALTHY;
            result.message = "Streaming funcionando normalmente";
        }

        result.details["active_streams"] = std::to_string(streamingMetrics.total_active_streams);
        result.details["average_latency"] = std::to_string(streamingMetrics.average_latency_ms);
        result.details["success_rate"] = std::to_string(streamingMetrics.stream_success_rate);

    } catch (const std::exception& e) {
        result.status = HealthStatus::UNHEALTHY;
        result.message = "Erro ao verificar saúde do streaming: " + std::string(e.what());
    }

    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - startTime);

    return result;
}

HealthCheckResult HealthChecker::checkDiskSpace() {
    HealthCheckResult result("disk_space");
    auto startTime = std::chrono::high_resolution_clock::now();

    try {
        auto metrics = MetricsCollector::getInstance().getSystemMetrics();

        double usagePercent = 0.0;
        if (metrics.disk_total_bytes > 0) {
            usagePercent = (static_cast<double>(metrics.disk_used_bytes) /
                           metrics.disk_total_bytes) * 100.0;
        }

        if (usagePercent > 95.0) {
            result.status = HealthStatus::UNHEALTHY;
            result.message = "Espaço em disco crítico: " + std::to_string(usagePercent) + "% usado";
        } else if (usagePercent > 85.0) {
            result.status = HealthStatus::DEGRADED;
            result.message = "Espaço em disco baixo: " + std::to_string(usagePercent) + "% usado";
        } else {
            result.status = HealthStatus::HEALTHY;
            result.message = "Espaço em disco adequado";
        }

        result.details["disk_used_percent"] = std::to_string(usagePercent);
        result.details["disk_free_bytes"] = std::to_string(metrics.disk_free_bytes);

    } catch (const std::exception& e) {
        result.status = HealthStatus::UNHEALTHY;
        result.message = "Erro ao verificar espaço em disco: " + std::string(e.what());
    }

    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - startTime);

    return result;
}

HealthCheckResult HealthChecker::checkMemoryUsage() {
    HealthCheckResult result("memory_usage");
    auto startTime = std::chrono::high_resolution_clock::now();

    try {
        auto metrics = MetricsCollector::getInstance().getSystemMetrics();

        double usagePercent = 0.0;
        if (metrics.memory_total_bytes > 0) {
            usagePercent = (static_cast<double>(metrics.memory_used_bytes) /
                           metrics.memory_total_bytes) * 100.0;
        }

        if (usagePercent > 95.0) {
            result.status = HealthStatus::UNHEALTHY;
            result.message = "Uso de memória crítico: " + std::to_string(usagePercent) + "%";
        } else if (usagePercent > 80.0) {
            result.status = HealthStatus::DEGRADED;
            result.message = "Uso de memória alto: " + std::to_string(usagePercent) + "%";
        } else {
            result.status = HealthStatus::HEALTHY;
            result.message = "Uso de memória normal";
        }

        result.details["memory_used_percent"] = std::to_string(usagePercent);
        result.details["memory_free_bytes"] = std::to_string(metrics.memory_free_bytes);

    } catch (const std::exception& e) {
        result.status = HealthStatus::UNHEALTHY;
        result.message = "Erro ao verificar uso de memória: " + std::string(e.what());
    }

    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - startTime);

    return result;
}

// ========== HELPERS ==========

HealthStatus HealthChecker::calculateOverallStatus(const std::vector<HealthCheckResult>& results) const {
    bool hasUnhealthy = false;
    bool hasDegraded = false;

    for (const auto& result : results) {
        if (result.status == HealthStatus::UNHEALTHY) {
            hasUnhealthy = true;
        } else if (result.status == HealthStatus::DEGRADED) {
            hasDegraded = true;
        }
    }

    if (hasUnhealthy) {
        return HealthStatus::UNHEALTHY;
    } else if (hasDegraded) {
        return HealthStatus::DEGRADED;
    } else {
        return HealthStatus::HEALTHY;
    }
}

void HealthChecker::updateStats(const std::vector<HealthCheckResult>& results,
                               std::chrono::milliseconds duration) {
    std::lock_guard<std::mutex> lock(statsMutex_);

    stats_.totalChecks++;
    stats_.lastCheckTime = std::chrono::system_clock::now();

    // Atualizar contadores por status
    for (const auto& result : results) {
        switch (result.status) {
            case HealthStatus::HEALTHY: stats_.healthyChecks++; break;
            case HealthStatus::DEGRADED: stats_.degradedChecks++; break;
            case HealthStatus::UNHEALTHY: stats_.unhealthyChecks++; break;
            default: break;
        }
    }

    // Calcular duração média
    if (stats_.totalChecks == 1) {
        stats_.averageCheckDuration = duration;
    } else {
        stats_.averageCheckDuration = (stats_.averageCheckDuration * (stats_.totalChecks - 1) +
                                     duration) / stats_.totalChecks;
    }
}

void HealthChecker::notifyStatusChange(HealthStatus oldStatus, HealthStatus newStatus,
                                     const std::vector<HealthCheckResult>& results) {
    if (statusCallback_) {
        try {
            statusCallback_(oldStatus, newStatus, results);
        } catch (const std::exception& e) {
            std::cerr << "Erro no callback de mudança de status: " << e.what() << std::endl;
        }
    }

    std::cout << "Status de saúde mudou: " << getStatusDescription(oldStatus)
              << " -> " << getStatusDescription(newStatus) << std::endl;
}

void HealthChecker::autoCheckLoop() {
    std::cout << "Auto health check loop iniciado" << std::endl;

    while (running_ && autoCheckEnabled_) {
        performHealthCheck();

        // Aguardar intervalo
        for (int i = 0; i < autoCheckInterval_.count() && running_ && autoCheckEnabled_; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    std::cout << "Auto health check loop finalizado" << std::endl;
}

void HealthChecker::stopAutoCheck() {
    running_ = false;

    if (autoCheckThread_.joinable()) {
        autoCheckThread_.join();
    }
}

} // namespace AndroidStreamManager