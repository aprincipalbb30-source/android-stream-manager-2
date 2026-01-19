#include "database_manager.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <algorithm>

namespace AndroidStreamManager {

DatabaseManager& DatabaseManager::getInstance() {
    static DatabaseManager instance;
    return instance;
}

DatabaseManager::DatabaseManager()
    : db_(nullptr)
    , insertDeviceStmt_(nullptr)
    , updateDeviceStmt_(nullptr)
    , insertSessionStmt_(nullptr)
    , updateSessionStmt_(nullptr)
    , insertAuditStmt_(nullptr)
    , insertBuildStmt_(nullptr) {
    std::cout << "DatabaseManager criado" << std::endl;
}

DatabaseManager::~DatabaseManager() {
    shutdown();
    std::cout << "DatabaseManager destruído" << std::endl;
}

bool DatabaseManager::initialize(const std::string& dbPath) {
    dbPath_ = dbPath;

    // Abrir conexão SQLite
    int result = sqlite3_open(dbPath_.c_str(), &db_);
    if (result != SQLITE_OK) {
        std::cerr << "Erro ao abrir banco de dados: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }

    // Habilitar WAL mode para melhor performance
    executeQuery("PRAGMA journal_mode=WAL;");
    executeQuery("PRAGMA synchronous=NORMAL;");
    executeQuery("PRAGMA cache_size=-2000;"); // 2MB cache

    // Criar tabelas se não existirem
    if (!createTables()) {
        std::cerr << "Erro ao criar tabelas do banco de dados" << std::endl;
        return false;
    }

    // Preparar statements
    if (!prepareStatements()) {
        std::cerr << "Erro ao preparar statements" << std::endl;
        return false;
    }

    std::cout << "DatabaseManager inicializado com sucesso: " << dbPath_ << std::endl;
    return true;
}

void DatabaseManager::shutdown() {
    if (!db_) {
        return;
    }

    // Finalizar statements preparados
    finalizeStatements();

    // Fechar conexão
    sqlite3_close(db_);
    db_ = nullptr;

    std::cout << "DatabaseManager finalizado" << std::endl;
}

bool DatabaseManager::createTables() {
    const std::vector<std::string> createQueries = {
        // Tabela de dispositivos registrados
        "CREATE TABLE IF NOT EXISTS devices ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "device_id TEXT UNIQUE NOT NULL,"
        "device_name TEXT,"
        "device_model TEXT,"
        "android_version TEXT,"
        "registration_key TEXT,"
        "registered_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "last_seen_at DATETIME,"
        "active BOOLEAN DEFAULT 1"
        ");",

        // Tabela de sessões
        "CREATE TABLE IF NOT EXISTS sessions ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "device_id TEXT NOT NULL,"
        "operator_id TEXT,"
        "started_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "ended_at DATETIME,"
        "active BOOLEAN DEFAULT 1,"
        "client_info TEXT,"
        "FOREIGN KEY(device_id) REFERENCES devices(device_id)"
        ");",

        // Tabela de auditoria
        "CREATE TABLE IF NOT EXISTS audit_logs ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "operator_id TEXT,"
        "action TEXT NOT NULL,"
        "resource TEXT,"
        "details TEXT,"
        "ip_address TEXT,"
        "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");",

        // Tabela de builds de APK
        "CREATE TABLE IF NOT EXISTS apk_builds ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "build_id TEXT UNIQUE NOT NULL,"
        "app_name TEXT,"
        "package_name TEXT,"
        "version_name TEXT,"
        "version_code INTEGER,"
        "apk_path TEXT,"
        "sha256_hash TEXT,"
        "operator_id TEXT,"
        "created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "active BOOLEAN DEFAULT 1"
        ");",

        // Índices para performance
        "CREATE INDEX IF NOT EXISTS idx_devices_device_id ON devices(device_id);",
        "CREATE INDEX IF NOT EXISTS idx_sessions_device_id ON sessions(device_id);",
        "CREATE INDEX IF NOT EXISTS idx_sessions_active ON sessions(active);",
        "CREATE INDEX IF NOT EXISTS idx_audit_operator ON audit_logs(operator_id);",
        "CREATE INDEX IF NOT EXISTS idx_audit_timestamp ON audit_logs(timestamp);",
        "CREATE INDEX IF NOT EXISTS idx_builds_build_id ON apk_builds(build_id);",
        "CREATE INDEX IF NOT EXISTS idx_builds_operator ON apk_builds(operator_id);"
    };

    for (const auto& query : createQueries) {
        if (!executeQuery(query)) {
            return false;
        }
    }

    std::cout << "Tabelas do banco de dados criadas/verificada" << std::endl;
    return true;
}

bool DatabaseManager::prepareStatements() {
    // INSERT device
    const char* insertDeviceSQL = "INSERT OR REPLACE INTO devices "
                                 "(device_id, device_name, device_model, android_version, registration_key, last_seen_at, active) "
                                 "VALUES (?, ?, ?, ?, ?, ?, ?);";

    if (sqlite3_prepare_v2(db_, insertDeviceSQL, -1, &insertDeviceStmt_, nullptr) != SQLITE_OK) {
        logError("prepare insertDeviceStmt", sqlite3_errmsg(db_));
        return false;
    }

    // UPDATE device last seen
    const char* updateDeviceSQL = "UPDATE devices SET last_seen_at = ? WHERE device_id = ?;";

    if (sqlite3_prepare_v2(db_, updateDeviceSQL, -1, &updateDeviceStmt_, nullptr) != SQLITE_OK) {
        logError("prepare updateDeviceStmt", sqlite3_errmsg(db_));
        return false;
    }

    // INSERT session
    const char* insertSessionSQL = "INSERT INTO sessions "
                                  "(device_id, operator_id, client_info) "
                                  "VALUES (?, ?, ?);";

    if (sqlite3_prepare_v2(db_, insertSessionSQL, -1, &insertSessionStmt_, nullptr) != SQLITE_OK) {
        logError("prepare insertSessionStmt", sqlite3_errmsg(db_));
        return false;
    }

    // UPDATE session
    const char* updateSessionSQL = "UPDATE sessions SET ended_at = ?, active = 0 WHERE id = ?;";

    if (sqlite3_prepare_v2(db_, updateSessionSQL, -1, &updateSessionStmt_, nullptr) != SQLITE_OK) {
        logError("prepare updateSessionStmt", sqlite3_errmsg(db_));
        return false;
    }

    // INSERT audit log
    const char* insertAuditSQL = "INSERT INTO audit_logs "
                                "(operator_id, action, resource, details, ip_address) "
                                "VALUES (?, ?, ?, ?, ?);";

    if (sqlite3_prepare_v2(db_, insertAuditSQL, -1, &insertAuditStmt_, nullptr) != SQLITE_OK) {
        logError("prepare insertAuditStmt", sqlite3_errmsg(db_));
        return false;
    }

    // INSERT build
    const char* insertBuildSQL = "INSERT INTO apk_builds "
                                "(build_id, app_name, package_name, version_name, version_code, apk_path, sha256_hash, operator_id) "
                                "VALUES (?, ?, ?, ?, ?, ?, ?, ?);";

    if (sqlite3_prepare_v2(db_, insertBuildSQL, -1, &insertBuildStmt_, nullptr) != SQLITE_OK) {
        logError("prepare insertBuildStmt", sqlite3_errmsg(db_));
        return false;
    }

    std::cout << "Statements preparados com sucesso" << std::endl;
    return true;
}

void DatabaseManager::finalizeStatements() {
    if (insertDeviceStmt_) sqlite3_finalize(insertDeviceStmt_);
    if (updateDeviceStmt_) sqlite3_finalize(updateDeviceStmt_);
    if (insertSessionStmt_) sqlite3_finalize(insertSessionStmt_);
    if (updateSessionStmt_) sqlite3_finalize(updateSessionStmt_);
    if (insertAuditStmt_) sqlite3_finalize(insertAuditStmt_);
    if (insertBuildStmt_) sqlite3_finalize(insertBuildStmt_);
}

// ========== DEVICE MANAGEMENT ==========

bool DatabaseManager::registerDevice(const RegisteredDevice& device) {
    if (!insertDeviceStmt_) return false;

    sqlite3_reset(insertDeviceStmt_);

    // Converter timestamp para string
    auto timestamp = std::chrono::system_clock::to_time_t(device.lastSeenAt);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&timestamp), "%Y-%m-%d %H:%M:%S");

    sqlite3_bind_text(insertDeviceStmt_, 1, device.deviceId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(insertDeviceStmt_, 2, device.deviceName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(insertDeviceStmt_, 3, device.deviceModel.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(insertDeviceStmt_, 4, device.androidVersion.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(insertDeviceStmt_, 5, device.registrationKey.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(insertDeviceStmt_, 6, ss.str().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(insertDeviceStmt_, 7, device.active ? 1 : 0);

    int result = sqlite3_step(insertDeviceStmt_);
    if (result != SQLITE_DONE) {
        logError("registerDevice", sqlite3_errmsg(db_));
        return false;
    }

    return true;
}

bool DatabaseManager::updateDeviceLastSeen(const std::string& deviceId) {
    if (!updateDeviceStmt_) return false;

    sqlite3_reset(updateDeviceStmt_);

    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&timestamp), "%Y-%m-%d %H:%M:%S");

    sqlite3_bind_text(updateDeviceStmt_, 1, ss.str().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(updateDeviceStmt_, 2, deviceId.c_str(), -1, SQLITE_TRANSIENT);

    int result = sqlite3_step(updateDeviceStmt_);
    if (result != SQLITE_DONE) {
        logError("updateDeviceLastSeen", sqlite3_errmsg(db_));
        return false;
    }

    return true;
}

std::optional<RegisteredDevice> DatabaseManager::getDeviceById(const std::string& deviceId) {
    std::string query = "SELECT * FROM devices WHERE device_id = '" + deviceId + "';";
    auto rows = executeSelect(query);

    if (rows.empty()) {
        return std::nullopt;
    }

    return rowToDevice(rows[0]);
}

std::vector<RegisteredDevice> DatabaseManager::getAllDevices() {
    std::string query = "SELECT * FROM devices ORDER BY last_seen_at DESC;";
    auto rows = executeSelect(query);

    std::vector<RegisteredDevice> devices;
    devices.reserve(rows.size());

    for (const auto& row : rows) {
        devices.push_back(rowToDevice(row));
    }

    return devices;
}

// ========== SESSION MANAGEMENT ==========

bool DatabaseManager::startSession(const PersistentSession& session) {
    if (!insertSessionStmt_) return false;

    sqlite3_reset(insertSessionStmt_);

    sqlite3_bind_text(insertSessionStmt_, 1, session.deviceId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(insertSessionStmt_, 2, session.operatorId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(insertSessionStmt_, 3, session.clientInfo.c_str(), -1, SQLITE_TRANSIENT);

    int result = sqlite3_step(insertSessionStmt_);
    if (result != SQLITE_DONE) {
        logError("startSession", sqlite3_errmsg(db_));
        return false;
    }

    return true;
}

bool DatabaseManager::endSession(int sessionId) {
    if (!updateSessionStmt_) return false;

    sqlite3_reset(updateSessionStmt_);

    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&timestamp), "%Y-%m-%d %H:%M:%S");

    sqlite3_bind_text(updateSessionStmt_, 1, ss.str().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(updateSessionStmt_, 2, sessionId);

    int result = sqlite3_step(updateSessionStmt_);
    if (result != SQLITE_DONE) {
        logError("endSession", sqlite3_errmsg(db_));
        return false;
    }

    return true;
}

std::vector<PersistentSession> DatabaseManager::getActiveSessions() {
    std::string query = "SELECT * FROM sessions WHERE active = 1 ORDER BY started_at DESC;";
    auto rows = executeSelect(query);

    std::vector<PersistentSession> sessions;
    sessions.reserve(rows.size());

    for (const auto& row : rows) {
        sessions.push_back(rowToSession(row));
    }

    return sessions;
}

// ========== AUDIT LOGGING ==========

bool DatabaseManager::logAuditEvent(const AuditLog& log) {
    if (!insertAuditStmt_) return false;

    sqlite3_reset(insertAuditStmt_);

    sqlite3_bind_text(insertAuditStmt_, 1, log.operatorId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(insertAuditStmt_, 2, log.action.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(insertAuditStmt_, 3, log.resource.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(insertAuditStmt_, 4, log.details.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(insertAuditStmt_, 5, log.ipAddress.c_str(), -1, SQLITE_TRANSIENT);

    int result = sqlite3_step(insertAuditStmt_);
    if (result != SQLITE_DONE) {
        logError("logAuditEvent", sqlite3_errmsg(db_));
        return false;
    }

    return true;
}

std::vector<AuditLog> DatabaseManager::getAuditLogs(int limit, int offset) {
    std::string query = "SELECT * FROM audit_logs ORDER BY timestamp DESC LIMIT " +
                       std::to_string(limit) + " OFFSET " + std::to_string(offset) + ";";
    auto rows = executeSelect(query);

    std::vector<AuditLog> logs;
    logs.reserve(rows.size());

    for (const auto& row : rows) {
        logs.push_back(rowToAuditLog(row));
    }

    return logs;
}

// ========== APK BUILD HISTORY ==========

bool DatabaseManager::saveBuildRecord(const ApkBuild& build) {
    if (!insertBuildStmt_) return false;

    sqlite3_reset(insertBuildStmt_);

    sqlite3_bind_text(insertBuildStmt_, 1, build.buildId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(insertBuildStmt_, 2, build.appName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(insertBuildStmt_, 3, build.packageName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(insertBuildStmt_, 4, build.versionName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(insertBuildStmt_, 5, build.versionCode);
    sqlite3_bind_text(insertBuildStmt_, 6, build.apkPath.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(insertBuildStmt_, 7, build.sha256Hash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(insertBuildStmt_, 8, build.operatorId.c_str(), -1, SQLITE_TRANSIENT);

    int result = sqlite3_step(insertBuildStmt_);
    if (result != SQLITE_DONE) {
        logError("saveBuildRecord", sqlite3_errmsg(db_));
        return false;
    }

    return true;
}

std::vector<ApkBuild> DatabaseManager::getBuildHistory(int limit) {
    std::string query = "SELECT * FROM apk_builds ORDER BY created_at DESC LIMIT " + std::to_string(limit) + ";";
    auto rows = executeSelect(query);

    std::vector<ApkBuild> builds;
    builds.reserve(rows.size());

    for (const auto& row : rows) {
        builds.push_back(rowToBuild(row));
    }

    return builds;
}

// ========== STATISTICS ==========

DatabaseManager::DatabaseStats DatabaseManager::getStats() {
    DatabaseStats stats = {};

    // Contar dispositivos
    auto deviceRows = executeSelect("SELECT COUNT(*) as count FROM devices;");
    if (!deviceRows.empty()) {
        stats.totalDevices = std::stoi(deviceRows[0].at("count"));
    }

    auto activeDeviceRows = executeSelect("SELECT COUNT(*) as count FROM devices WHERE active = 1;");
    if (!activeDeviceRows.empty()) {
        stats.activeDevices = std::stoi(activeDeviceRows[0].at("count"));
    }

    // Contar sessões
    auto sessionRows = executeSelect("SELECT COUNT(*) as count FROM sessions;");
    if (!sessionRows.empty()) {
        stats.totalSessions = std::stoi(sessionRows[0].at("count"));
    }

    auto activeSessionRows = executeSelect("SELECT COUNT(*) as count FROM sessions WHERE active = 1;");
    if (!activeSessionRows.empty()) {
        stats.activeSessions = std::stoi(activeSessionRows[0].at("count"));
    }

    // Contar logs de auditoria
    auto auditRows = executeSelect("SELECT COUNT(*) as count FROM audit_logs;");
    if (!auditRows.empty()) {
        stats.totalAuditLogs = std::stoi(auditRows[0].at("count"));
    }

    // Contar builds
    auto buildRows = executeSelect("SELECT COUNT(*) as count FROM apk_builds;");
    if (!buildRows.empty()) {
        stats.totalBuilds = std::stoi(buildRows[0].at("count"));
    }

    // Tamanho do banco
    if (std::filesystem::exists(dbPath_)) {
        stats.databaseSizeBytes = std::filesystem::file_size(dbPath_);
    }

    return stats;
}

// ========== UTILITY METHODS ==========

bool DatabaseManager::executeQuery(const std::string& query) {
    char* errorMsg = nullptr;
    int result = sqlite3_exec(db_, query.c_str(), nullptr, nullptr, &errorMsg);

    if (result != SQLITE_OK) {
        std::cerr << "Erro SQL: " << errorMsg << std::endl;
        sqlite3_free(errorMsg);
        return false;
    }

    return true;
}

std::vector<std::unordered_map<std::string, std::string>> DatabaseManager::executeSelect(const std::string& query) {
    std::vector<std::unordered_map<std::string, std::string>> results;

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        logError("executeSelect prepare", sqlite3_errmsg(db_));
        return results;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::unordered_map<std::string, std::string> row;

        int columnCount = sqlite3_column_count(stmt);
        for (int i = 0; i < columnCount; ++i) {
            const char* columnName = sqlite3_column_name(stmt, i);
            const char* columnValue = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));

            if (columnName && columnValue) {
                row[columnName] = columnValue;
            }
        }

        results.push_back(row);
    }

    sqlite3_finalize(stmt);
    return results;
}

// ========== CONVERSION HELPERS ==========

RegisteredDevice DatabaseManager::rowToDevice(const std::unordered_map<std::string, std::string>& row) {
    RegisteredDevice device;

    device.id = std::stoi(row.at("id"));
    device.deviceId = row.at("device_id");
    device.deviceName = row.count("device_name") ? row.at("device_name") : "";
    device.deviceModel = row.count("device_model") ? row.at("device_model") : "";
    device.androidVersion = row.count("android_version") ? row.at("android_version") : "";
    device.registrationKey = row.count("registration_key") ? row.at("registration_key") : "";
    device.active = std::stoi(row.at("active")) != 0;

    // Parse timestamps
    if (row.count("registered_at")) {
        // TODO: Parse timestamp string to time_point
    }
    if (row.count("last_seen_at")) {
        // TODO: Parse timestamp string to time_point
    }

    return device;
}

PersistentSession DatabaseManager::rowToSession(const std::unordered_map<std::string, std::string>& row) {
    PersistentSession session;

    session.id = std::stoi(row.at("id"));
    session.deviceId = row.at("device_id");
    session.operatorId = row.count("operator_id") ? row.at("operator_id") : "";
    session.active = std::stoi(row.at("active")) != 0;
    session.clientInfo = row.count("client_info") ? row.at("client_info") : "";

    // TODO: Parse timestamps

    return session;
}

AuditLog DatabaseManager::rowToAuditLog(const std::unordered_map<std::string, std::string>& row) {
    AuditLog log;

    log.id = std::stoi(row.at("id"));
    log.operatorId = row.count("operator_id") ? row.at("operator_id") : "";
    log.action = row.at("action");
    log.resource = row.count("resource") ? row.at("resource") : "";
    log.details = row.count("details") ? row.at("details") : "";
    log.ipAddress = row.count("ip_address") ? row.at("ip_address") : "";

    // TODO: Parse timestamp

    return log;
}

ApkBuild DatabaseManager::rowToBuild(const std::unordered_map<std::string, std::string>& row) {
    ApkBuild build;

    build.id = std::stoi(row.at("id"));
    build.buildId = row.at("build_id");
    build.appName = row.count("app_name") ? row.at("app_name") : "";
    build.packageName = row.count("package_name") ? row.at("package_name") : "";
    build.versionName = row.count("version_name") ? row.at("version_name") : "";
    build.versionCode = std::stoi(row.at("version_code"));
    build.apkPath = row.count("apk_path") ? row.at("apk_path") : "";
    build.sha256Hash = row.count("sha256_hash") ? row.at("sha256_hash") : "";
    build.operatorId = row.count("operator_id") ? row.at("operator_id") : "";
    build.active = std::stoi(row.at("active")) != 0;

    // TODO: Parse timestamp

    return build;
}

// ========== MAINTENANCE ==========

bool DatabaseManager::cleanupOldRecords(int daysOld) {
    std::string query = "DELETE FROM audit_logs WHERE timestamp < datetime('now', '-" +
                       std::to_string(daysOld) + " days');";

    return executeQuery(query);
}

bool DatabaseManager::vacuumDatabase() {
    return executeQuery("VACUUM;");
}

bool DatabaseManager::backupDatabase(const std::string& backupPath) {
    std::string query = "VACUUM INTO '" + backupPath + "';";
    return executeQuery(query);
}

// ========== ERROR HANDLING ==========

void DatabaseManager::logError(const std::string& operation, const std::string& error) {
    std::cerr << "Database error in " << operation << ": " << error << std::endl;
}

// ========== UNIMPLEMENTED METHODS (stubs) ==========

bool DatabaseManager::unregisterDevice(const std::string& deviceId) {
    std::string query = "UPDATE devices SET active = 0 WHERE device_id = '" + deviceId + "';";
    return executeQuery(query);
}

std::vector<RegisteredDevice> DatabaseManager::getActiveDevices() {
    std::string query = "SELECT * FROM devices WHERE active = 1 ORDER BY last_seen_at DESC;";
    auto rows = executeSelect(query);

    std::vector<RegisteredDevice> devices;
    for (const auto& row : rows) {
        devices.push_back(rowToDevice(row));
    }
    return devices;
}

bool DatabaseManager::updateSessionActivity(int sessionId) {
    // Stub - implementar se necessário
    return true;
}

std::vector<PersistentSession> DatabaseManager::getSessionsForDevice(const std::string& deviceId) {
    std::string query = "SELECT * FROM sessions WHERE device_id = '" + deviceId +
                       "' ORDER BY started_at DESC;";
    auto rows = executeSelect(query);

    std::vector<PersistentSession> sessions;
    for (const auto& row : rows) {
        sessions.push_back(rowToSession(row));
    }
    return sessions;
}

std::vector<AuditLog> DatabaseManager::getAuditLogsForDevice(const std::string& deviceId) {
    std::string query = "SELECT * FROM audit_logs WHERE resource LIKE '%" + deviceId +
                       "%' ORDER BY timestamp DESC LIMIT 100;";
    auto rows = executeSelect(query);

    std::vector<AuditLog> logs;
    for (const auto& row : rows) {
        logs.push_back(rowToAuditLog(row));
    }
    return logs;
}

std::vector<AuditLog> DatabaseManager::getAuditLogsForOperator(const std::string& operatorId) {
    std::string query = "SELECT * FROM audit_logs WHERE operator_id = '" + operatorId +
                       "' ORDER BY timestamp DESC LIMIT 100;";
    auto rows = executeSelect(query);

    std::vector<AuditLog> logs;
    for (const auto& row : rows) {
        logs.push_back(rowToAuditLog(row));
    }
    return logs;
}

std::vector<ApkBuild> DatabaseManager::getBuildsForOperator(const std::string& operatorId) {
    std::string query = "SELECT * FROM apk_builds WHERE operator_id = '" + operatorId +
                       "' ORDER BY created_at DESC;";
    auto rows = executeSelect(query);

    std::vector<ApkBuild> builds;
    for (const auto& row : rows) {
        builds.push_back(rowToBuild(row));
    }
    return builds;
}

std::optional<ApkBuild> DatabaseManager::getBuildById(const std::string& buildId) {
    std::string query = "SELECT * FROM apk_builds WHERE build_id = '" + buildId + "';";
    auto rows = executeSelect(query);

    if (rows.empty()) {
        return std::nullopt;
    }

    return rowToBuild(rows[0]);
}

bool DatabaseManager::beginTransaction() {
    return executeQuery("BEGIN TRANSACTION;");
}

bool DatabaseManager::commitTransaction() {
    return executeQuery("COMMIT;");
}

bool DatabaseManager::rollbackTransaction() {
    return executeQuery("ROLLBACK;");
}

} // namespace AndroidStreamManager