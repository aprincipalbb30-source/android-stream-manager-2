#include "apk_builder.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <openssl/sha.h>
#include <chrono>
#include <thread>

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

ApkBuilder::~ApkBuilder() {
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

        // 4. Personalizar arquivos
        if (!customizeFiles(buildDir, config)) {
            result.errorMessage = "Falha ao personalizar arquivos";
            return result;
        }

        // 5. Compilar recursos
        if (!compileResources(buildDir)) {
            result.errorMessage = "Falha ao compilar recursos";
            return result;
        }

        // 6. Gerar bytecode
        if (!generateBytecode(buildDir)) {
            result.errorMessage = "Falha ao gerar bytecode";
            return result;
        }

        // 7. Empacotar APK
        std::string apkPath = packageApk(buildDir, config);
        if (apkPath.empty()) {
            result.errorMessage = "Falha ao empacotar APK";
            return result;
        }

        // 8. Calcular hash do APK
        result.sha256Hash = calculateSha256(apkPath);
        if (result.sha256Hash.empty()) {
            result.errorMessage = "Falha ao calcular hash SHA256";
            return result;
        }

        // 9. Configurar resultado
        result.success = true;
        result.apkPath = apkPath;
        result.buildId = buildId;
        result.buildTime = std::chrono::system_clock::now();

        std::cout << "Build concluído com sucesso: " << apkPath << std::endl;

        // 10. Limpar arquivos temporários
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

bool ApkBuilder::customizeFiles(const std::string& buildDir, const ApkConfig& config) {
    // Personalizar AndroidManifest.xml
    if (!customizeManifest(buildDir, config)) {
        return false;
    }

    // Personalizar build.gradle
    if (!customizeBuildGradle(buildDir, config)) {
        return false;
    }

    // Personalizar strings.xml
    if (!customizeStrings(buildDir, config)) {
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
        replaceAll(content, "{{PACKAGE_NAME}}", config.packageName);
        replaceAll(content, "{{APP_NAME}}", config.appName);
        replaceAll(content, "{{APP_ICON}}", config.iconPath.empty() ? "@android:drawable/ic_launcher" : config.iconPath);
        replaceAll(content, "{{SERVER_CONFIG}}", generateServerConfig(config));

        // Adicionar permissões
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
        replaceAll(content, "{{PACKAGE_NAME}}", config.packageName);
        replaceAll(content, "{{COMPILE_SDK_VERSION}}", std::to_string(config.compileSdkVersion));
        replaceAll(content, "{{MIN_SDK_VERSION}}", std::to_string(config.minSdkVersion));
        replaceAll(content, "{{TARGET_SDK_VERSION}}", std::to_string(config.targetSdkVersion));
        replaceAll(content, "{{VERSION_CODE}}", std::to_string(config.versionCode));
        replaceAll(content, "{{VERSION_NAME}}", config.versionName);

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
        replaceAll(content, "{{APP_NAME}}", config.appName);
        replaceAll(content, "{{SERVER_URL}}", config.serverUrl);

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

bool ApkBuilder::compileResources(const std::string& buildDir) {
    std::cout << "Compilando recursos..." << std::endl;

    // Simular compilação de recursos
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::cout << "Recursos compilados" << std::endl;
    return true;
}

bool ApkBuilder::generateBytecode(const std::string& buildDir) {
    std::cout << "Gerando bytecode..." << std::endl;

    // Simular geração de bytecode
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    std::cout << "Bytecode gerado" << std::endl;
    return true;
}

std::string ApkBuilder::packageApk(const std::string& buildDir, const ApkConfig& config) {
    std::cout << "Empacotando APK..." << std::endl;

    try {
        // Gerar nome do APK
        std::string apkName = config.packageName + "_" + std::to_string(config.versionCode) + ".apk";
        std::filesystem::path apkPath = std::filesystem::path(buildDir) / apkName;

        // Simular criação do APK (em produção usaria Android SDK)
        std::ofstream apkFile(apkPath, std::ios::binary);
        if (!apkFile) {
            std::cerr << "Falha ao criar arquivo APK" << std::endl;
            return "";
        }

        // Escrever dados simulados do APK
        std::string dummyData = "APK_DUMMY_DATA_FOR_" + config.appName;
        apkFile.write(dummyData.c_str(), dummyData.size());
        apkFile.close();

        std::cout << "APK criado: " << apkPath << std::endl;
        return apkPath.string();

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

        SHA256_CTX sha256;
        SHA256_Init(&sha256);

        char buffer[8192];
        while (file.read(buffer, sizeof(buffer))) {
            SHA256_Update(&sha256, buffer, file.gcount());
        }
        SHA256_Update(&sha256, buffer, file.gcount());

        unsigned char hash[SHA256_DIGEST_LENGTH];
        SHA256_Final(hash, &sha256);

        std::stringstream ss;
        for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
            ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
        }

        return ss.str();

    } catch (const std::exception& e) {
        std::cerr << "Erro ao calcular SHA256: " << e.what() << std::endl;
        return "";
    }
}

void ApkBuilder::cleanup(const std::string& buildDir) {
    try {
        // Remover arquivos temporários (manter apenas o APK final)
        std::filesystem::path buildPath(buildDir);
        std::filesystem::path apkDir = buildPath.parent_path();

        // Manter apenas arquivos .apk
        for (const auto& entry : std::filesystem::recursive_directory_iterator(buildPath)) {
            if (entry.is_regular_file() && entry.path().extension() != ".apk") {
                std::filesystem::remove(entry.path());
            }
        }

        std::cout << "Limpeza concluída" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Erro durante limpeza: " << e.what() << std::endl;
    }
}

void ApkBuilder::replaceAll(std::string& str, const std::string& from, const std::string& to) {
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
}

} // namespace AndroidStreamManager