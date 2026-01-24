#include "stream_optimizer.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace AndroidStreamManager {

StreamOptimizer::StreamOptimizer()
    : adaptiveEnabled_(true) {
    std::cout << "StreamOptimizer inicializado" << std::endl;
}

StreamOptimizer::~StreamOptimizer() {
    std::cout << "StreamOptimizer destruído" << std::endl;
}

StreamMetrics StreamOptimizer::optimizeStream(StreamData& data) {
    StreamMetrics metrics;
    metrics.originalSize = data.data.size();
    metrics.startTime = std::chrono::high_resolution_clock::now();
    
    // Detectar tipo de dados e otimizar adequadamente
    switch (data.dataType) {
        case StreamData::DataType::VIDEO_H264:
        case StreamData::DataType::VIDEO_H265:
            optimizeVideoData(data, metrics);
            break;
            
        case StreamData::DataType::AUDIO_AAC:
        case StreamData::DataType::AUDIO_OPUS:
            optimizeAudioData(data, metrics);
            break;

        case StreamData::DataType::SENSOR_DATA:
            optimizeSensorData(data, metrics);
            break;

        case StreamData::DataType::DEVICE_INFO:
            optimizeDeviceInfo(data, metrics);
            break;
            
        default:
            // Compressão genérica
            compressData(data, metrics);
            break;
    }
    
    metrics.endTime = std::chrono::high_resolution_clock::now();
    metrics.compressedSize = data.data.size();
    metrics.compressionRatio = calculateCompressionRatio(metrics.originalSize, metrics.compressedSize);
    metrics.processingTime = std::chrono::duration_cast<std::chrono::microseconds>(
        metrics.endTime - metrics.startTime);

    updateStatistics(metrics);

    return metrics;
}

void StreamOptimizer::optimizeVideoData(StreamData& data, StreamMetrics& metrics) {
    // Otimizações específicas para vídeo

    // 1. Verificar se é keyframe (mais importante)
    bool isKeyframe = isVideoKeyframe(data);
    metrics.isKeyframe = isKeyframe;

    // 2. Ajustar qualidade baseada na largura de banda
    if (adaptiveEnabled_) {
        adjustVideoQuality(data, metrics);
    }

    // 3. Compressão LZ4 para dados de vídeo
    compressData(data, metrics);

    // 4. Estatísticas específicas de vídeo
    metrics.frameType = isKeyframe ? "keyframe" : "pframe";
    metrics.videoBitrate = calculateVideoBitrate(data, metrics);
}

void StreamOptimizer::optimizeAudioData(StreamData& data, StreamMetrics& metrics) {
    // Otimizações específicas para áudio

    // 1. Detectar silêncio e comprimir
    if (isSilence(data)) {
        metrics.audioSilent = true;
        // Dados de silêncio podem ser altamente comprimidos
        compressWithHigherRatio(data, metrics);
    } else {
        metrics.audioSilent = false;
        compressData(data, metrics);
    }

    // 2. Calcular volume RMS
    metrics.audioVolumeRMS = calculateAudioRMS(data);
}

void StreamOptimizer::optimizeSensorData(StreamData& data, StreamMetrics& metrics) {
    // Otimizações para dados de sensores

    // 1. Delta encoding para sensores (apenas mudanças significativas)
    if (deltaEncodingEnabled_) {
        applyDeltaEncoding(data, metrics);
    }

    // 2. Compressão de ponto flutuante
    compressSensorData(data, metrics);
}

void StreamOptimizer::optimizeDeviceInfo(StreamData& data, StreamMetrics& metrics) {
    // Dados de informação do dispositivo geralmente são texto
    // Boa compressão com LZ4

    // 1. Verificar se são dados JSON (alta compressão)
    if (isJSONData(data)) {
        metrics.isJSON = true;
        compressData(data, metrics);
    } else {
        metrics.isJSON = false;
        // Compressão menor para dados binários
        compressData(data, metrics);
    }
}

void StreamOptimizer::compressData(StreamData& data, StreamMetrics& metrics) {
    if (data.data.empty()) {
        return;
    }

    // Preparar buffer de saída
    const size_t maxCompressedSize = LZ4_compressBound(data.data.size());
    std::vector<uint8_t> compressedData(maxCompressedSize);

    // Comprimir usando LZ4
    const int compressedSize = LZ4_compress_default(
        reinterpret_cast<const char*>(data.data.data()),
        reinterpret_cast<char*>(compressedData.data()),
        static_cast<int>(data.data.size()),
        static_cast<int>(maxCompressedSize)
    );

    if (compressedSize > 0 && compressedSize < static_cast<int>(data.data.size())) {
        // Compressão bem-sucedida
        compressedData.resize(compressedSize);
        data.data = std::move(compressedData);
        metrics.compressionUsed = true;
        metrics.compressionAlgorithm = "LZ4";
    } else {
        // Compressão não foi benéfica
        metrics.compressionUsed = false;
    }
}

void StreamOptimizer::compressWithHigherRatio(StreamData& data, StreamMetrics& metrics) {
    if (data.data.empty()) {
        return;
    }

    const size_t maxCompressedSize = LZ4_compressBound(data.data.size());
    std::vector<uint8_t> compressedData(maxCompressedSize);

    const int compressedSize = LZ4_compress_HC(
        reinterpret_cast<const char*>(data.data.data()),
        reinterpret_cast<char*>(compressedData.data()),
        static_cast<int>(data.data.size()),
        static_cast<int>(maxCompressedSize),
        LZ4HC_CLEVEL_MAX
    );

    if (compressedSize > 0) {
        compressedData.resize(compressedSize);
        data.data = std::move(compressedData);
        metrics.compressionUsed = true;
        metrics.compressionAlgorithm = "LZ4_HC";
    }
}

bool StreamOptimizer::isVideoKeyframe(const StreamData& data) {
    // Detecção simples de keyframe (implementação básica)
    // Em produção, analisaria headers H.264/H.265
    if (data.data.size() < 4) {
        return false;
    }

    // Verificar padrões comuns de keyframe
    // H.264 NAL unit type 5 (IDR frame)
    if (data.data[0] == 0x00 && data.data[1] == 0x00 &&
        data.data[2] == 0x00 && data.data[3] == 0x01) {
        uint8_t nalType = data.data[4] & 0x1F;
        return nalType == 5; // IDR frame
    }

    return false;
}

void StreamOptimizer::adjustVideoQuality(StreamData& data, StreamMetrics& metrics) {
    // Ajuste adaptativo baseado em condições de rede
    // (simplificado - em produção usaria dados reais de rede)

    static int frameCount = 0;
    frameCount++;

    // Simular ajuste periódico de qualidade
    if (frameCount % 30 == 0) { // A cada 30 frames
        metrics.qualityAdjusted = true;

        // Simular redução de qualidade se "rede lenta"
        if (shouldReduceQuality()) {
            reduceVideoQuality(data);
            metrics.qualityReduction = 0.8f; // 20% redução
        }
    } else {
        metrics.qualityAdjusted = false;
    }
}

bool StreamOptimizer::shouldReduceQuality() {
    // Simulação: reduzir qualidade a cada 5 segundos aproximadamente
    static int counter = 0;
    counter++;
    return (counter % 150) == 0; // ~5 segundos a 30fps
}

void StreamOptimizer::reduceVideoQuality(StreamData& data) {
    // Implementação simplificada de redução de qualidade
    // Em produção, re-encoderia o vídeo ou ajustaria parâmetros

    // Simular redução removendo alguns bytes (não realista, apenas para demonstração)
    if (data.data.size() > 1000) {
        size_t reduction = data.data.size() / 20; // Reduzir 5%
        data.data.resize(data.data.size() - reduction);
    }
}

bool StreamOptimizer::isSilence(const StreamData& data) {
    // Detecção simples de silêncio
    if (data.data.empty()) {
        return true;
    }

    // Para dados PCM, verificar se todos os valores estão próximos de zero
    const int16_t* samples = reinterpret_cast<const int16_t*>(data.data.data());
    size_t sampleCount = data.data.size() / sizeof(int16_t);

    int silentSamples = 0;
    for (size_t i = 0; i < sampleCount; ++i) {
        if (std::abs(samples[i]) < 100) { // Threshold arbitrário
            silentSamples++;
        }
    }

    // Considerar silêncio se >90% das amostras são silenciosas
    return (silentSamples * 100.0f / sampleCount) > 90.0f;
}

float StreamOptimizer::calculateAudioRMS(const StreamData& data) {
    if (data.data.size() < sizeof(int16_t)) {
        return 0.0f;
    }

    const int16_t* samples = reinterpret_cast<const int16_t*>(data.data.data());
    size_t sampleCount = data.data.size() / sizeof(int16_t);

    double sum = 0.0;
    for (size_t i = 0; i < sampleCount; ++i) {
        double sample = samples[i] / 32768.0; // Normalizar para [-1, 1]
        sum += sample * sample;
    }

    return static_cast<float>(std::sqrt(sum / sampleCount));
}

void StreamOptimizer::applyDeltaEncoding(StreamData& data, StreamMetrics& metrics) {
    // Codificação delta para sensores (apenas mudanças)
    // Implementação simplificada

    if (data.data.size() < 8) {
        return;
    }

    // Assumir dados de float (4 bytes por valor)
    const float* input = reinterpret_cast<const float*>(data.data.data());
    size_t valueCount = data.data.size() / sizeof(float);

    std::vector<float> deltas;
    deltas.reserve(valueCount);

    float previousValue = 0.0f;
    for (size_t i = 0; i < valueCount; ++i) {
        float delta = input[i] - previousValue;
        deltas.push_back(delta);
        previousValue = input[i];
    }

    // Codificar deltas (simplificado)
    data.data.resize(deltas.size() * sizeof(float));
    std::memcpy(data.data.data(), deltas.data(), data.data.size());

    metrics.deltaEncodingUsed = true;
}

void StreamOptimizer::compressSensorData(StreamData& data, StreamMetrics& metrics) {
    // Compressão específica para dados de sensores
    // Usar compressão mais agressiva pois sensores geram muitos dados
    compressWithHigherRatio(data, metrics);
}

bool StreamOptimizer::isJSONData(const StreamData& data) {
    if (data.data.empty()) {
        return false;
    }

    // Verificar se começa com '{' (JSON object) ou '[' (JSON array)
    return data.data[0] == '{' || data.data[0] == '[';
}

double StreamOptimizer::calculateVideoBitrate(const StreamData& data, const StreamMetrics& metrics) {
    // Calcular bitrate aproximado
    // Assumir 30fps como padrão
    double bitsPerSecond = (metrics.compressedSize * 8.0 * 30.0);
    return bitsPerSecond / 1000000.0; // Mbps
}

double StreamOptimizer::calculateCompressionRatio(size_t original, size_t compressed) {
    if (original == 0) {
        return 1.0;
    }
    return static_cast<double>(compressed) / static_cast<double>(original);
}

void StreamOptimizer::updateStatistics(const StreamMetrics& metrics) {
    std::lock_guard<std::mutex> lock(statsMutex_);

    totalStreamsOptimized_++;
    totalOriginalBytes_ += metrics.originalSize;
    totalCompressedBytes_ += metrics.compressedSize;
    totalProcessingTime_ += metrics.processingTime;

    if (metrics.compressionUsed) {
        compressedStreams_++;
    }
}

StreamOptimizerStats StreamOptimizer::getStatistics() const {
    std::lock_guard<std::mutex> lock(statsMutex_);

    StreamOptimizerStats stats;
    stats.totalStreamsOptimized = totalStreamsOptimized_;
    stats.totalOriginalBytes = totalOriginalBytes_;
    stats.totalCompressedBytes = totalCompressedBytes_;
    stats.averageCompressionRatio = totalOriginalBytes_ > 0 ?
        static_cast<double>(totalCompressedBytes_) / totalOriginalBytes_ : 1.0;
    stats.compressionEfficiency = totalStreamsOptimized_ > 0 ?
        static_cast<double>(compressedStreams_) / totalStreamsOptimized_ : 0.0;

    if (totalStreamsOptimized_ > 0) {
        stats.averageProcessingTime = totalProcessingTime_ / totalStreamsOptimized_;
    }

    return stats;
}

void StreamOptimizer::resetStatistics() {
    std::lock_guard<std::mutex> lock(statsMutex_);

    totalStreamsOptimized_ = 0;
    totalOriginalBytes_ = 0;
    totalCompressedBytes_ = 0;
    totalProcessingTime_ = std::chrono::microseconds(0);
    compressedStreams_ = 0;
}

void StreamOptimizer::setAdaptiveEnabled(bool enabled) {
    adaptiveEnabled_ = enabled;
}

void StreamOptimizer::setDeltaEncodingEnabled(bool enabled) {
    deltaEncodingEnabled_ = enabled;
}

void StreamOptimizer::setCompressionLevel(int level) {
    compressionLevel_ = std::max(1, std::min(16, level));
}

} // namespace AndroidStreamManager
