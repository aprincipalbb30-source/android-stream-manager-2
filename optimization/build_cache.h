#ifndef BUILD_CACHE_H
#define BUILD_CACHE_H

#include <string>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <shared/apk_config.h>

namespace AndroidStreamManager {

struct CacheEntry {
    std::string configHash;
    std::string apkPath;
    std::string buildId;
    size_t fileSize;
    std::chrono::system_clock::time_point createdAt;
    std::chrono::system_clock::time_point lastAccessed;
    uint32_t accessCount;

    CacheEntry()
        : fileSize(0)
        , accessCount(0) {}
};

struct BuildCacheStats {
    size_t totalEntries;
    size_t currentSizeBytes;
    size_t maxSizeBytes;
    uint64_t cacheHits;
    uint64_t cacheMisses;
    uint64_t evictions;
    double hitRate; // 0.0-1.0
    double utilization; // 0.0-1.0
    std::chrono::seconds averageAge;
    std::chrono::seconds oldestEntry;
    std::chrono::seconds newestEntry;

    BuildCacheStats()
        : totalEntries(0)
        , currentSizeBytes(0)
        , maxSizeBytes(0)
        , cacheHits(0)
        , cacheMisses(0)
        , evictions(0)
        , hitRate(0.0)
        , utilization(0.0)
        , averageAge(0)
        , oldestEntry(0)
        , newestEntry(0) {}
};

class BuildCache {
public:
    explicit BuildCache(size_t maxSizeMB = 2048); // 2GB default
    ~BuildCache();

    // Operações principais
    std::string calculateConfigHash(const ApkConfig& config);
    bool storeBuild(const std::string& configHash, const std::string& apkPath, const std::string& buildId);
    std::string getBuild(const std::string& configHash);
    bool hasBuild(const std::string& configHash) const;
    void removeBuild(const std::string& configHash);

    // Manutenção
    void cleanup();
    void clear();

    // Configurações
    void setCleanupEnabled(bool enabled);
    void setMaxSize(size_t maxSizeMB);

    // Estatísticas
    BuildCacheStats getStatistics() const;
    size_t getCurrentSize() const;
    size_t getMaxSize() const;

private:
    // Métodos auxiliares
    void cleanupOrphanedFiles();
    void evictEntries(size_t requiredSpace);
    void updateStatistics();

    // Dados do cache
    mutable std::mutex mutex_;
    std::unordered_map<std::string, CacheEntry> cacheEntries_;
    size_t maxSizeBytes_;
    size_t currentSizeBytes_;
    bool cleanupEnabled_;

    // Estatísticas
    uint64_t cacheHits_;
    uint64_t cacheMisses_;
    uint64_t evictions_;
};

} // namespace AndroidStreamManager

#endif // BUILD_CACHE_H