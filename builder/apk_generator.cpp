#include "apk_generator.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <regex>

namespace fs = std::filesystem;

namespace AndroidStreamManager {

class ApkBuilder::Impl {
public:
    Impl(const std::string& androidSdkPath,
         const std::string& templatePath)
        : androidSdkPath(androidSdkPath)
        , templatePath(templatePath) {}
    
    BuildResult buildApkImpl(const ApkConfig& config) {
        BuildResult result;
        result.buildId = generateBuildId();
        
        try {
            // 1. Preparar diretório temporário
            std::string tempDir = createTempDirectory();
            
            // 2. Copiar template
            copyTemplate(tempDir);
            
            // 3. Aplicar configurações
            updateAndroidManifest(tempDir, config);
            updateStringsXml(tempDir, config);
            updateColorsXml(tempDir, config);
            updateIcon(tempDir, config);
            
            // 4. Executar Gradle
            if (!executeGradleBuild(tempDir)) {
                throw std::runtime_error("Gradle build failed");
            }
            
            // 5. Copiar APK gerado
            result.apkPath = copyOutputApk(tempDir, config);
            result.sha256Hash = calculateSHA256(result.apkPath);
            result.success = true;
            
            // 6. Limpar
            fs::remove_all(tempDir);
            
        } catch (const std::exception& e) {
            result.success = false;
            result.errorMessage = e.what();
        }
        
        return result;
    }
    
private:
    std::string androidSdkPath;
    std::string templatePath;
    ProgressCallback progressCallback;
    
    std::string createTempDirectory() {
        auto tempPath = fs::temp_directory_path() / 
                       ("asmbuild_" + std::to_string(time(nullptr)));
        fs::create_directories(tempPath);
        return tempPath.string();
    }
    
    void updateAndroidManifest(const std::string& projectDir,
                              const ApkConfig& config) {
        std::string manifestPath = projectDir + "/app/src/main/AndroidManifest.xml";
        std::ifstream inFile(manifestPath);
        std::stringstream buffer;
        buffer << inFile.rdbuf();
        std::string content = buffer.str();
        inFile.close();
        
        // Atualizar nome do pacote
        std::string packageName = config.packageName.empty() ?
            generatePackageName(config.appName) : config.packageName;
        
        content = std::regex_replace(content,
            std::regex("package=\"[^\"]*\""),
            "package=\"" + packageName + "\"");
        
        // Atualizar permissões
        std::string permissionsBlock;
        for (const auto& perm : config.permissions) {
            permissionsBlock += "    <uses-permission android:name=\"" +
                               permissionToString(perm) + "\"/>\n";
        }
        
        content = std::regex_replace(content,
            std::regex("<!-- PERMISSIONS_PLACEHOLDER -->"),
            permissionsBlock);
        
        // Atualizar configurações de serviço
        if (config.visibility == ApkVisibility::FOREGROUND_SERVICE) {
            std::string serviceConfig = 
                "android:foregroundServiceType=\"mediaProjection|camera|microphone\"\n"
                "        android:foregroundServiceType=\"mediaProjection\"";
            content = std::regex_replace(content,
                std::regex("<!-- SERVICE_CONFIG_PLACEHOLDER -->"),
                serviceConfig);
        }
        
        // Escrever arquivo atualizado
        std::ofstream outFile(manifestPath);
        outFile << content;
        outFile.close();
    }
    
    std::string permissionToString(Permission perm) {
        switch (perm) {
            case Permission::CAMERA:
                return "android.permission.CAMERA";
            case Permission::MICROPHONE:
                return "android.permission.RECORD_AUDIO";
            case Permission::STORAGE:
                return "android.permission.WRITE_EXTERNAL_STORAGE";
            case Permission::NETWORK:
                return "android.permission.INTERNET";
            default:
                return "";
        }
    }
    
    bool executeGradleBuild(const std::string& projectDir) {
        std::string gradleCmd = "cd \"" + projectDir + "\" && ";
        gradleCmd += "\"" + androidSdkPath + "/gradlew\" assembleRelease";
        
        if (progressCallback) {
            progressCallback(50, "Building APK with Gradle...");
        }
        
        // Executar comando (simplificado)
        int result = system(gradleCmd.c_str());
        return result == 0;
    }
};

} // namespace AndroidStreamManager