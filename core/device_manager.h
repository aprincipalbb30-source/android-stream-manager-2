#ifndef DEVICE_MANAGER_H
#define DEVICE_MANAGER_H

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>
#include <memory>
#include <shared/protocol.h>

namespace AndroidStreamManager {

enum class DeviceStatus {
    CONNECTED,
    DISCONNECTED,
    STREAMING,
    ERROR
};

struct DeviceInfo {
    std::string deviceId;
    std::string deviceModel;
    std::string androidVersion;
    std::string appVersion;
    std::string ipAddress;
    int batteryLevel;
    bool isCharging;

    DeviceInfo()
        : batteryLevel(0)
        , isCharging(false) {}
};

struct StreamConfig {
    std::string streamType; // "screen", "audio", "both"
    int quality; // 1-100
    int frameRate; // FPS
    bool enableCompression;

    StreamConfig()
        : quality(80)
        , frameRate(30)
        , enableCompression(true) {}
};

struct DeviceSession {
    DeviceInfo deviceInfo;
    DeviceStatus status;
    bool streamingActive;
    StreamConfig streamConfig;
    std::chrono::system_clock::time_point connectedAt;
    std::chrono::system_clock::time_point lastHeartbeat;
    std::chrono::system_clock::time_point streamStartedAt;
    std::chrono::system_clock::time_point streamEndedAt;

    DeviceSession()
        : status(DeviceStatus::DISCONNECTED)
        , streamingActive(false) {}
};

struct DeviceStats {
    size_t totalDevices;
    size_t streamingDevices;
    std::chrono::seconds totalUptime;
    std::chrono::seconds averageUptime;

    DeviceStats()
        : totalDevices(0)
        , streamingDevices(0)
        , totalUptime(0)
        , averageUptime(0) {}
};

class DeviceListener {
public:
    virtual ~DeviceListener() = default;
    virtual void onDeviceConnected(const std::string& deviceId) = 0;
    virtual void onDeviceDisconnected(const std::string& deviceId) = 0;
    virtual void onStreamingStarted(const std::string& deviceId) = 0;
    virtual void onStreamingStopped(const std::string& deviceId) = 0;
};

class DeviceManager {
public:
    DeviceManager();
    ~DeviceManager();

    // Lifecycle
    bool initialize();
    void shutdown();

    // Device management
    bool registerDevice(const DeviceInfo& deviceInfo);
    bool unregisterDevice(const std::string& deviceId);
    bool updateDeviceHeartbeat(const std::string& deviceId);

    // Streaming control
    bool startStreaming(const std::string& deviceId, const StreamConfig& config);
    bool stopStreaming(const std::string& deviceId);

    // Communication
    bool sendCommand(const std::string& deviceId, const ControlMessage& command);

    // Queries
    std::vector<DeviceInfo> getConnectedDevices() const;
    DeviceSession getDeviceSession(const std::string& deviceId) const;
    bool isDeviceConnected(const std::string& deviceId) const;
    bool isDeviceStreaming(const std::string& deviceId) const;
    DeviceStats getDeviceStats() const;

    // Listeners
    void addDeviceListener(DeviceListener* listener);
    void removeDeviceListener(DeviceListener* listener);

private:
    // Maintenance
    void maintenanceLoop();
    void checkInactiveDevices();
    void disconnectDevice(const std::string& deviceId);

    // Notifications
    void notifyDeviceConnected(const std::string& deviceId);
    void notifyDeviceDisconnected(const std::string& deviceId);
    void notifyStreamingStarted(const std::string& deviceId);
    void notifyStreamingStopped(const std::string& deviceId);

    mutable std::mutex mutex_;
    std::unordered_map<std::string, DeviceSession> connectedDevices_;
    std::vector<DeviceListener*> listeners_;
    std::thread maintenanceThread_;
    std::condition_variable maintenanceCV_;
    bool running_;
};

} // namespace AndroidStreamManager

#endif // DEVICE_MANAGER_H