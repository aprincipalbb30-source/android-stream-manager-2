#include "apk_builder.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <regex>
#include <openssl/sha.h>
#include <chrono>
#include <thread>

// Funções utilitárias que estavam faltando
void replaceAll(std::string& str, const std::string& from, const std::string& to) {
    if(from.empty())
        return;
    size_t start_pos = 0;
    while((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
}

std::string readFile(const std::filesystem::path& path); // Declaração
void writeFile(const std::filesystem::path& path, const std::string& content); // Declaração


#include "security/apk_signer.h" // Assuming ApkSigner is available
#include "shared/apk_config.h" // Ensure ApkConfig is included

namespace AndroidStreamManager {

ApkBuilder::ApkBuilder(const std::string& androidSdkPath,
                       const std::string& templatePath)
    : androidSdkPath_(androidSdkPath)
    , templatePath_(templatePath)
    , buildCounter_(0) {

    std::cout << "ApkBuilder inicializado" << std::endl;
    std::cout << "SDK Path: " << androidSdkPath_ << std::endl;
    std::cout << "Template Path: " << templatePath_ << std::endl;
}

ApkBuilder::~ApkBuilder() { // No changes needed here
    std::cout << "ApkBuilder destruído" << std::endl;
}

BuildResult ApkBuilder::buildApk(const ApkConfig& config) {
    BuildResult result;
    result.success = false;

    try {
        std::cout << "Iniciando build do APK: " << config.appName << std::endl;

        // 1. Validar configuração
        if (!validateConfig(config)) {
            result.errorMessage = "Configuração inválida";
            return result;
        }

        // 2. Preparar diretório de build
        std::string buildId = generateBuildId();
        std::string buildDir = createBuildDirectory(buildId);

        if (buildDir.empty()) {
            result.errorMessage = "Falha ao criar diretório de build";
            return result;
        }

        // 3. Copiar template
        if (!copyTemplate(buildDir, config)) {
            result.errorMessage = "Falha ao copiar template";
            return result;
        }

        // 4. Modificar template Android
        if (!modifyAndroidTemplate(buildDir, config)) {
            result.errorMessage = "Falha ao modificar template Android";
            return result;
        }

        // 5. Executar Gradle para compilar o APK
        std::string unsignedApkPath = executeGradleBuild(buildDir, config);
        if (unsignedApkPath.empty()) {
            result.errorMessage = "Falha na compilação do APK com Gradle";
            return result;
        }

        // 6. Assinar o APK
        std::string signedApkPath = signApk(unsignedApkPath, config);
        if (signedApkPath.empty()) {
            result.errorMessage = "Falha ao assinar o APK";
            return result;
        }

        // 7. Calcular hash do APK assinado
        result.sha256Hash = calculateSha256(signedApkPath);
        if (result.sha256Hash.empty()) {
            result.errorMessage = "Falha ao calcular hash SHA256 do APK assinado";
            return result;
        }

        // 8. Configurar resultado
        result.success = true;
        result.apkPath = signedApkPath;
        result.buildId = buildId;
        result.buildTime = std::chrono::system_clock::now();

        std::cout << "Build concluído com sucesso: " << signedApkPath << std::endl;

        // 9. Limpar arquivos temporários
        cleanup(buildDir);

    } catch (const std::exception& e) {
        result.errorMessage = std::string("Exceção durante build: ") + e.what();
        std::cerr << "Erro durante build: " << e.what() << std::endl;
    }

    return result;
}

bool ApkBuilder::validateConfig(const ApkConfig& config) {
    if (config.appName.empty()) {
        std::cerr << "Nome do app não especificado" << std::endl;
        return false;
    }

    if (config.packageName.empty()) {
        std::cerr << "Nome do pacote não especificado" << std::endl;
        return false;
    }

    if (config.minSdkVersion < 16) {
        std::cerr << "Versão mínima do SDK muito baixa" << std::endl;
        return false;
    }

    if (config.targetSdkVersion < config.minSdkVersion) {
        std::cerr << "Versão target menor que versão mínima" << std::endl;
        return false;
    }
    
    if (config.enableWebview && config.backgroundOnly) {
        std::cerr << "Modo WebView e Background Only são mutuamente exclusivos." << std::endl;
        return false;
    }

    if (config.enableWebview) {
        // Basic URL validation
        if (config.webviewUrl.empty() || !(config.webviewUrl.rfind("http://", 0) == 0 || config.webviewUrl.rfind("https://", 0) == 0)) {
            std::cerr << "URL para WebView inválida. Deve começar com http:// ou https://." << std::endl;
            return false;
        }
    }

    return true;
}

std::string ApkBuilder::generateBuildId() {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    buildCounter_++;

    std::stringstream ss;
    ss << "build_" << timestamp << "_" << buildCounter_;
    return ss.str();
}

std::string ApkBuilder::createBuildDirectory(const std::string& buildId) {
    try {
        std::filesystem::path buildPath = std::filesystem::temp_directory_path() /
                                         "android_stream_builds" / buildId;

        if (!std::filesystem::create_directories(buildPath)) {
            std::cerr << "Falha ao criar diretório: " << buildPath << std::endl;
            return "";
        }

        std::cout << "Diretório de build criado: " << buildPath << std::endl;
        return buildPath.string();

    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Erro ao criar diretório: " << e.what() << std::endl;
        return "";
    }
}

bool ApkBuilder::copyTemplate(const std::string& buildDir, const ApkConfig& config) {
    try {
        std::filesystem::path templatePath(templatePath_);
        std::filesystem::path buildPath(buildDir);

        if (!std::filesystem::exists(templatePath)) {
            std::cerr << "Template não encontrado: " << templatePath_ << std::endl;
            return false;
        }

        // Copiar template para diretório de build
        std::filesystem::copy(templatePath, buildPath,
                             std::filesystem::copy_options::recursive |
                             std::filesystem::copy_options::overwrite_existing);

        std::cout << "Template copiado para: " << buildDir << std::endl;
        return true;

    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Erro ao copiar template: " << e.what() << std::endl;
        return false;
    }
}

bool ApkBuilder::modifyAndroidTemplate(const std::string& buildDir, const ApkConfig& config) {
    // Personalizar AndroidManifest.xml
    if (!customizeManifest(buildDir, config)) {
        return false; // Error already logged
    }

    // Personalizar build.gradle
    if (!customizeBuildGradle(buildDir, config)) {
        return false;
    }

    // Personalizar strings.xml
    if (!customizeStrings(buildDir, config)) {
        return false;
    }    

    // Personalizar arquivos Java/Kotlin (ex: MainActivity, criar WebViewActivity)
    if (!customizeJavaFiles(buildDir, config)) {
        return false;
    }
    
    std::cout << "Arquivos personalizados" << std::endl;
    return true;
}

bool ApkBuilder::customizeManifest(const std::string& buildDir, const ApkConfig& config) {
    std::filesystem::path manifestPath = std::filesystem::path(buildDir) /
                                        "app/src/main/AndroidManifest.xml";

    if (!std::filesystem::exists(manifestPath)) {
        std::cerr << "AndroidManifest.xml não encontrado" << std::endl;
        return false;
    }

    try {
        std::ifstream manifestFile(manifestPath);
        std::stringstream buffer;
        buffer << manifestFile.rdbuf();
        std::string content = buffer.str();
        manifestFile.close();

        // Substituir placeholders
        content = std::regex_replace(content, std::regex("package=\"[^\"]+\""), "package=\"" + config.packageName + "\"");
        content = std::regex_replace(content, std::regex("android:label=\"@string/app_name\""), "android:label=\"" + config.appName + "\"");
        
        // Ícone
        std::string iconRef = config.iconPath.empty() ? "@mipmap/ic_launcher" : "@drawable/" + std::filesystem::path(config.iconPath).stem().string(); // Assuming icon is copied to drawable
        content = std::regex_replace(content, std::regex("android:icon=\"@mipmap/ic_launcher\""), "android:icon=\"" + iconRef + "\"");
        content = std::regex_replace(content, std::regex("android:roundIcon=\"@mipmap/ic_launcher_round\""), "android:roundIcon=\"" + iconRef + "_round\""); // Assuming round icon also uses the same base name

        // Configuração do servidor (pode ser injetada em strings.xml ou em código Java)
        // replaceAll(content, "{{SERVER_CONFIG}}", generateServerConfig(config));

        // Manipulação de LAUNCHER intent-filter
        std::string launcherIntentFilterRegex = R"(<intent-filter>\s*<action android:name="android.intent.action.MAIN" />\s*<category android:name="android.intent.category.LAUNCHER" />\s*</intent-filter>)";
        
        if (config.backgroundOnly || config.hideIcon) {
            // Remover intent-filter de LAUNCHER
            content = std::regex_replace(content, std::regex(launcherIntentFilterRegex), "");
            std::cout << "Removido intent-filter LAUNCHER da MainActivity." << std::endl;
        }

        if (config.enableWebview) {
            // Adicionar WebViewActivity e configurá-la como LAUNCHER
            std::string webviewActivityDeclaration = R"(
        <activity
            android:name=")" + config.packageName + R"(.WebViewActivity"
            android:exported="true"
            android:label="@string/app_name"
            android:theme="@style/Theme.AppCompat.Light.NoActionBar">
            <intent-filter>
                <action android:name="android.intent.action.MAIN" />
                <category android:name="android.intent.category.LAUNCHER" />
            </intent-filter>
        </activity>)";
            // Inserir antes do fechamento da tag <application>
            content = std::regex_replace(content, std::regex("</application>"), webviewActivityDeclaration + "\n</application>");
            std::cout << "Configurado WebViewActivity como launcher." << std::endl;

            // Se a MainActivity ainda tiver o LAUNCHER intent-filter (após a remoção condicional acima), garantir que não seja launcher
            // Isso é um fallback caso o template tenha uma estrutura diferente ou o regex não seja perfeito
            std::string mainActivityTag = "<activity android:name=\"" + config.packageName + ".MainActivity\"";
            if (content.find(mainActivityTag) != std::string::npos && content.find(launcherIntentFilterRegex) != std::string::npos) {
                content = std::regex_replace(content, std::regex(mainActivityTag + "[^>]*>" + launcherIntentFilterRegex), mainActivityTag + ">");
            }
        } else if (!config.backgroundOnly && !config.hideIcon) {
            // Se não for background nem hideIcon, garantir que MainActivity seja launcher (se o template já tiver o filtro)
            std::string mainActivityTag = "<activity android:name=\"" + config.packageName + ".MainActivity\"";
            if (content.find(mainActivityTag) != std::string::npos && content.find(launcherIntentFilterRegex) == std::string::npos) {
                // This part needs to be carefully handled to re-add the intent-filter if it was removed
                // For simplicity, assuming the template already has it and we only remove it if needed.
                // Re-adding it would require knowing where to insert it.
            }
        }
        // Adicionar permissões (se houver um placeholder)
        std::string permissions = generatePermissions(config);
        replaceAll(content, "<!-- PERMISSIONS_PLACEHOLDER -->", permissions);

        // Escrever arquivo modificado
        std::ofstream outputFile(manifestPath);
        outputFile << content;
        outputFile.close();

        return true;

    } catch (const std::exception& e) {
        std::cerr << "Erro ao personalizar manifest: " << e.what() << std::endl;
        return false;
    }
}

bool ApkBuilder::customizeBuildGradle(const std::string& buildDir, const ApkConfig& config) {
    std::filesystem::path gradlePath = std::filesystem::path(buildDir) /
                                      "app/build.gradle";

    if (!std::filesystem::exists(gradlePath)) {
        std::cerr << "build.gradle não encontrado" << std::endl;
        return false;
    }

    try {
        std::ifstream gradleFile(gradlePath);
        std::stringstream buffer;
        buffer << gradleFile.rdbuf();
        std::string content = buffer.str();
        gradleFile.close();

        // Substituir placeholders
        content = std::regex_replace(content, std::regex("applicationId \"[^\"]+\""), "applicationId \"" + config.packageName + "\""); // Update applicationId
        content = std::regex_replace(content, std::regex("compileSdkVersion \\d+"), "compileSdkVersion " + std::to_string(config.compileSdkVersion)); // Update compileSdkVersion
        content = std::regex_replace(content, std::regex("minSdkVersion \\d+"), "minSdkVersion " + std::to_string(config.minSdkVersion)); // Update minSdkVersion
        content = std::regex_replace(content, std::regex("targetSdkVersion \\d+"), "targetSdkVersion " + std::to_string(config.targetSdkVersion)); // Update targetSdkVersion
        content = std::regex_replace(content, std::regex("versionCode \\d+"), "versionCode " + std::to_string(config.versionCode)); // Update versionCode
        content = std::regex_replace(content, std::regex("versionName \"[^\"]+\""), "versionName \"" + config.versionName + "\""); // Update versionName

        // Configurar debug e proguard
        if (config.enableDebug) {
            content = std::regex_replace(content, std::regex("debuggable false"), "debuggable true");
        }
        if (config.enableProguard) {
            content = std::regex_replace(content, std::regex("minifyEnabled false"), "minifyEnabled true");
        }

        // Escrever arquivo modificado
        std::ofstream outputFile(gradlePath);
        outputFile << content;
        outputFile.close();

        return true;

    } catch (const std::exception& e) {
        std::cerr << "Erro ao personalizar build.gradle: " << e.what() << std::endl;
        return false;
    }
}

bool ApkBuilder::customizeStrings(const std::string& buildDir, const ApkConfig& config) {
    std::filesystem::path stringsPath = std::filesystem::path(buildDir) /
                                       "app/src/main/res/values/strings.xml";

    if (!std::filesystem::exists(stringsPath)) {
        std::cerr << "strings.xml não encontrado" << std::endl;
        return false;
    }

    try {
        std::ifstream stringsFile(stringsPath);
        std::stringstream buffer;
        buffer << stringsFile.rdbuf();
        std::string content = buffer.str();
        stringsFile.close();

        // Substituir placeholders
        content = std::regex_replace(content, std::regex("<string name=\"app_name\">[^<]+</string>"), "<string name=\"app_name\">" + config.appName + "</string>"); // Update app_name
        content = std::regex_replace(content, std::regex("public static final String SERVER_URL = \"[^\"]+\";"), "public static final String SERVER_URL = \"" + config.serverUrl + "\";");

        // Escrever arquivo modificado
        std::ofstream outputFile(stringsPath);
        outputFile << content;
        outputFile.close();

        return true;

    } catch (const std::exception& e) {
        std::cerr << "Erro ao personalizar strings.xml: " << e.what() << std::endl;
        return false;
    }
}

bool ApkBuilder::customizeJavaFiles(const std::string& buildDir, const ApkConfig& config) {
    std::filesystem::path javaDir = std::filesystem::path(buildDir) / "app/src/main/java" /
                                    std::regex_replace(config.packageName, std::regex("\\."), "/");
    
    // Renomear diretório do pacote
    std::filesystem::path oldPackageDir = std::filesystem::path(buildDir) / "app/src/main/java/com/example/template"; // Exemplo
    if (std::filesystem::exists(oldPackageDir) && oldPackageDir != javaDir) {
        std::filesystem::create_directories(javaDir.parent_path());
        std::filesystem::rename(oldPackageDir, javaDir);
    }
    // Update MainActivity.java (e.g., inject server URL, if it's still the main entry point)
    std::filesystem::path mainActivityPath = javaDir / "MainActivity.java";
    if (std::filesystem::exists(mainActivityPath)) {
        std::string content = readFile(mainActivityPath);
        content = std::regex_replace(content, std::regex("public static final String SERVER_URL = \"[^\"]+\";"), "public static final String SERVER_URL = \"" + config.serverUrl + "\";");
        writeFile(mainActivityPath, content);
        std::cout << "MainActivity.java modificado com URL do servidor." << std::endl;
    } else {
        std::cerr << "MainActivity.java não encontrado em: " << mainActivityPath << std::endl;
        return false;
    }

    // Handle WebViewActivity
    if (config.enableWebview) {
        std::filesystem::path webviewActivityPath = javaDir / "WebViewActivity.java";
        std::string webviewActivityContent = R"(
package )" + config.packageName + R"(;

import android.os.Bundle;
import androidx.appcompat.app.AppCompatActivity;
import android.webkit.WebView;
import android.webkit.WebViewClient;

public class WebViewActivity extends AppCompatActivity {

    private WebView webView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        // You might want to inflate a layout with a WebView here, or create it programmatically
        webView = new WebView(this);
        setContentView(webView);

        webView.getSettings().setJavaScriptEnabled(true);
        webView.setWebViewClient(new WebViewClient());
        webView.loadUrl(")" + config.webviewUrl + R"(");
    }
}
)";
        writeFile(webviewActivityPath, webviewActivityContent);
        std::cout << "WebViewActivity.java criado para modo espelho de site." << std::endl;

        // TODO: Create or modify the layout XML for WebViewActivity (e.g., activity_webview.xml)
        // This would involve creating a file like buildDir/app/src/main/res/layout/activity_webview.xml
        // Example content:
        // <RelativeLayout xmlns:android="http://schemas.android.com/apk/res/android"
        //     android:layout_width="match_parent"
        //     android:layout_height="match_parent">
        //     <WebView android:id="@+id/webview" android:layout_width="match_parent" android:layout_height="match_parent" />
        // </RelativeLayout>
    }

    return true;
}

std::string ApkBuilder::generatePermissions(const ApkConfig& config) {
    std::stringstream ss;

    // Permissões padrão
    ss << "    <uses-permission android:name=\"android.permission.INTERNET\" />\n";
    ss << "    <uses-permission android:name=\"android.permission.ACCESS_NETWORK_STATE\" />\n";

    // Adicionar permissões baseadas na configuração
    for (const auto& permission : config.permissions) {
        ss << "    <uses-permission android:name=\"android.permission." << permission << "\" />\n";
    }

    return ss.str();
}

std::string ApkBuilder::generateServerConfig(const ApkConfig& config) {
    std::stringstream ss;
    ss << config.serverUrl << ":" << config.serverPort;
    return ss.str();
}

std::string ApkBuilder::executeGradleBuild(const std::string& buildDir, const ApkConfig& config) {
    std::cout << "Executando build Gradle..." << std::endl;

    // Ensure platform-tools and build-tools are installed
    std::string sdkManagerCommand = androidSdkPath_ + "/cmdline-tools/latest/bin/sdkmanager --install \"platform-tools\" \"build-tools;" + std::to_string(config.compileSdkVersion) + ".0.0\" \"platforms;android-" + std::to_string(config.compileSdkVersion) + "\" > /dev/null 2>&1";
    std::cout << "Executando sdkmanager para garantir ferramentas: " << sdkManagerCommand << std::endl;
    int sdkManagerResult = std::system(sdkManagerCommand.c_str());
    if (sdkManagerResult != 0) {
        std::cerr << "Falha ao instalar/verificar ferramentas do SDK com sdkmanager. Código de saída: " << sdkManagerResult << std::endl;
        // Continue, as tools might already be there, but log warning
    }

    // Construct Gradle command
    std::string gradleCommand = "cd " + buildDir + " && ";
    gradleCommand += buildDir + "/gradlew assembleRelease"; // Always build release for production APKs

    std::cout << "Executando comando Gradle: " << gradleCommand << std::endl;

    int result = std::system(gradleCommand.c_str());
    if (result != 0) {
        std::cerr << "Falha na compilação do APK com Gradle. Código de saída: " << result << std::endl;
        return "";
    }
    std::cout << "APK compilado com sucesso pelo Gradle." << std::endl;

    // Find the generated APK
    std::filesystem::path generatedApkSource;
    std::filesystem::path releaseApkDir = std::filesystem::path(buildDir) / "app" / "build" / "outputs" / "apk" / "release";
    if (std::filesystem::exists(releaseApkDir)) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(releaseApkDir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".apk") {
                generatedApkSource = entry.path();
                break;
            }
        }
    }

    if (generatedApkSource.empty()) {
        std::cerr << "APK gerado não encontrado após a compilação em: " << releaseApkDir << std::endl;
        return "";
    }

    return generatedApkSource.string();
}

std::string ApkBuilder::signApk(const std::string& unsignedApkPath, const ApkConfig& config) {
    std::cout << "Assinando APK: " << unsignedApkPath << std::endl;
    try {
        std::filesystem::path signedApkPath = std::filesystem::path(unsignedApkPath).parent_path() /
                                              (std::filesystem::path(unsignedApkPath).stem().string() + "_signed.apk");

        // Use ApkSigner to sign the APK
        SigningConfig signingConfig;
        signingConfig.keystorePath = config.keystorePath;
        signingConfig.keystorePassword = config.keystorePass;
        signingConfig.keyAlias = config.keyAlias;
        signingConfig.keyPassword = config.keyPass;

        ApkSigner signer;
        if (!signer.signApk(unsignedApkPath, signedApkPath.string(), signingConfig)) {
            std::cerr << "Falha ao assinar APK com ApkSigner." << std::endl;
            return "";
        }

        std::cout << "APK assinado e movido para: " << signedApkPath << std::endl;
        return signedApkPath.string();

    } catch (const std::exception& e) {
        std::cerr << "Erro ao empacotar APK: " << e.what() << std::endl;
        return "";
    }
}

std::string ApkBuilder::calculateSha256(const std::string& filePath) {
    try {
        std::ifstream file(filePath, std::ios::binary);
        if (!file) {
            std::cerr << "Falha ao abrir arquivo para cálculo de hash" << std::endl;
            return "";
        }

        // Modern OpenSSL 3.0+ approach using EVP API to avoid deprecation warnings
        EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
        if (!mdctx) return "";

        const EVP_MD* md = EVP_sha256();
        EVP_DigestInit_ex(mdctx, md, nullptr);

        std::vector<char> buffer(8192);
        while (file.read(buffer.data(), buffer.size())) {
            EVP_DigestUpdate(mdctx, buffer.data(), file.gcount());
        }
        EVP_DigestUpdate(mdctx, buffer.data(), file.gcount());

        unsigned char hash[EVP_MAX_MD_SIZE];
        unsigned int hash_len;
        EVP_DigestFinal_ex(mdctx, hash, &hash_len);
        EVP_MD_CTX_free(mdctx);

        std::stringstream ss;
        for (unsigned int i = 0; i < hash_len; i++) {
            ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
        }
        return ss.str();

    } catch (const std::exception& e) {
        std::cerr << "Erro ao calcular SHA256: " << e.what() << std::endl;
        return "";
    }
}

void ApkBuilder::cleanup(const std::string& buildDir) {
    try { // Removed the flawed cleanup logic
        std::filesystem::remove_all(buildDir); // Simply remove the entire temporary build directory
        std::cout << "Diretório temporário de build limpo: " << buildDir << std::endl;
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Erro durante a limpeza do diretório de build: " << e.what() << std::endl;
    }
}

// Definições das funções utilitárias
std::string readFile(const std::filesystem::path& path) {
    std::ifstream file(path);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void writeFile(const std::filesystem::path& path, const std::string& content) {
    // Garante que o diretório pai exista
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }
    std::ofstream file(path);
    file << content;
}


} // namespace AndroidStreamManager