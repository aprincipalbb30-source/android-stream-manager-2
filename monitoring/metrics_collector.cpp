#include "metrics_collector.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <thread>
#include <fstream>
#include <regex>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#include <pdh.h>
#pragma comment(lib, "pdh.lib")
#else
#include <unistd.h>
#include <sys/sysinfo.h>
#include <sys/statvfs.h>
#include <proc/readproc.h>
#endif

namespace AndroidStreamManager {

MetricsCollector& MetricsCollector::getInstance() {
    static MetricsCollector instance;
    return instance;
}

MetricsCollector::MetricsCollector()
    : collectionInterval_(30) { // 30 segundos por padrão
    std::cout << "MetricsCollector criado" << std::endl;
}

bool MetricsCollector::initialize() {
    lastCollection_ = std::chrono::system_clock::now();

    // Métricas padrão habilitadas
    enabledMetrics_["system"] = true;
    enabledMetrics_["devices"] = true;
    enabledMetrics_["streaming"] = true;
    enabledMetrics_["application"] = true;

    std::cout << "MetricsCollector inicializado com intervalo de "
              << collectionInterval_.count() << " segundos" << std::endl;

    return true;
}

void MetricsCollector::shutdown() {
    // Limpar métricas
    std::lock_guard<std::mutex> lock(mutex_);
    customMetrics_.clear();
    deviceMetrics_.clear();

    std::cout << "MetricsCollector finalizado" << std::endl;
}

void MetricsCollector::reset() {
    std::lock_guard<std::mutex> lock(mutex_);

    // Resetar métricas do sistema
    systemMetrics_ = SystemMetrics();
    deviceMetrics_.clear();
    streamingMetrics_ = StreamingMetrics();
    applicationMetrics_ = ApplicationMetrics();

    // Manter métricas customizadas, apenas resetar valores
    for (auto& pair : customMetrics_) {
        pair.second.value = 0.0;
        pair.second.timestamp = std::chrono::system_clock::now();
    }
}

void MetricsCollector::collectSystemMetrics() {
    if (!enabledMetrics_["system"]) return;

    std::lock_guard<std::mutex> lock(mutex_);

    collectCpuMetrics(systemMetrics_);
    collectMemoryMetrics(systemMetrics_);
    collectDiskMetrics(systemMetrics_);
    collectNetworkMetrics(systemMetrics_);
    collectSystemLoadMetrics(systemMetrics_);

    lastCollection_ = std::chrono::system_clock::now();
}

void MetricsCollector::collectDeviceMetrics() {
    if (!enabledMetrics_["devices"]) return;

    std::lock_guard<std::mutex> lock(mutex_);

    // TODO: Integrar com DeviceManager real
    // Por enquanto, simular alguns dispositivos
    deviceMetrics_.clear();

    // Dispositivo de exemplo 1
    DeviceMetrics device1;
    device1.device_id = "device_001";
    device1.connected = true;
    device1.battery_level = 85;
    device1.is_charging = false;
    device1.cpu_usage_percent = 15.5;
    device1.memory_used_bytes = 256 * 1024 * 1024; // 256MB
    device1.network_sent_bytes = 1024 * 1024; // 1MB
    device1.network_received_bytes = 512 * 1024; // 512KB
    device1.connection_duration = std::chrono::seconds(3600); // 1 hora
    device1.active_streams = 1;

    // Dispositivo de exemplo 2
    DeviceMetrics device2;
    device2.device_id = "device_002";
    device2.connected = true;
    device2.battery_level = 92;
    device2.is_charging = true;
    device2.cpu_usage_percent = 8.2;
    device2.memory_used_bytes = 180 * 1024 * 1024; // 180MB
    device2.network_sent_bytes = 256 * 1024; // 256KB
    device2.network_received_bytes = 128 * 1024; // 128KB
    device2.connection_duration = std::chrono::seconds(1800); // 30 min
    device2.active_streams = 0;

    deviceMetrics_.push_back(device1);
    deviceMetrics_.push_back(device2);
}

void MetricsCollector::collectStreamingMetrics() {
    if (!enabledMetrics_["streaming"]) return;

    std::lock_guard<std::mutex> lock(mutex_);

    // TODO: Integrar com streaming real
    // Por enquanto, métricas simuladas
    streamingMetrics_.total_active_streams = 3;
    streamingMetrics_.video_streams = 2;
    streamingMetrics_.audio_streams = 1;
    streamingMetrics_.average_bitrate_mbps = 2.5;
    streamingMetrics_.average_latency_ms = 45.2;
    streamingMetrics_.total_bytes_streamed = 1024 * 1024 * 1024; // 1GB
    streamingMetrics_.dropped_frames_total = 12;
    streamingMetrics_.stream_success_rate = 0.987; // 98.7%
}

void MetricsCollector::collectApplicationMetrics() {
    if (!enabledMetrics_["application"]) return;

    std::lock_guard<std::mutex> lock(mutex_);

    // TODO: Integrar com componentes reais
    // Por enquanto, métricas simuladas
    applicationMetrics_.total_requests = 15420;
    applicationMetrics_.active_connections = 5;
    applicationMetrics_.average_response_time_ms = 23.5;
    applicationMetrics_.error_count = 3;

    applicationMetrics_.database_connections_active = 2;
    applicationMetrics_.database_queries_total = 45230;
    applicationMetrics_.database_query_time_avg_ms = 2.1;

    applicationMetrics_.cache_hit_rate = 0.894; // 89.4%
    applicationMetrics_.cache_entries_total = 1250;
    applicationMetrics_.cache_evictions_total = 45;
}

void MetricsCollector::collectCpuMetrics(SystemMetrics& metrics) {
#ifdef _WIN32
    // Windows CPU metrics
    FILETIME idleTime, kernelTime, userTime;
    if (GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
        static ULARGE_INTEGER prevIdleTime = {0}, prevKernelTime = {0}, prevUserTime = {0};
        ULARGE_INTEGER currIdleTime, currKernelTime, currUserTime;

        currIdleTime.LowPart = idleTime.dwLowDateTime;
        currIdleTime.HighPart = idleTime.dwHighDateTime;
        currKernelTime.LowPart = kernelTime.dwLowDateTime;
        currKernelTime.HighPart = kernelTime.dwHighDateTime;
        currUserTime.LowPart = userTime.dwLowDateTime;
        currUserTime.HighPart = userTime.dwHighDateTime;

        if (prevIdleTime.QuadPart != 0) {
            ULONGLONG idleDiff = currIdleTime.QuadPart - prevIdleTime.QuadPart;
            ULONGLONG kernelDiff = currKernelTime.QuadPart - prevKernelTime.QuadPart;
            ULONGLONG userDiff = currUserTime.QuadPart - prevUserTime.QuadPart;
            ULONGLONG totalDiff = kernelDiff + userDiff;

            if (totalDiff > 0) {
                metrics.cpu_usage_percent = 100.0 * (totalDiff - idleDiff) / totalDiff;
            }
        }

        prevIdleTime = currIdleTime;
        prevKernelTime = currKernelTime;
        prevUserTime = currUserTime;
    }
#else
    // Linux CPU metrics
    std::ifstream statFile("/proc/stat");
    if (statFile.is_open()) {
        std::string line;
        std::getline(statFile, line);

        std::istringstream iss(line);
        std::string cpu;
        unsigned long long user, nice, system, idle, iowait, irq, softirq;
        iss >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq;

        static unsigned long long prevTotal = 0, prevIdle = 0;
        unsigned long long total = user + nice + system + idle + iowait + irq + softirq;

        if (prevTotal > 0) {
            unsigned long long totalDiff = total - prevTotal;
            unsigned long long idleDiff = idle - prevIdle;

            if (totalDiff > 0) {
                metrics.cpu_usage_percent = 100.0 * (totalDiff - idleDiff) / totalDiff;
            }
        }

        prevTotal = total;
        prevIdle = idle;
    }
#endif

    // CPU Temperature (se disponível)
    metrics.cpu_temperature_celsius = 45.0; // Placeholder
}

void MetricsCollector::collectMemoryMetrics(SystemMetrics& metrics) {
#ifdef _WIN32
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        metrics.memory_total_bytes = memInfo.ullTotalPhys;
        metrics.memory_used_bytes = memInfo.ullTotalPhys - memInfo.ullAvailPhys;
        metrics.memory_free_bytes = memInfo.ullAvailPhys;
    }
#else
    struct sysinfo memInfo;
    if (sysinfo(&memInfo) == 0) {
        metrics.memory_total_bytes = memInfo.totalram * memInfo.mem_unit;
        metrics.memory_free_bytes = memInfo.freeram * memInfo.mem_unit;
        metrics.memory_used_bytes = metrics.memory_total_bytes - metrics.memory_free_bytes;
    }
#endif
}

void MetricsCollector::collectDiskMetrics(SystemMetrics& metrics) {
#ifdef _WIN32
    ULARGE_INTEGER freeBytes, totalBytes, totalFreeBytes;
    if (GetDiskFreeSpaceExA("C:", &freeBytes, &totalBytes, &totalFreeBytes)) {
        metrics.disk_total_bytes = totalBytes.QuadPart;
        metrics.disk_free_bytes = freeBytes.QuadPart;
        metrics.disk_used_bytes = totalBytes.QuadPart - freeBytes.QuadPart;
    }
#else
    struct statvfs stat;
    if (statvfs("/", &stat) == 0) {
        metrics.disk_total_bytes = stat.f_blocks * stat.f_frsize;
        metrics.disk_free_bytes = stat.f_available * stat.f_frsize;
        metrics.disk_used_bytes = metrics.disk_total_bytes - metrics.disk_free_bytes;
    }
#endif
}

void MetricsCollector::collectNetworkMetrics(SystemMetrics& metrics) {
    // Placeholder - em produção, implementar leitura real de /proc/net/dev ou GetIfTable
    static uint64_t baseSent = 0, baseReceived = 0;
    if (baseSent == 0) {
        baseSent = 1024 * 1024; // 1MB base
        baseReceived = 512 * 1024; // 512KB base
    }

    metrics.network_bytes_sent = baseSent + (rand() % 1000);
    metrics.network_bytes_received = baseReceived + (rand() % 500);
    metrics.network_packets_sent = metrics.network_bytes_sent / 1500; // Aproximado
    metrics.network_packets_received = metrics.network_bytes_received / 1500;
}

void MetricsCollector::collectSystemLoadMetrics(SystemMetrics& metrics) {
#ifdef _WIN32
    // Windows load average (simplificado)
    metrics.load_average_1m = 0.5;
    metrics.load_average_5m = 0.4;
    metrics.load_average_15m = 0.3;
#else
    double load[3];
    if (getloadavg(load, 3) == 3) {
        metrics.load_average_1m = load[0];
        metrics.load_average_5m = load[1];
        metrics.load_average_15m = load[2];
    }
#endif

    // Uptime
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now() - std::chrono::system_clock::time_point{});
    metrics.uptime_seconds = std::chrono::seconds(uptime.count());
}

void MetricsCollector::incrementCounter(const std::string& name, uint64_t value,
                                       const std::unordered_map<std::string, std::string>& labels) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = customMetrics_.find(name);
    if (it == customMetrics_.end()) {
        customMetrics_[name] = MetricValue(name, "Custom counter", MetricType::COUNTER);
        customMetrics_[name].labels = labels;
    }

    customMetrics_[name].value += value;
    customMetrics_[name].timestamp = std::chrono::system_clock::now();
}

void MetricsCollector::setGauge(const std::string& name, double value,
                                const std::unordered_map<std::string, std::string>& labels) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = customMetrics_.find(name);
    if (it == customMetrics_.end()) {
        customMetrics_[name] = MetricValue(name, "Custom gauge", MetricType::GAUGE);
        customMetrics_[name].labels = labels;
    }

    customMetrics_[name].value = value;
    customMetrics_[name].timestamp = std::chrono::system_clock::now();
}

void MetricsCollector::observeHistogram(const std::string& name, double value,
                                       const std::unordered_map<std::string, std::string>& labels) {
    // TODO: Implementar histograma real
    setGauge(name + "_observed", value, labels);
}

void MetricsCollector::observeSummary(const std::string& name, double value,
                                     const std::unordered_map<std::string, std::string>& labels) {
    // TODO: Implementar summary real
    setGauge(name + "_summary", value, labels);
}

SystemMetrics MetricsCollector::getSystemMetrics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return systemMetrics_;
}

std::vector<DeviceMetrics> MetricsCollector::getDeviceMetrics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return deviceMetrics_;
}

StreamingMetrics MetricsCollector::getStreamingMetrics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return streamingMetrics_;
}

ApplicationMetrics MetricsCollector::getApplicationMetrics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return applicationMetrics_;
}

std::string MetricsCollector::exportToPrometheus() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream oss;

    // System metrics
    oss << "# HELP android_stream_manager_cpu_usage_percent CPU usage percentage\n";
    oss << "# TYPE android_stream_manager_cpu_usage_percent gauge\n";
    oss << "android_stream_manager_cpu_usage_percent " << systemMetrics_.cpu_usage_percent << "\n";

    oss << "# HELP android_stream_manager_memory_used_bytes Memory used in bytes\n";
    oss << "# TYPE android_stream_manager_memory_used_bytes gauge\n";
    oss << "android_stream_manager_memory_used_bytes " << systemMetrics_.memory_used_bytes << "\n";

    oss << "# HELP android_stream_manager_disk_used_bytes Disk used in bytes\n";
    oss << "# TYPE android_stream_manager_disk_used_bytes gauge\n";
    oss << "android_stream_manager_disk_used_bytes " << systemMetrics_.disk_used_bytes << "\n";

    // Device metrics
    for (const auto& device : deviceMetrics_) {
        std::string deviceLabel = "{device_id=\"" + device.device_id + "\"}";

        oss << "# HELP android_stream_manager_device_connected Device connection status\n";
        oss << "# TYPE android_stream_manager_device_connected gauge\n";
        oss << "android_stream_manager_device_connected" << deviceLabel << " "
            << (device.connected ? 1 : 0) << "\n";

        oss << "# HELP android_stream_manager_device_battery_level Device battery level\n";
        oss << "# TYPE android_stream_manager_device_battery_level gauge\n";
        oss << "android_stream_manager_device_battery_level" << deviceLabel << " "
            << device.battery_level << "\n";
    }

    // Application metrics
    oss << "# HELP android_stream_manager_requests_total Total requests\n";
    oss << "# TYPE android_stream_manager_requests_total counter\n";
    oss << "android_stream_manager_requests_total " << applicationMetrics_.total_requests << "\n";

    oss << "# HELP android_stream_manager_active_connections Active connections\n";
    oss << "# TYPE android_stream_manager_active_connections gauge\n";
    oss << "android_stream_manager_active_connections " << applicationMetrics_.active_connections << "\n";

    return oss.str();
}

std::string MetricsCollector::exportToJson() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream oss;

    oss << "{\n";

    // System metrics
    oss << "  \"system\": {\n";
    oss << "    \"cpu_usage_percent\": " << systemMetrics_.cpu_usage_percent << ",\n";
    oss << "    \"memory_used_bytes\": " << systemMetrics_.memory_used_bytes << ",\n";
    oss << "    \"memory_total_bytes\": " << systemMetrics_.memory_total_bytes << ",\n";
    oss << "    \"disk_used_bytes\": " << systemMetrics_.disk_used_bytes << ",\n";
    oss << "    \"disk_total_bytes\": " << systemMetrics_.disk_total_bytes << ",\n";
    oss << "    \"uptime_seconds\": " << systemMetrics_.uptime_seconds.count() << "\n";
    oss << "  },\n";

    // Device metrics
    oss << "  \"devices\": [\n";
    for (size_t i = 0; i < deviceMetrics_.size(); ++i) {
        const auto& device = deviceMetrics_[i];
        oss << "    {\n";
        oss << "      \"device_id\": \"" << device.device_id << "\",\n";
        oss << "      \"connected\": " << (device.connected ? "true" : "false") << ",\n";
        oss << "      \"battery_level\": " << device.battery_level << ",\n";
        oss << "      \"cpu_usage_percent\": " << device.cpu_usage_percent << ",\n";
        oss << "      \"memory_used_bytes\": " << device.memory_used_bytes << ",\n";
        oss << "      \"active_streams\": " << device.active_streams << "\n";
        oss << "    }";
        if (i < deviceMetrics_.size() - 1) oss << ",";
        oss << "\n";
    }
    oss << "  ],\n";

    // Application metrics
    oss << "  \"application\": {\n";
    oss << "    \"total_requests\": " << applicationMetrics_.total_requests << ",\n";
    oss << "    \"active_connections\": " << applicationMetrics_.active_connections << ",\n";
    oss << "    \"average_response_time_ms\": " << applicationMetrics_.average_response_time_ms << ",\n";
    oss << "    \"cache_hit_rate\": " << applicationMetrics_.cache_hit_rate << "\n";
    oss << "  }\n";

    oss << "}\n";

    return oss.str();
}

void MetricsCollector::setCollectionInterval(std::chrono::seconds interval) {
    collectionInterval_ = interval;
}

void MetricsCollector::enableMetric(const std::string& name, bool enabled) {
    enabledMetrics_[name] = enabled;
}

void MetricsCollector::setMetricLabels(const std::string& name,
                                      const std::unordered_map<std::string, std::string>& labels) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = customMetrics_.find(name);
    if (it != customMetrics_.end()) {
        it->second.labels = labels;
    }
}

} // namespace AndroidStreamManager