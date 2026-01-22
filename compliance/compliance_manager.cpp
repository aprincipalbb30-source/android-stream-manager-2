#include "compliance/compliance_manager.h"
#include <iostream>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace Compliance {

ComplianceManager::ComplianceManager() {
    // Initialize with some dummy data if needed
}

std::string ComplianceManager::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&now_c), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

void ComplianceManager::logAction(const std::string& userId, const std::string& action, const std::string& details) {
    std::lock_guard<std::mutex> lock(auditMutex);
    auditLogs.push_back({getCurrentTimestamp(), userId, action, details});
    std::cout << "[AUDIT] " << getCurrentTimestamp() << " User: " << userId << ", Action: " << action << ", Details: " << details << std::endl;
}

std::vector<AuditLogEntry> ComplianceManager::getAuditLogs(const std::string& userId, int limit) {
    std::lock_guard<std::mutex> lock(auditMutex);
    std::vector<AuditLogEntry> filteredLogs;
    int count = 0;
    for (auto it = auditLogs.rbegin(); it != auditLogs.rend() && count < limit; ++it) {
        if (userId.empty() || it->userId == userId) {
            filteredLogs.push_back(*it);
            count++;
        }
    }
    return filteredLogs;
}

bool ComplianceManager::getUserConsent(const std::string& userId) const {
    std::lock_guard<std::mutex> lock(consentMutex);
    auto it = userConsents.find(userId);
    return (it != userConsents.end() && it->second);
}

void ComplianceManager::setUserConsent(const std::string& userId, bool consent) {
    std::lock_guard<std::mutex> lock(consentMutex);
    userConsents[userId] = consent;
    logAction(userId, "Consent Update", "Consent set to " + std::string(consent ? "true" : "false"));
}

void ComplianceManager::recordSessionStart(const std::string& userId, const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(sessionMutex);
    activeSessions[userId][deviceId] = std::chrono::system_clock::now();
    logAction(userId, "Session Start", "Device: " + deviceId);
}

void ComplianceManager::recordSessionEnd(const std::string& userId, const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(sessionMutex);
    auto it = activeSessions.find(userId);
    if (it != activeSessions.end()) {
        it->second.erase(deviceId);
        if (it->second.empty()) {
            activeSessions.erase(it);
        }
    }
    logAction(userId, "Session End", "Device: " + deviceId);
}

bool ComplianceManager::isSessionActive(const std::string& userId, const std::string& deviceId) const {
    std::lock_guard<std::mutex> lock(sessionMutex);
    auto userIt = activeSessions.find(userId);
    if (userIt != activeSessions.end()) {
        auto deviceIt = userIt->second.find(deviceId);
        return (deviceIt != userIt->second.end());
    }
    return false;
}

} // namespace Compliance
