#ifndef PROMETHEUS_EXPORTER_H
#define PROMETHEUS_EXPORTER_H

#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <unordered_map>
#include "metrics_collector.h"

namespace AndroidStreamManager {

// Classe para exportar métricas no formato Prometheus
class PrometheusExporter {
public:
    PrometheusExporter();
    ~PrometheusExporter();

    // Configuração
    bool initialize(int port = 9090, const std::string& bindAddress = "0.0.0.0");
    void shutdown();

    // Controle
    bool start();
    void stop();
    bool isRunning() const;

    // Configuração de métricas customizadas
    void addCustomMetric(const std::string& name, const std::string& help,
                        MetricType type, const std::unordered_map<std::string, std::string>& labels = {});
    void updateCustomMetric(const std::string& name, double value);
    void removeCustomMetric(const std::string& name);

    // Endpoint de métricas
    std::string getMetricsEndpoint() const;

    // Estatísticas do exportador
    struct ExporterStats {
        uint64_t totalRequests;
        uint64_t activeConnections;
        double averageResponseTimeMs;
        std::chrono::system_clock::time_point lastRequestTime;
    };
    ExporterStats getStats() const;

private:
    // Servidor HTTP simples para métricas
    class MetricsHttpServer {
    public:
        MetricsHttpServer();
        ~MetricsHttpServer();

        bool start(int port, const std::string& bindAddress);
        void stop();
        bool isRunning() const;

        // Handlers
        std::string handleMetricsRequest();
        std::string handleHealthRequest();
        std::string handleNotFound();

    private:
        void serverLoop();
        void handleClient(int clientSocket);

        int serverSocket_;
        std::atomic<bool> running_;
        std::thread serverThread_;
        std::string bindAddress_;
        int port_;
    };

    // Estado interno
    std::unique_ptr<MetricsHttpServer> httpServer_;
    std::atomic<bool> running_;
    int port_;
    std::string bindAddress_;

    // Métricas customizadas
    mutable std::mutex customMetricsMutex_;
    std::unordered_map<std::string, std::tuple<std::string, MetricType, std::unordered_map<std::string, std::string>, double>> customMetrics_;

    // Estatísticas
    mutable std::mutex statsMutex_;
    ExporterStats stats_;

    // Helpers para formatação Prometheus
    std::string formatMetricHeader(const std::string& name, const std::string& help, MetricType type) const;
    std::string formatMetricValue(const std::string& name, double value,
                                 const std::unordered_map<std::string, std::string>& labels) const;
    std::string formatMetricLabels(const std::unordered_map<std::string, std::string>& labels) const;
};

} // namespace AndroidStreamManager

#endif // PROMETHEUS_EXPORTER_H