#ifndef APK_CONFIG_H
#define APK_CONFIG_H

#include <string>
#include <vector>

namespace AndroidStreamManager {

struct ApkConfig {
    // Informações básicas do app
    std::string appName;
    std::string packageName;
    std::string versionName;
    int versionCode;

    // Configuração do SDK
    int minSdkVersion;
    int targetSdkVersion;
    int compileSdkVersion;

    // Servidor
    std::string serverUrl;
    int serverPort;

    // Aparência
    std::string iconPath;
    std::string theme;

    // Permissões
    std::vector<std::string> permissions;

    // Configurações específicas
    bool enableDebug;
    bool enableProguard;

    // Assinatura
    std::string keystorePath;
    std::string keystorePass;
    std::string keyAlias;
    std::string keyPass;

    // Funcionalidades do APK
    bool enableWebview;
    std::string webviewUrl;
    bool backgroundOnly;
    bool hideIcon;

    // Construtor padrão
    ApkConfig()
        : versionCode(1)
        , minSdkVersion(23)
        , targetSdkVersion(33)
        , compileSdkVersion(33)
        , serverPort(8443)
        , enableDebug(false)
        , enableProguard(false)
        , enableWebview(false)
        , backgroundOnly(false)
        , hideIcon(false) {}

    // Método auxiliar para adicionar permissões comuns
    void addCommonPermissions() {
        permissions.push_back("INTERNET");
        permissions.push_back("ACCESS_NETWORK_STATE");
        permissions.push_back("WAKE_LOCK");
    }

    // Método auxiliar para adicionar permissões de mídia
    void addMediaPermissions() {
        permissions.push_back("CAMERA");
        permissions.push_back("RECORD_AUDIO");
        permissions.push_back("READ_EXTERNAL_STORAGE");
        permissions.push_back("WRITE_EXTERNAL_STORAGE");
    }

    // Validação básica
    bool isValid() const {
        return !appName.empty() &&
               !packageName.empty() &&
               !serverUrl.empty() &&
               minSdkVersion > 0 &&
               targetSdkVersion >= minSdkVersion &&
               compileSdkVersion >= targetSdkVersion;
    }
};

} // namespace AndroidStreamManager

#endif // APK_CONFIG_H