#include "build_cache.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <openssl/sha256.h>
#include <algorithm>
#include <chrono>

namespace AndroidStreamManager {

BuildCache::BuildCache(size_t maxSizeMB)
    : maxSizeBytes_(maxSizeMB * 1024 * 1024)
    , currentSizeBytes_(0)
    , cleanupEnabled_(true) {

    std::cout << "BuildCache inicializado com tamanho máximo: "
              << maxSizeMB << "MB (" << maxSizeBytes_ << " bytes)" << std::endl;
}

BuildCache::~BuildCache() {
    cleanup();
    std::cout << "BuildCache destruído" << std::endl;
}

std::string BuildCache::calculateConfigHash(const ApkConfig& config) {
    // Criar representação string da configuração
    std::stringstream ss;

    ss << config.appName << "|"
       << config.packageName << "|"
       << config.versionName << "|"
       << config.versionCode << "|"
       << config.minSdkVersion << "|"
       << config.targetSdkVersion << "|"
       << config.compileSdkVersion << "|"
       << config.serverUrl << "|"
       << config.serverPort << "|"
       << config.iconPath << "|"
       << config.theme << "|"
       << config.enableDebug << "|"
       << config.enableProguard;

    // Adicionar permissões (ordenadas para consistência)
    std::vector<std::string> sortedPermissions = config.permissions;
    std::sort(sortedPermissions.begin(), sortedPermissions.end());
    for (const auto& perm : sortedPermissions) {
        ss << "|" << perm;
    }

    // Calcular SHA256 do string
    std::string configStr = ss.str();
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, configStr.c_str(), configStr.size());
    SHA256_Final(hash, &sha256);

    // Converter para string hexadecimal
    std::stringstream hashSS;
    hashSS << std::hex << std::setfill('0');
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        hashSS << std::setw(2) << static_cast<int>(hash[i]);
    }

    return hashSS.str();
}

bool BuildCache::storeBuild(const std::string& configHash,
                           const std::string& apkPath,
                           const std::string& buildId) {
    std::lock_guard<std::mutex> lock(mutex_);

    try {
        // Verificar se arquivo APK existe
        if (!std::filesystem::exists(apkPath)) {
            std::cerr << "Arquivo APK não encontrado: " << apkPath << std::endl;
            return false;
        }

        // Obter tamanho do arquivo
        size_t fileSize = std::filesystem::file_size(apkPath);

        // Verificar se cabe no cache
        if (fileSize > maxSizeBytes_) {
            std::cerr << "Arquivo muito grande para cache: " << fileSize << " bytes" << std::endl;
            return false;
        }

        // Criar entrada de cache
        CacheEntry entry;
        entry.configHash = configHash;
        entry.apkPath = apkPath;
        entry.buildId = buildId;
        entry.fileSize = fileSize;
        entry.createdAt = std::chrono::system_clock::now();
        entry.lastAccessed = entry.createdAt;
        entry.accessCount = 0;

        // Verificar se já existe entrada
        auto it = cacheEntries_.find(configHash);
        if (it != cacheEntries_.end()) {
            // Remover entrada antiga
            currentSizeBytes_ -= it->second.fileSize;
            cacheEntries_.erase(it);
        }

        // Fazer espaço se necessário
        if (currentSizeBytes_ + fileSize > maxSizeBytes_) {
            evictEntries(fileSize);
        }

        // Adicionar nova entrada
        cacheEntries_[configHash] = entry;
        currentSizeBytes_ += fileSize;

        // Atualizar estatísticas
        cacheHits_++;
        updateStatistics();

        std::cout << "Build armazenado em cache: " << configHash.substr(0, 8)
                  << " (" << fileSize << " bytes)" << std::endl;

        return true;

    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Erro ao armazenar build em cache: " << e.what() << std::endl;
        return false;
    }
}

std::string BuildCache::getBuild(const std::string& configHash) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = cacheEntries_.find(configHash);
    if (it == cacheEntries_.end()) {
        cacheMisses_++;
        return "";
    }

    // Verificar se arquivo ainda existe
    if (!std::filesystem::exists(it->second.apkPath)) {
        std::cerr << "Arquivo em cache não encontrado: " << it->second.apkPath << std::endl;
        // Remover entrada inválida
        currentSizeBytes_ -= it->second.fileSize;
        cacheEntries_.erase(it);
        cacheMisses_++;
        return "";
    }

    // Atualizar estatísticas de acesso
    it->second.lastAccessed = std::chrono::system_clock::now();
    it->second.accessCount++;

    cacheHits_++;

    std::cout << "Cache hit: " << configHash.substr(0, 8) << std::endl;

    return it->second.apkPath;
}

bool BuildCache::hasBuild(const std::string& configHash) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cacheEntries_.find(configHash) != cacheEntries_.end();
}

void BuildCache::removeBuild(const std::string& configHash) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = cacheEntries_.find(configHash);
    if (it != cacheEntries_.end()) {
        // Remover arquivo se existir
        try {
            if (std::filesystem::exists(it->second.apkPath)) {
                std::filesystem::remove(it->second.apkPath);
            }
        } catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "Erro ao remover arquivo: " << e.what() << std::endl;
        }

        currentSizeBytes_ -= it->second.fileSize;
        cacheEntries_.erase(it);

        std::cout << "Build removido do cache: " << configHash.substr(0, 8) << std::endl;
    }
}

void BuildCache::cleanup() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!cleanupEnabled_) {
        return;
    }

    std::cout << "Executando limpeza de cache..." << std::endl;

    auto now = std::chrono::system_clock::now();
    auto maxAge = std::chrono::hours(24 * 30); // 30 dias
    auto minAccessAge = std::chrono::hours(24 * 7); // 7 dias sem acesso

    std::vector<std::string> entriesToRemove;

    for (const auto& pair : cacheEntries_) {
        const CacheEntry& entry = pair.second;
        auto age = now - entry.createdAt;
        auto accessAge = now - entry.lastAccessed;

        bool shouldRemove = false;

        // Remover se muito velho
        if (age > maxAge) {
            shouldRemove = true;
        }
        // Remover se não acessado recentemente e cache está cheio
        else if (accessAge > minAccessAge &&
                 currentSizeBytes_ > maxSizeBytes_ * 0.8) { // 80% capacity
            shouldRemove = true;
        }

        if (shouldRemove) {
            entriesToRemove.push_back(pair.first);
        }
    }

    // Remover entradas selecionadas
    for (const auto& hash : entriesToRemove) {
        removeBuild(hash);
    }

    // Remover arquivos órfãos (APKs sem entrada no cache)
    cleanupOrphanedFiles();

    std::cout << "Limpeza concluída. " << entriesToRemove.size() << " entradas removidas" << std::endl;
}

void BuildCache::cleanupOrphanedFiles() {
    try {
        // Procurar por arquivos APK no diretório de cache
        // (implementação simplificada - em produção seria mais robusta)

        for (const auto& pair : cacheEntries_) {
            const CacheEntry& entry = pair.second;

            // Verificar se arquivo existe
            if (!std::filesystem::exists(entry.apkPath)) {
                std::cerr << "Arquivo órfão detectado: " << entry.apkPath << std::endl;
                // Remover entrada do cache
                currentSizeBytes_ -= entry.fileSize;
                cacheEntries_.erase(pair.first);
                break; // Iterator invalidado, sair do loop
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "Erro durante limpeza de arquivos órfãos: " << e.what() << std::endl;
    }
}

void BuildCache::evictEntries(size_t requiredSpace) {
    if (cacheEntries_.empty()) {
        return;
    }

    // Estratégia LRU (Least Recently Used) com peso de frequência
    std::vector<std::pair<std::string, double>> evictionCandidates;

    for (const auto& pair : cacheEntries_) {
        const CacheEntry& entry = pair.second;
        auto accessAge = std::chrono::system_clock::now() - entry.lastAccessed;

        // Calcular score: combinação de idade e frequência
        double ageHours = std::chrono::duration_cast<std::chrono::hours>(accessAge).count();
        double frequency = entry.accessCount;
        double sizePenalty = static_cast<double>(entry.fileSize) / (1024.0 * 1024.0); // MB

        // Score: quanto menor, mais provável de ser removido
        double score = frequency / (ageHours + 1.0) - sizePenalty;

        evictionCandidates.emplace_back(pair.first, score);
    }

    // Ordenar por score (menor primeiro)
    std::sort(evictionCandidates.begin(), evictionCandidates.end(),
              [](const auto& a, const auto& b) { return a.second < b.second; });

    // Remover entradas até ter espaço suficiente
    size_t freedSpace = 0;
    for (const auto& candidate : evictionCandidates) {
        if (freedSpace >= requiredSpace) {
            break;
        }

        const auto& hash = candidate.first;
        auto it = cacheEntries_.find(hash);
        if (it != cacheEntries_.end()) {
            freedSpace += it->second.fileSize;
            removeBuild(hash);
            evictions_++;
        }
    }

    std::cout << "Evict: liberado " << freedSpace << " bytes" << std::endl;
}

BuildCacheStats BuildCache::getStatistics() const {
    std::lock_guard<std::mutex> lock(mutex_);

    BuildCacheStats stats;
    stats.totalEntries = cacheEntries_.size();
    stats.currentSizeBytes = currentSizeBytes_;
    stats.maxSizeBytes = maxSizeBytes_;
    stats.cacheHits = cacheHits_;
    stats.cacheMisses = cacheMisses_;
    stats.evictions = evictions_;

    // Calcular hit rate
    uint64_t totalRequests = cacheHits_ + cacheMisses_;
    stats.hitRate = totalRequests > 0 ? static_cast<double>(cacheHits_) / totalRequests : 0.0;

    // Calcular utilization
    stats.utilization = maxSizeBytes_ > 0 ? static_cast<double>(currentSizeBytes_) / maxSizeBytes_ : 0.0;

    // Estatísticas de idade
    if (!cacheEntries_.empty()) {
        auto now = std::chrono::system_clock::now();
        stats.averageAge = std::chrono::seconds(0);
        stats.oldestEntry = std::chrono::seconds(0);
        stats.newestEntry = std::chrono::hours(24*365); // 1 ano

        for (const auto& pair : cacheEntries_) {
            const CacheEntry& entry = pair.second;
            auto age = now - entry.createdAt;
            auto ageSeconds = std::chrono::duration_cast<std::chrono::seconds>(age);

            stats.averageAge += ageSeconds;
            stats.oldestEntry = std::max(stats.oldestEntry, ageSeconds);
            stats.newestEntry = std::min(stats.newestEntry, ageSeconds);
        }

        stats.averageAge /= cacheEntries_.size();
    }

    return stats;
}

void BuildCache::clear() {
    std::lock_guard<std::mutex> lock(mutex_);

    for (const auto& pair : cacheEntries_) {
        try {
            if (std::filesystem::exists(pair.second.apkPath)) {
                std::filesystem::remove(pair.second.apkPath);
            }
        } catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "Erro ao remover arquivo durante limpeza: " << e.what() << std::endl;
        }
    }

    cacheEntries_.clear();
    currentSizeBytes_ = 0;
    cacheHits_ = 0;
    cacheMisses_ = 0;
    evictions_ = 0;

    std::cout << "Cache limpo completamente" << std::endl;
}

void BuildCache::setCleanupEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    cleanupEnabled_ = enabled;
}

void BuildCache::setMaxSize(size_t maxSizeMB) {
    std::lock_guard<std::mutex> lock(mutex_);
    maxSizeBytes_ = maxSizeMB * 1024 * 1024;

    // Se tamanho reduzido, pode ser necessário fazer limpeza
    if (currentSizeBytes_ > maxSizeBytes_) {
        std::cout << "Tamanho de cache reduzido, executando limpeza..." << std::endl;
        cleanup();
    }
}

size_t BuildCache::getCurrentSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return currentSizeBytes_;
}

size_t BuildCache::getMaxSize() const {
    return maxSizeBytes_;
}

void BuildCache::updateStatistics() {
    // Estatísticas atualizadas automaticamente nos getters
}

} // namespace AndroidStreamManager