#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <filesystem>
#include "core/system_manager.h"
#include "core/apk_builder.h"
#include "optimization/thread_pool.h"
#include "optimization/stream_optimizer.h"
#include "optimization/build_cache.h"
#include "database/database_manager.h"

using namespace AndroidStreamManager;

void testThreadPool() {
    std::cout << "\n=== Teste ThreadPool ===" << std::endl;

    ThreadPool pool(4);

    // Teste básico de enqueue
    auto future1 = pool.enqueue([]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return 42;
    });

    auto future2 = pool.enqueue([]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        return "Hello World";
    });

    // Aguardar resultados
    std::cout << "Resultado 1: " << future1.get() << std::endl;
    std::cout << "Resultado 2: " << future2.get() << std::endl;

    // Estatísticas
    auto stats = pool.getStatistics();
    std::cout << "Tasks processadas: " << stats.totalTasksProcessed << std::endl;
    std::cout << "Threads ativas: " << stats.activeThreads << std::endl;

    pool.waitForAllTasks();
}

void testStreamOptimizer() {
    std::cout << "\n=== Teste StreamOptimizer ===" << std::endl;

    StreamOptimizer optimizer;

    // Criar dados de teste (vídeo simulada)
    StreamData videoData;
    videoData.dataType = StreamData::DataType::VIDEO_H264;
    videoData.data.resize(10000); // 10KB de dados simulados

    // Preencher com dados simulados
    for (size_t i = 0; i < videoData.data.size(); ++i) {
        videoData.data[i] = static_cast<uint8_t>(i % 256);
    }

    // Otimizar
    StreamMetrics metrics = optimizer.optimizeStream(videoData);

    std::cout << "Tamanho original: " << metrics.originalSize << " bytes" << std::endl;
    std::cout << "Tamanho comprimido: " << metrics.compressedSize << " bytes" << std::endl;
    std::cout << "Razão de compressão: " << metrics.compressionRatio << std::endl;
    std::cout << "Tempo de processamento: " << metrics.processingTime.count() << " μs" << std::endl;
    std::cout << "Compressão usada: " << (metrics.compressionUsed ? "Sim" : "Não") << std::endl;

    // Estatísticas gerais
    auto stats = optimizer.getStatistics();
    std::cout << "Total de streams otimizados: " << stats.totalStreamsOptimized << std::endl;
}

void testBuildCache() {
    std::cout << "\n=== Teste BuildCache ===" << std::endl;

    BuildCache cache(100); // 100MB

    // Criar configuração de teste
    ApkConfig config;
    config.appName = "TestApp";
    config.packageName = "com.test.app";
    config.versionCode = 1;
    config.serverUrl = "wss://test-server.com";

    // Calcular hash
    std::string hash = cache.calculateConfigHash(config);
    std::cout << "Hash da configuração: " << hash.substr(0, 16) << "..." << std::endl;

    // Criar arquivo APK simulado
    std::string apkPath = "/tmp/test_app.apk";
    {
        std::ofstream apkFile(apkPath, std::ios::binary);
        std::string dummyData = "APK_DUMMY_DATA_FOR_TESTING";
        apkFile.write(dummyData.c_str(), dummyData.size());
    }

    // Armazenar em cache
    bool stored = cache.storeBuild(hash, apkPath, "build_123");
    std::cout << "Build armazenado: " << (stored ? "Sim" : "Não") << std::endl;

    // Verificar cache
    std::string cachedPath = cache.getBuild(hash);
    std::cout << "Build encontrado no cache: " << (!cachedPath.empty() ? "Sim" : "Não") << std::endl;

    // Estatísticas
    auto stats = cache.getStatistics();
    std::cout << "Entradas no cache: " << stats.totalEntries << std::endl;
    std::cout << "Taxa de acertos: " << (stats.hitRate * 100) << "%" << std::endl;

    // Limpar arquivo de teste
    std::remove(apkPath.c_str());
}

void testApkBuilder() {
    std::cout << "\n=== Teste ApkBuilder ===" << std::endl;

    // Criar APK builder
    ApkBuilder builder("/opt/android-sdk", "./templates");

    // Configuração de teste
    ApkConfig config;
    config.appName = "Demo App";
    config.packageName = "com.demo.app";
    config.versionName = "1.0.0";
    config.versionCode = 1;
    config.minSdkVersion = 23;
    config.targetSdkVersion = 33;
    config.compileSdkVersion = 33;
    config.serverUrl = "wss://demo-server.com:8443/ws";
    config.iconPath = "@android:drawable/ic_launcher";
    config.theme = "Theme.AppCompat.Light";
    config.enableDebug = false;
    config.enableProguard = false;

    config.addCommonPermissions();

    std::cout << "Configuração válida: " << (config.isValid() ? "Sim" : "Não") << std::endl;

    // Calcular hash da configuração
    std::string hash = builder.calculateConfigHash(config);
    std::cout << "Hash da configuração: " << hash.substr(0, 16) << "..." << std::endl;

    // Nota: Build real requer template Android, então apenas testamos validação
    std::cout << "Build real requer template Android SDK configurado" << std::endl;
}

void testDatabaseManager() {
    std::cout << "\n=== Teste DatabaseManager ===" << std::endl;

    // Criar arquivo temporário para teste
    std::string testDbPath = "/tmp/test_stream_manager.db";
    std::filesystem::remove(testDbPath); // Remover se existir

    // Inicializar database
    if (!DatabaseManager::getInstance().initialize(testDbPath)) {
        std::cerr << "Falha ao inicializar database para teste" << std::endl;
        return;
    }

    std::cout << "Database inicializado com sucesso" << std::endl;

    // Testar registro de dispositivo
    AndroidStreamManager::RegisteredDevice device;
    device.deviceId = "test_device_001";
    device.deviceName = "Test Device";
    device.deviceModel = "TestModel";
    device.androidVersion = "13.0";
    device.registrationKey = "test_key_123";
    device.active = true;

    if (DatabaseManager::getInstance().registerDevice(device)) {
        std::cout << "✓ Dispositivo registrado com sucesso" << std::endl;
    } else {
        std::cout << "✗ Falha ao registrar dispositivo" << std::endl;
    }

    // Testar busca de dispositivo
    auto foundDevice = DatabaseManager::getInstance().getDeviceById("test_device_001");
    if (foundDevice) {
        std::cout << "✓ Dispositivo encontrado: " << foundDevice->deviceName << std::endl;
    } else {
        std::cout << "✗ Dispositivo não encontrado" << std::endl;
    }

    // Testar log de auditoria
    AndroidStreamManager::AuditLog auditLog;
    auditLog.operatorId = "test_operator";
    auditLog.action = "DEVICE_REGISTER";
    auditLog.resource = "test_device_001";
    auditLog.details = "Device registration test";
    auditLog.ipAddress = "127.0.0.1";

    if (DatabaseManager::getInstance().logAuditEvent(auditLog)) {
        std::cout << "✓ Log de auditoria registrado" << std::endl;
    } else {
        std::cout << "✗ Falha ao registrar log de auditoria" << std::endl;
    }

    // Testar estatísticas
    auto stats = DatabaseManager::getInstance().getStats();
    std::cout << "✓ Estatísticas - Dispositivos: " << stats.totalDevices
              << ", Logs: " << stats.totalAuditLogs
              << ", Tamanho DB: " << stats.databaseSizeBytes << " bytes" << std::endl;

    // Limpar arquivo de teste
    DatabaseManager::getInstance().shutdown();
    std::filesystem::remove(testDbPath);
    std::cout << "Database de teste limpo" << std::endl;
}

int main() {
    std::cout << "=== Testes do Android Stream Manager ===" << std::endl;
    std::cout << "Testando componentes implementados..." << std::endl;

    try {
    // Executar testes
    testThreadPool();
    testStreamOptimizer();
    testBuildCache();
    testApkBuilder();
    testDatabaseManager();

        std::cout << "\n=== Todos os testes concluídos ===" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Erro durante testes: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}