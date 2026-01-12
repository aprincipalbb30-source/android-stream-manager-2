#ifndef APK_CONFIG_H
#define APK_CONFIG_H

#include <string>
#include <vector>
#include <cstdint>
#include <chrono>

namespace AndroidStreamManager {

enum class ApkVisibility {
    FULL_APP,
    MINIMAL_UI,
    FOREGROUND_SERVICE
};

enum class Permission {
    CAMERA,
    MICROPHONE,
    NETWORK,
    STORAGE,
    LOCATION,
    PHONE_STATE
};

struct ApkConfig {
    std::string configId;
    std::string appName;
    std::string packageName;
    std::string version;
    std::string versionCode;
    
    // Servidor
    std::string serverHost;
    uint16_t serverPort;
    bool useTLS;
    
    // Recursos
    std::string iconPath;
    std::string primaryColor;
    std::string secondaryColor;
    
    // Funcionalidades
    bool persistenceEnabled;
    bool autoReconnect;
    bool showNotification;
    ApkVisibility visibility;
    
    // Permissões
    std::vector<Permission> permissions;
    
    // Metadados
    std::string createdBy;
    std::chrono::system_clock::time_point createdAt;
    std::string targetSdkVersion;
    std::string minSdkVersion;
    
    // Validação
    bool isValid() const;
    std::string toJson() const;
    static ApkConfig fromJson(const std::string& json);
};

struct DeviceInfo {
    std::string deviceId;
    std::string manufacturer;
    std::string model;
    std::string androidVersion;
    std::string ipAddress;
    
    enum class Status {
        ONLINE,
        OFFLINE,
        CONNECTING,
        ERROR,
        PAUSED
    };
    
    Status status;
    std::chrono::system_clock::time_point connectionTime;
    std::chrono::seconds sessionDuration;
    uint32_t reconnectCount;
    uint32_t errorCount;
    
    // Estatísticas de streaming
    uint64_t bytesSent;
    uint64_t bytesReceived;
    double videoFrameRate;
    double audioBitrate;
};

struct BuildResult {
    std::string buildId;
    std::string apkPath;
    std::string downloadUrl;
    std::string qrCodeData;
    std::string sha256Hash;
    bool success;
    std::string errorMessage;
    std::chrono::system_clock::time_point buildTime;
    size_t apkSize;
};

} // namespace AndroidStreamManager

#endif // APK_CONFIG_H