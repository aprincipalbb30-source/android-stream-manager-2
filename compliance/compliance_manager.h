#ifndef COMPLIANCE_MANAGER_H
#define COMPLIANCE_MANAGER_H

#include <string>
#include <vector>
#include <mutex>
#include <map>
#include <chrono>
#include "shared/apk_config.h"

namespace AndroidStreamManager {

struct AuditLogEntry {
    std::string timestamp;
    std::string userId;
    std::string action;
    std::string details;
};

class ComplianceManager {
public:
    static ComplianceManager& getInstance();

    bool initialize(const std::string& configPath);

    // Audit methods
    void logActivity(const std::string& operatorId, const std::string& activity, const std::string& details);
    void logAction(const std::string& userId, const std::string& action, const std::string& details);
    std::vector<AuditLogEntry> getAuditLogs(const std::string& userId = "", int limit = 100);

    // Consent management
    bool getUserConsent(const std::string& userId) const;
    void setUserConsent(const std::string& userId, bool consent);

    // Session management
    void recordSessionStart(const std::string& userId, const std::string& deviceId);
    void recordSessionEnd(const std::string& userId, const std::string& deviceId);
    bool isSessionActive(const std::string& userId, const std::string& deviceId) const;

    bool checkCompliance(const ApkConfig& config);

private:
    ComplianceManager();
    ~ComplianceManager() = default;
    ComplianceManager(const ComplianceManager&) = delete;
    ComplianceManager& operator=(const ComplianceManager&) = delete;

    std::vector<AuditLogEntry> auditLogs;
    mutable std::mutex auditMutex;

    std::map<std::string, bool> userConsents;
    std::map<std::string, std::map<std::string, std::chrono::time_point<std::chrono::system_clock>>> activeSessions;
    mutable std::mutex consentMutex;
    mutable std::mutex sessionMutex;

    std::string getCurrentTimestamp();
};
 
} // namespace AndroidStreamManager

#endif // COMPLIANCE_MANAGER_H
