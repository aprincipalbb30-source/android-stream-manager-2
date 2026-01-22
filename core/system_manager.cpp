#include "system_manager.h"

namespace AndroidStreamManager {

SystemManager& SystemManager::getInstance() {
    static SystemManager instance;
    return instance;
}

SystemManager::SystemManager() 
    : threadPool(std::thread::hardware_concurrency() * 2),
      buildCache(2048), // 2GB cache
      startTime(std::chrono::system_clock::now()) {}

SystemManager::~SystemManager() {
    shutdown();
}

bool SystemManager::initialize(const std::string& configPath) {
    if (initialized) return true;
    
    try {
        // 1. Inicializar segurança
        TlsManager::getInstance().initialize(
            configPath + "/ca.crt",
            configPath + "/client.crt",
            configPath + "/client.key");
        
        JwtManager::getInstance().initialize(
            "corporate-secret-key-2024",
            std::chrono::hours(12));
        
        // 2. Inicializar conformidade
        ComplianceManager::getInstance().initialize(configPath);
        
        // 3. Inicializar assinatura
        CorporateSigningManager::getInstance().initialize(
            configPath + "/master.key",
            configPath + "/certchain.pem");

        // 4. Inicializar banco de dados
        std::string dbPath = configPath + "/stream_manager.db";
        if (!DatabaseManager::getInstance().initialize(dbPath)) {
            throw std::runtime_error("Falha ao inicializar banco de dados");
        }

        // 5. Inicializar monitoramento
        if (!metricsCollector.initialize()) {
            throw std::runtime_error("Falha ao inicializar coletor de métricas");
        }

        if (!alertsManager.initialize()) {
            throw std::runtime_error("Falha ao inicializar gerenciador de alertas");
        }

        if (!healthChecker.initialize()) {
            throw std::runtime_error("Falha ao inicializar verificador de saúde");
        }

        if (!prometheusExporter.initialize(9090)) {
            throw std::runtime_error("Falha ao inicializar exportador Prometheus");
        }

        // Configurar callbacks de alertas
        alertsManager.setAlertTriggeredCallback([](const ActiveAlert& alert) {
            std::cout << "🚨 ALERTA: [" << AlertsManager::getInstance().getStatusDescription(alert.severity)
                      << "] " << alert.message << std::endl;
        });

        // Iniciar exportador Prometheus
        if (!prometheusExporter.start()) {
            std::cout << "Aviso: Não foi possível iniciar exportador Prometheus" << std::endl;
        }

        // 6. Iniciar manutenção periódica
        threadPool.enqueue([this]() {
            while (initialized) {
                std::this_thread::sleep_for(std::chrono::minutes(5));
                buildCache.cleanup();
                JwtManager::getInstance().rotateKeys();

                // Limpeza periódica do database (registros antigos)
                DatabaseManager::getInstance().cleanupOldRecords(30); // 30 dias

                // Coletar métricas do sistema
                metricsCollector.collectSystemMetrics();

                // Verificar alertas
                alertsManager.checkAllAlerts();

                // Verificar saúde do sistema
                healthChecker.performHealthCheck();
            }
        });

        // 7. Iniciar coleta de métricas de dispositivos (mais frequente)
        threadPool.enqueue([this]() {
            while (initialized) {
                std::this_thread::sleep_for(std::chrono::seconds(30));

                // Coletar métricas de dispositivos e aplicação
                metricsCollector.collectDeviceMetrics();
                metricsCollector.collectStreamingMetrics();
                metricsCollector.collectApplicationMetrics();
            }
        });
        
        initialized = true;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "System initialization failed: " << e.what() << std::endl;
        return false;
    }
}

void SystemManager::shutdown() {
    if (!initialized) return;
    
    initialized = false;
    
    // Encerrar sessões ativas
    {
        std::lock_guard<std::mutex> lock(sessionsMutex);
        for (auto& [_, session] : activeSessions) {
            if (session.connection) {
                session.connection->disconnect();
            }
        }
        activeSessions.clear();
    }
    
    // Registrar desligamento
    ComplianceManager::getInstance().logActivity(
        "system", "SYSTEM_SHUTDOWN", "");
}

BuildResult SystemManager::buildApkWithCache(const ApkConfig& config,
                                            const std::string& operatorId) {
    
    // Verificar conformidade
    if (!ComplianceManager::getInstance().checkCompliance(config)) {
        BuildResult result;
        result.success = false;
        result.errorMessage = "Configuration does not comply with policies";
        return result;
    }
    
    // Calcular hash da configuração
    std::string configHash = buildCache.calculateConfigHash(config);
    
    // Verificar cache
    std::string cachedApk = buildCache.getBuild(configHash);
    if (!cachedApk.empty()) {
        BuildResult result;
        result.success = true;
        result.apkPath = cachedApk;
        result.buildId = "cached_" + configHash.substr(0, 8);
        
        ComplianceManager::getInstance().logActivity(
            operatorId, "BUILD_CACHE_HIT", config.appName);
        
        return result;
    }
    
    // Build novo (assíncrono)
    auto future = threadPool.enqueue([this, config, operatorId, configHash]() {
        // Executar build
        ApkBuilder builder(androidSdkPath, templatePath);
        auto result = builder.buildApk(config);
        
        if (result.success) {
            // Assinar APK
            CorporateSigningManager::getInstance().signBuild(
                result.buildId, result.sha256Hash, operatorId);
            
            // Armazenar em cache
            buildCache.storeBuild(configHash, result.apkPath, result.buildId);
            
            ComplianceManager::getInstance().logActivity(
                operatorId, "BUILD_COMPLETED", config.appName);
        }
        
        return result;
    });
    
    // Aguardar resultado (timeout de 5 minutos)
    if (future.wait_for(std::chrono::minutes(5)) == std::future_status::timeout) {
        BuildResult result;
        result.success = false;
        result.errorMessage = "Build timeout";
        return result;
    }
    
    return future.get();
}

bool SystemManager::streamData(const std::string& deviceId,
                              StreamData& data) {
    
    // Verificar sessão
    std::lock_guard<std::mutex> lock(sessionsMutex);
    auto it = activeSessions.find(deviceId);
    if (it == activeSessions.end()) {
        return false;
    }
    
    // Otimizar dados
    StreamMetrics metrics = streamOptimizer.optimizeStream(data);
    
    // Enviar através da conexão segura
    if (it->second.connection) {
        return it->second.connection->send(data.data);
    }
    
    return false;
}

SystemManager::SystemStats SystemManager::getStats() const {
    SystemStats stats;

    stats.activeSessions = activeSessions.size();
    stats.buildsInCache = buildCache.getCurrentSize();
    stats.threadsActive = threadPool.getActiveThreads();
    stats.uptime = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now() - startTime);

    // Calcular taxa de cache hit (simplificado)
    stats.cacheHitRate = 0.85; // Placeholder

    // Estatísticas do database
    auto dbStats = DatabaseManager::getInstance().getStats();
    stats.totalDevices = dbStats.totalDevices;
    stats.activeDevices = dbStats.activeDevices;
    stats.totalAuditLogs = dbStats.totalAuditLogs;
    stats.databaseSizeBytes = dbStats.databaseSizeBytes;

    return stats;
}

} // namespace AndroidStreamManager