#ifndef STREAM_OPTIMIZER_H
#define STREAM_OPTIMIZER_H

#include <vector>
#include <chrono>
#include <string>
#include <mutex>
#include <shared/protocol.h>

namespace AndroidStreamManager {

struct StreamMetrics {
    // Tamanhos
    size_t originalSize;
    size_t compressedSize;
    double compressionRatio;

    // Tempos
    std::chrono::high_resolution_clock::time_point startTime;
    std::chrono::high_resolution_clock::time_point endTime;
    std::chrono::microseconds processingTime;

    // Flags
    bool compressionUsed;
    bool deltaEncodingUsed;
    bool qualityAdjusted;
    bool isKeyframe;
    bool audioSilent;
    bool isJSON;

    // Metadados específicos
    std::string compressionAlgorithm;
    std::string frameType;
    double videoBitrate; // Mbps
    float audioVolumeRMS;
    float qualityReduction;

    StreamMetrics()
        : originalSize(0)
        , compressedSize(0)
        , compressionRatio(1.0)
        , processingTime(0)
        , compressionUsed(false)
        , deltaEncodingUsed(false)
        , qualityAdjusted(false)
        , isKeyframe(false)
        , audioSilent(false)
        , isJSON(false)
        , videoBitrate(0.0)
        , audioVolumeRMS(0.0f)
        , qualityReduction(1.0f) {}
};

struct StreamOptimizerStats {
    uint64_t totalStreamsOptimized;
    uint64_t totalOriginalBytes;
    uint64_t totalCompressedBytes;
    double averageCompressionRatio;
    double compressionEfficiency; // 0.0-1.0
    std::chrono::microseconds averageProcessingTime;

    StreamOptimizerStats()
        : totalStreamsOptimized(0)
        , totalOriginalBytes(0)
        , totalCompressedBytes(0)
        , averageCompressionRatio(1.0)
        , compressionEfficiency(0.0)
        , averageProcessingTime(0) {}
};

class StreamOptimizer {
public:
    StreamOptimizer();
    ~StreamOptimizer();

    // Otimização principal
    StreamMetrics optimizeStream(StreamData& data);

    // Configurações
    void setAdaptiveEnabled(bool enabled);
    void setDeltaEncodingEnabled(bool enabled);
    void setCompressionLevel(int level);

    // Estatísticas
    StreamOptimizerStats getStatistics() const;
    void resetStatistics();

private:
    // Métodos de otimização específicos
    void optimizeVideoData(StreamData& data, StreamMetrics& metrics);
    void optimizeAudioData(StreamData& data, StreamMetrics& metrics);
    void optimizeSensorData(StreamData& data, StreamMetrics& metrics);
    void optimizeDeviceInfo(StreamData& data, StreamMetrics& metrics);

    // Compressão
    void compressData(StreamData& data, StreamMetrics& metrics);
    void compressWithHigherRatio(StreamData& data, StreamMetrics& metrics);

    // Utilitários de análise
    bool isVideoKeyframe(const StreamData& data);
    bool isSilence(const StreamData& data);
    bool isJSONData(const StreamData& data);
    float calculateAudioRMS(const StreamData& data);
    double calculateVideoBitrate(const StreamData& data, const StreamMetrics& metrics);
    double calculateCompressionRatio(size_t original, size_t compressed);

    // Otimização adaptativa
    void adjustVideoQuality(StreamData& data, StreamMetrics& metrics);
    bool shouldReduceQuality();
    void reduceVideoQuality(StreamData& data);

    // Codificação delta
    void applyDeltaEncoding(StreamData& data, StreamMetrics& metrics);
    void compressSensorData(StreamData& data, StreamMetrics& metrics);

    // Configurações
    int compressionLevel_;
    bool adaptiveEnabled_;
    bool deltaEncodingEnabled_;

    // Estatísticas
    mutable std::mutex statsMutex_;
    uint64_t totalStreamsOptimized_;
    uint64_t totalOriginalBytes_;
    uint64_t totalCompressedBytes_;
    uint64_t compressedStreams_;
    std::chrono::microseconds totalProcessingTime_;
    // Utilitários de estatísticas
    void updateStatistics(const StreamMetrics& metrics);
};

} // namespace AndroidStreamManager

#endif // STREAM_OPTIMIZER_H
