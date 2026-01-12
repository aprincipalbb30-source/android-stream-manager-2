#include "stream_optimizer.h"
#include <algorithm>

namespace AndroidStreamManager {

StreamOptimizer::StreamOptimizer() 
    : targetFrameRate(30),
      targetBitrate(2000000), // 2 Mbps
      quality(StreamQuality::BALANCED) {
    
    // Inicializar codecs
    initializeCodecs();
}

void StreamOptimizer::initializeCodecs() {
    // Configurar parâmetros do codec H.264
    avcodec_register_all();
    
    videoCodec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!videoCodec) {
        throw std::runtime_error("H.264 codec not found");
    }
    
    videoContext = avcodec_alloc_context3(videoCodec);
    videoContext->bit_rate = targetBitrate;
    videoContext->width = 1280;
    videoContext->height = 720;
    videoContext->time_base = (AVRational){1, targetFrameRate};
    videoContext->framerate = (AVRational){targetFrameRate, 1};
    videoContext->gop_size = 10;
    videoContext->max_b_frames = 1;
    videoContext->pix_fmt = AV_PIX_FMT_YUV420P;
    
    if (avcodec_open2(videoContext, videoCodec, nullptr) < 0) {
        throw std::runtime_error("Failed to open video codec");
    }
    
    // Configurar áudio Opus
    audioCodec = avcodec_find_encoder(AV_CODEC_ID_OPUS);
    audioContext = avcodec_alloc_context3(audioCodec);
    audioContext->sample_rate = 48000;
    audioContext->channels = 2;
    audioContext->channel_layout = AV_CH_LAYOUT_STEREO;
    audioContext->bit_rate = 64000; // 64 kbps
    audioContext->sample_fmt = AV_SAMPLE_FMT_S16;
    
    if (avcodec_open2(audioContext, audioCodec, nullptr) < 0) {
        throw std::runtime_error("Failed to open audio codec");
    }
}

StreamMetrics StreamOptimizer::optimizeStream(StreamData& data) {
    StreamMetrics metrics;
    
    switch (data.dataType) {
        case StreamData::DataType::VIDEO_H264:
            metrics = optimizeVideo(data);
            break;
            
        case StreamData::DataType::AUDIO_AAC:
        case StreamData::DataType::AUDIO_OPUS:
            metrics = optimizeAudio(data);
            break;
            
        default:
            // Não otimizar outros tipos de dados
            break;
    }
    
    // Aplicar compressão LZ4 se benéfico
    if (shouldCompress(data.data)) {
        auto compressed = CompressionManager::compressLZ4(data.data);
        float ratio = CompressionManager::getCompressionRatio(
            data.data, compressed);
        
        if (ratio > 1.2f) { // Pelo menos 20% de compressão
            data.data = std::move(compressed);
            metrics.compressionRatio = ratio;
            metrics.compressionApplied = true;
        }
    }
    
    updateAdaptiveBitrate(metrics);
    return metrics;
}

StreamMetrics StreamOptimizer::optimizeVideo(StreamData& data) {
    StreamMetrics metrics;
    
    // Analisar quadro
    metrics.frameSize = data.data.size();
    metrics.timestamp = std::chrono::system_clock::now();
    
    // Ajustar qualidade baseado na rede
    if (networkConditions.bandwidth < targetBitrate * 0.7) {
        // Reduzir qualidade
        adjustVideoQuality(0.8f);
        metrics.qualityAdjustment = -0.2f;
    } else if (networkConditions.bandwidth > targetBitrate * 1.3) {
        // Aumentar qualidade
        adjustVideoQuality(1.2f);
        metrics.qualityAdjustment = 0.2f;
    }
    
    // Aplicar encode se necessário
    if (data.dataType == StreamData::DataType::VIDEO_H264) {
        // Re-encode com parâmetros otimizados
        data.data = reencodeVideo(data.data);
        metrics.reencoded = true;
    }
    
    return metrics;
}

void StreamOptimizer::updateNetworkConditions(
    const NetworkConditions& conditions) {
    
    std::lock_guard<std::mutex> lock(mutex);
    networkConditions = conditions;
    
    // Ajustar bitrate alvo
    if (conditions.bandwidth > 0) {
        targetBitrate = static_cast<int>(
            conditions.bandwidth * 0.8); // Usar 80% da banda disponível
    }
    
    // Ajustar framerate se necessário
    if (conditions.latency > 100) { // Latência alta
        targetFrameRate = std::max(15, targetFrameRate - 5);
    } else if (conditions.latency < 50 && targetFrameRate < 60) {
        targetFrameRate = std::min(60, targetFrameRate + 5);
    }
}

std::vector<uint8_t> StreamOptimizer::reencodeVideo(
    const std::vector<uint8_t>& frameData) {
    
    // Implementação simplificada de re-encode
    // Em produção, usar FFmpeg ou similar
    
    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    
    // Configurar frame (simplificado)
    frame->format = videoContext->pix_fmt;
    frame->width = videoContext->width;
    frame->height = videoContext->height;
    
    // Aqui viria o decode do frame original
    // e encode com novos parâmetros
    
    av_frame_free(&frame);
    av_packet_free(&packet);
    
    // Retornar dados otimizados
    return frameData; // Placeholder
}

} // namespace AndroidStreamManager