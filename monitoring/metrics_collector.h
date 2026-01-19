#ifndef METRICS_COLLECTOR_H
#define METRICS_COLLECTOR_H

#include <string>
#include <unordered_map>
#include <vector>
#include <atomic>
#include <chrono>
#include <mutex>
#include <memory>

namespace AndroidStreamManager {

enum class MetricType {
    COUNTER,     // Valor que só aumenta (ex: requests_total)
    GAUGE,       // Valor que pode aumentar/diminuir (ex: active_connections)
    HISTOGRAM,   // Distribuição de valores (ex: request_duration)
    SUMMARY      // Estatísticas resumidas
};

// Estrutura base para uma métrica
struct MetricValue {
    std::string name;
    std::string help;
    MetricType type;
    std::unordered_map<std::string, std::string> labels;
    double value;
    std::chrono::system_clock::time_point timestamp;

    MetricValue(const std::string& n, const std::string& h, MetricType t)
        : name(n), help(h), type(t), value(0.0), timestamp(std::chrono::system_clock::now()) {}
};

// Métricas específicas do sistema
struct SystemMetrics {
    // CPU
    double cpu_usage_percent;
    double cpu_temperature_celsius;

    // Memória
    uint64_t memory_total_bytes;
    uint64_t memory_used_bytes;
    uint64_t memory_free_bytes;

    // Disco
    uint64_t disk_total_bytes;
    uint64_t disk_used_bytes;
    uint64_t disk_free_bytes;

    // Rede
    uint64_t network_bytes_sent;
    uint64_t network_bytes_received;
    uint64_t network_packets_sent;
    uint64_t network_packets_received;

    // Sistema
    std::chrono::seconds uptime_seconds;
    double load_average_1m;
    double load_average_5m;
    double load_average_15m;

    SystemMetrics()
        : cpu_usage_percent(0.0), cpu_temperature_celsius(0.0),
          memory_total_bytes(0), memory_used_bytes(0), memory_free_bytes(0),
          disk_total_bytes(0), disk_used_bytes(0), disk_free_bytes(0),
          network_bytes_sent(0), network_bytes_received(0),
          network_packets_sent(0), network_packets_received(0),
          uptime_seconds(0), load_average_1m(0.0), load_average_5m(0.0), load_average_15m(0.0) {}
};

struct DeviceMetrics {
    std::string device_id;
    bool connected;
    int battery_level;
    bool is_charging;
    double cpu_usage_percent;
    uint64_t memory_used_bytes;
    uint64_t network_sent_bytes;
    uint64_t network_received_bytes;
    std::chrono::seconds connection_duration;
    int active_streams;

    DeviceMetrics()
        : connected(false), battery_level(0), is_charging(false),
          cpu_usage_percent(0.0), memory_used_bytes(0), network_sent_bytes(0),
          network_received_bytes(0), connection_duration(0), active_streams(0) {}
};

struct StreamingMetrics {
    int total_active_streams;
    int video_streams;
    int audio_streams;
    double average_bitrate_mbps;
    double average_latency_ms;
    uint64_t total_bytes_streamed;
    int dropped_frames_total;
    double stream_success_rate; // 0.0 - 1.0

    StreamingMetrics()
        : total_active_streams(0), video_streams(0), audio_streams(0),
          average_bitrate_mbps(0.0), average_latency_ms(0.0), total_bytes_streamed(0),
          dropped_frames_total(0), stream_success_rate(1.0) {}
};

struct ApplicationMetrics {
    // Servidor
    uint64_t total_requests;
    uint64_t active_connections;
    double average_response_time_ms;
    int error_count;

    // Database
    uint64_t database_connections_active;
    uint64_t database_queries_total;
    double database_query_time_avg_ms;

    // Cache
    double cache_hit_rate;
    uint64_t cache_entries_total;
    uint64_t cache_evictions_total;

    ApplicationMetrics()
        : total_requests(0), active_connections(0), average_response_time_ms(0.0), error_count(0),
          database_connections_active(0), database_queries_total(0), database_query_time_avg_ms(0.0),
          cache_hit_rate(0.0), cache_entries_total(0), cache_evictions_total(0) {}
};

// Classe principal do coletor de métricas
class MetricsCollector {
public:
    static MetricsCollector& getInstance();

    // Lifecycle
    bool initialize();
    void shutdown();
    void reset();

    // Coleta de métricas
    void collectSystemMetrics();
    void collectDeviceMetrics();
    void collectStreamingMetrics();
    void collectApplicationMetrics();

    // Atualização manual de métricas
    void incrementCounter(const std::string& name, uint64_t value = 1,
                         const std::unordered_map<std::string, std::string>& labels = {});
    void setGauge(const std::string& name, double value,
                  const std::unordered_map<std::string, std::string>& labels = {});
    void observeHistogram(const std::string& name, double value,
                         const std::unordered_map<std::string, std::string>& labels = {});
    void observeSummary(const std::string& name, double value,
                       const std::unordered_map<std::string, std::string>& labels = {});

    // Acesso às métricas coletadas
    SystemMetrics getSystemMetrics() const;
    std::vector<DeviceMetrics> getDeviceMetrics() const;
    StreamingMetrics getStreamingMetrics() const;
    ApplicationMetrics getApplicationMetrics() const;

    // Exportação
    std::string exportToPrometheus() const;
    std::string exportToJson() const;

    // Configuração
    void setCollectionInterval(std::chrono::seconds interval);
    void enableMetric(const std::string& name, bool enabled);
    void setMetricLabels(const std::string& name,
                        const std::unordered_map<std::string, std::string>& labels);

private:
    MetricsCollector();

    // Helpers para coleta
    void collectCpuMetrics(SystemMetrics& metrics);
    void collectMemoryMetrics(SystemMetrics& metrics);
    void collectDiskMetrics(SystemMetrics& metrics);
    void collectNetworkMetrics(SystemMetrics& metrics);
    void collectSystemLoadMetrics(SystemMetrics& metrics);

    // Formatação Prometheus
    std::string formatPrometheusMetric(const MetricValue& metric) const;
    std::string formatPrometheusLabels(const std::unordered_map<std::string, std::string>& labels) const;

    // Estado interno
    mutable std::mutex mutex_;
    SystemMetrics systemMetrics_;
    std::vector<DeviceMetrics> deviceMetrics_;
    StreamingMetrics streamingMetrics_;
    ApplicationMetrics applicationMetrics_;

    std::unordered_map<std::string, MetricValue> customMetrics_;
    std::chrono::seconds collectionInterval_;
    std::chrono::system_clock::time_point lastCollection_;

    // Configurações
    std::unordered_map<std::string, bool> enabledMetrics_;

    // Non-copyable
    MetricsCollector(const MetricsCollector&) = delete;
    MetricsCollector& operator=(const MetricsCollector&) = delete;
};

} // namespace AndroidStreamManager

#endif // METRICS_COLLECTOR_H