#ifndef APK_BUILDER_H
#define APK_BUILDER_H

#include <string>
#include <vector>
#include <chrono>
#include <shared/apk_config.h>

namespace AndroidStreamManager {

struct BuildResult {
    bool success;
    std::string apkPath;
    std::string buildId;
    std::string sha256Hash;
    std::string errorMessage;
    std::chrono::system_clock::time_point buildTime;
};

class ApkBuilder {
public:
    ApkBuilder(const std::string& androidSdkPath, const std::string& templatePath);
    ~ApkBuilder();

    BuildResult buildApk(const ApkConfig& config);

private:
    bool validateConfig(const ApkConfig& config);
    std::string generateBuildId();
    std::string createBuildDirectory(const std::string& buildId);
    bool copyTemplate(const std::string& buildDir, const ApkConfig& config);
    bool customizeFiles(const std::string& buildDir, const ApkConfig& config);
    bool customizeManifest(const std::string& buildDir, const ApkConfig& config);
    bool customizeBuildGradle(const std::string& buildDir, const ApkConfig& config);
    bool customizeStrings(const std::string& buildDir, const ApkConfig& config);
    std::string generatePermissions(const ApkConfig& config);
    std::string generateServerConfig(const ApkConfig& config);
    bool compileResources(const std::string& buildDir);
    bool generateBytecode(const std::string& buildDir);
    std::string packageApk(const std::string& buildDir, const ApkConfig& config);
    std::string calculateSha256(const std::string& filePath);
    void cleanup(const std::string& buildDir);
    void replaceAll(std::string& str, const std::string& from, const std::string& to);

    std::string androidSdkPath_;
    std::string templatePath_;
    uint64_t buildCounter_;
};

} // namespace AndroidStreamManager

#endif // APK_BUILDER_H