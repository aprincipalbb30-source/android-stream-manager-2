#ifndef DATABASE_MANAGER_H
#define DATABASE_MANAGER_H

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <shared/protocol.h>
#include <sqlite3.h>

namespace AndroidStreamManager {

// Estrutura para informações de auditoria
struct AuditLog {
    int id;
    std::string operatorId;
    std::string action;
    std::string resource;
    std::string details;
    std::string ipAddress;
    std::chrono::system_clock::time_point timestamp;

    AuditLog()
        : id(0) {}
};

// Estrutura para sessões persistidas
struct PersistentSession {
    int id;
    std::string deviceId;
    std::string operatorId;
    std::chrono::system_clock::time_point startedAt;
    std::chrono::system_clock::time_point endedAt;
    bool active;
    std::string clientInfo;

    PersistentSession()
        : id(0)
        , active(false) {}
};

// Estrutura para dispositivos registrados
struct RegisteredDevice {
    int id;
    std::string deviceId;
    std::string deviceName;
    std::string deviceModel;
    std::string androidVersion;
    std::string registrationKey;
    std::chrono::system_clock::time_point registeredAt;
    std::chrono::system_clock::time_point lastSeenAt;
    bool active;

    RegisteredDevice()
        : id(0)
        , active(true) {}
};

// Estrutura para builds de APK
struct ApkBuild {
    int id;
    std::string buildId;
    std::string appName;
    std::string packageName;
    std::string versionName;
    int versionCode;
    std::string apkPath;
    std::string sha256Hash;
    std::string operatorId;
    std::chrono::system_clock::time_point createdAt;
    bool active;

    ApkBuild()
        : id(0)
        , versionCode(0)
        , active(true) {}
};

// Classe principal do gerenciador de banco de dados
class DatabaseManager {
public:
    static DatabaseManager& getInstance();

    // Lifecycle
    bool initialize(const std::string& dbPath = "stream_manager.db");
    void shutdown();

    // Device Management
    bool registerDevice(const RegisteredDevice& device);
    bool updateDeviceLastSeen(const std::string& deviceId);
    bool unregisterDevice(const std::string& deviceId);
    std::optional<RegisteredDevice> getDeviceById(const std::string& deviceId);
    std::vector<RegisteredDevice> getAllDevices();
    std::vector<RegisteredDevice> getActiveDevices();

    // Session Management
    bool startSession(const PersistentSession& session);
    bool endSession(int sessionId);
    bool updateSessionActivity(int sessionId);
    std::vector<PersistentSession> getActiveSessions();
    std::vector<PersistentSession> getSessionsForDevice(const std::string& deviceId);

    // Audit Logging
    bool logAuditEvent(const AuditLog& log);
    std::vector<AuditLog> getAuditLogs(int limit = 100, int offset = 0);
    std::vector<AuditLog> getAuditLogsForDevice(const std::string& deviceId);
    std::vector<AuditLog> getAuditLogsForOperator(const std::string& operatorId);

    // APK Build History
    bool saveBuildRecord(const ApkBuild& build);
    std::vector<ApkBuild> getBuildHistory(int limit = 50);
    std::vector<ApkBuild> getBuildsForOperator(const std::string& operatorId);
    std::optional<ApkBuild> getBuildById(const std::string& buildId);

    // Statistics
    struct DatabaseStats {
        int totalDevices;
        int activeDevices;
        int totalSessions;
        int activeSessions;
        int totalAuditLogs;
        int totalBuilds;
        size_t databaseSizeBytes;
    };
    DatabaseStats getStats();

    // Maintenance
    bool cleanupOldRecords(int daysOld = 90);
    bool vacuumDatabase();
    bool backupDatabase(const std::string& backupPath);

private:
    DatabaseManager();
    ~DatabaseManager();

    // SQLite internals
    sqlite3* db_;
    std::string dbPath_;

    // Prepared statements
    sqlite3_stmt* insertDeviceStmt_;
    sqlite3_stmt* updateDeviceStmt_;
    sqlite3_stmt* insertSessionStmt_;
    sqlite3_stmt* updateSessionStmt_;
    sqlite3_stmt* insertAuditStmt_;
    sqlite3_stmt* insertBuildStmt_;

    // Initialization
    bool createTables();
    bool prepareStatements();
    void finalizeStatements();

    // Utility methods
    bool executeQuery(const std::string& query);
    std::vector<std::unordered_map<std::string, std::string>> executeSelect(const std::string& query);
    bool beginTransaction();
    bool commitTransaction();
    bool rollbackTransaction();

    // Helper methods for data conversion
    RegisteredDevice rowToDevice(const std::unordered_map<std::string, std::string>& row);
    PersistentSession rowToSession(const std::unordered_map<std::string, std::string>& row);
    AuditLog rowToAuditLog(const std::unordered_map<std::string, std::string>& row);
    ApkBuild rowToBuild(const std::unordered_map<std::string, std::string>& row);

    // Error handling
    void logError(const std::string& operation, const std::string& error);

    // Non-copyable
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;
};

} // namespace AndroidStreamManager

#endif // DATABASE_MANAGER_H