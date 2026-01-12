#ifndef APK_BUILDER_H
#define APK_BUILDER_H

#include "shared/apk_config.h"
#include <string>
#include <memory>
#include <functional>

namespace AndroidStreamManager {

class IApkBuilder {
public:
    virtual ~IApkBuilder() = default;
    
    virtual BuildResult buildApk(const ApkConfig& config) = 0;
    virtual bool validateConfig(const ApkConfig& config) = 0;
    virtual std::string generatePackageName(const std::string& appName) = 0;
    
    // Callback para progresso
    using ProgressCallback = std::function<void(int, const std::string&)>;
    virtual void setProgressCallback(ProgressCallback callback) = 0;
};

class ApkBuilder : public IApkBuilder {
public:
    explicit ApkBuilder(const std::string& androidSdkPath,
                       const std::string& templatePath);
    ~ApkBuilder() override;
    
    BuildResult buildApk(const ApkConfig& config) override;
    bool validateConfig(const ApkConfig& config) override;
    std::string generatePackageName(const std::string& appName) override;
    void setProgressCallback(ProgressCallback callback) override;
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace AndroidStreamManager

#endif // APK_BUILDER_H