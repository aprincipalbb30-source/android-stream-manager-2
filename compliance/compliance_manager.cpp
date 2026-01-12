#include "compliance/compliance_manager.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>

namespace AndroidStreamManager {

ComplianceManager& ComplianceManager::getInstance() {
    static ComplianceManager instance;
    return instance;
}

ComplianceManager::ComplianceManager() {
    // Carregar configurações de conformidade
    loadComplianceRules();
}

void ComplianceManager::initialize(const std::string& configPath) {
    std::lock_guard<std::mutex> lock(mutex);
    
    // Carregar configurações do arquivo
    std::ifstream configFile(configPath);
    if (configFile) {
        nlohmann::json config;
        configFile >> config;
        
        if (config.contains("maxSessionDuration")) {
            maxSessionDuration = std::chrono::hours(
                config["maxSessionDuration"]);
        }
        
        if (config.contains("requireExplicitConsent")) {
            requireExplicitConsent = config["requireExplicitConsent"];
        }
        
        if (config.contains("dataRetentionDays")) {
            dataRetentionDays = config["dataRetentionDays"];
        }
    }
    
    // Iniciar limpeza periódica
    cleanupThread = std::thread([this]() {
        while (cleanupRunning) {
            std::this_thread::sleep_for(std::chrono::hours(1));
            cleanupOldData();
        }
    });
}

bool ComplianceManager::verifyUserConsent(const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(mutex);
    
    auto it = consentRecords.find(deviceId);
    if (it == consentRecords.end()) {
        return false;
    }
    
    // Verificar se o consentimento ainda é válido
    auto now = std::chrono::system_clock::now();
    if (now - it->second.timestamp > std::chrono::hours(24)) {
        consentRecords.erase(it);
        return false;
    }
    
    return it->second.granted;
}

void ComplianceManager::recordConsent(const std::string& deviceId,
                                     const std::string& userId,
                                     bool granted,
                                     const std::string& consentText) {
    std::lock_guard<std::mutex> lock(mutex);
    
    ConsentRecord record;
    record.deviceId = deviceId;
    record.userId = userId;
    record.granted = granted;
    record.consentText = consentText;
    record.timestamp = std::chrono::system_clock::now();
    record.ipAddress = "127.0.0.1"; // Obter IP real
    
    consentRecords[deviceId] = record;
    
    // Registrar no log de auditoria
    logActivity(userId, 
               granted ? "CONSENT_GRANTED" : "CONSENT_DENIED", 
               deviceId);
}

void ComplianceManager::logActivity(const std::string& operatorId,
                                   const std::string& action,
                                   const std::string& target) {
    std::lock_guard<std::mutex> lock(mutex);
    
    AuditLogEntry entry;
    entry.operatorId = operatorId;
    entry.action = action;
    entry.target = target;
    entry.timestamp = std::chrono::system_clock::now();
    entry.ipAddress = "127.0.0.1"; // Obter IP real
    
    auditLog.push_back(entry);
    
    // Persistir no arquivo
    persistAuditLog(entry);
}

bool ComplianceManager::checkCompliance(const ApkConfig& config) {
    // Verificar regras de conformidade
    if (config.permissions.empty()) {
        logActivity("system", "COMPLIANCE_CHECK_FAILED", 
                   "No permissions specified");
        return false;
    }
    
    // Verificar se há permissões sensíveis sem justificativa
    bool hasSensitivePermission = false;
    for (const auto& perm : config.permissions) {
        if (perm == Permission::CAMERA || 
            perm == Permission::MICROPHONE ||
            perm == Permission::LOCATION) {
            hasSensitivePermission = true;
            break;
        }
    }
    
    if (hasSensitivePermission && config.persistenceEnabled) {
        // Requer revisão adicional
        logActivity("system", "SENSITIVE_FEATURE_ENABLED", 
                   config.appName);
        return true; // Permitir com auditoria
    }
    
    // Verificar nome do pacote
    if (config.packageName.find("com.google") != std::string::npos ||
        config.packageName.find("com.android") != std::string::npos) {
        logActivity("system", "INVALID_PACKAGE_NAME", 
                   config.packageName);
        return false;
    }
    
    return true;
}

void ComplianceManager::cleanupOldData() {
    std::lock_guard<std::mutex> lock(mutex);
    
    auto now = std::chrono::system_clock::now();
    auto cutoff = now - std::chrono::days(dataRetentionDays);
    
    // Limpar registros de consentimento antigos
    for (auto it = consentRecords.begin(); it != consentRecords.end();) {
        if (it->second.timestamp < cutoff) {
            it = consentRecords.erase(it);
        } else {
            ++it;
        }
    }
    
    // Limpar log de auditoria (mantém em arquivo)
    auditLog.erase(
        std::remove_if(auditLog.begin(), auditLog.end(),
            [cutoff](const AuditLogEntry& entry) {
                return entry.timestamp < cutoff;
            }),
        auditLog.end()
    );
}

void ComplianceManager::persistAuditLog(const AuditLogEntry& entry) {
    std::ofstream logFile("audit.log", std::ios::app);
    if (!logFile) return;
    
    auto time = std::chrono::system_clock::to_time_t(entry.timestamp);
    std::tm tm = *std::localtime(&time);
    
    logFile << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << " | "
            << entry.operatorId << " | "
            << entry.action << " | "
            << entry.target << " | "
            << entry.ipAddress << std::endl;
}

std::vector<AuditLogEntry> ComplianceManager::getAuditLog(
    const std::chrono::system_clock::time_point& start,
    const std::chrono::system_clock::time_point& end) const {
    
    std::vector<AuditLogEntry> result;
    std::lock_guard<std::mutex> lock(mutex);
    
    std::copy_if(auditLog.begin(), auditLog.end(),
                std::back_inserter(result),
                [start, end](const AuditLogEntry& entry) {
                    return entry.timestamp >= start && 
                           entry.timestamp <= end;
                });
    
    return result;
}

} // namespace AndroidStreamManager