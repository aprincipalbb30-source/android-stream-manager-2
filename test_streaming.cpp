#include <iostream>
#include <thread>
#include <chrono>
#include <memory>
#include "server/stream_server.h"
#include "core/system_manager.h"

using namespace AndroidStreamManager;

void testVideoStreaming() {
    std::cout << "🧪 TESTANDO SISTEMA DE STREAMING DE VÍDEO" << std::endl;
    std::cout << "==========================================" << std::endl;

    try {
        // 1. Inicializar SystemManager
        std::cout << "📡 Inicializando SystemManager..." << std::endl;
        if (!SystemManager::getInstance().initialize()) {
            throw std::runtime_error("Falha ao inicializar SystemManager");
        }
        std::cout << "✅ SystemManager inicializado" << std::endl;

        // 2. Aguardar inicialização completa
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // 3. Simular conexão de dispositivo
        std::cout << "📱 Simulando dispositivo Android..." << std::endl;
        // Em produção, isso seria feito pelo cliente real

        // 4. Testar transmissão de frames simulados
        std::cout << "🎬 Testando transmissão de frames..." << std::endl;

        for (int i = 0; i < 10; ++i) {
            // Simular frame H.264
            std::vector<uint8_t> mockFrame(1024, static_cast<uint8_t>(i % 256));

            StreamData frameData;
            frameData.deviceId = "test_device";
            frameData.dataType = StreamData::DataType::VIDEO_H264;
            frameData.frameData = mockFrame;
            frameData.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            frameData.isKeyFrame = (i % 30 == 0); // Keyframe a cada 30 frames

            // Enviar para servidor (simulado)
            SystemManager::getInstance().getMetricsCollector().incrementCounter("video_frames_sent");

            std::cout << "📦 Frame " << i << " enviado: " << mockFrame.size()
                      << " bytes, key=" << frameData.isKeyFrame << std::endl;

            std::this_thread::sleep_for(std::chrono::milliseconds(100)); // ~10 FPS
        }

        // 5. Verificar métricas
        std::cout << "📊 Verificando métricas..." << std::endl;
        auto metrics = SystemManager::getInstance().getMetricsCollector().getApplicationMetrics();
        std::cout << "📈 Frames enviados: " << metrics.total_requests << std::endl;

        // 6. Testar health check
        std::cout << "🔍 Executando health check..." << std::endl;
        auto healthStatus = SystemManager::getInstance().getHealthChecker().performHealthCheck();
        std::string statusStr = SystemManager::getInstance().getHealthChecker().getStatusDescription(healthStatus);
        std::cout << "💚 Status de saúde: " << statusStr << std::endl;

        std::cout << "✅ TESTE CONCLUÍDO COM SUCESSO!" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "❌ ERRO NO TESTE: " << e.what() << std::endl;
        return;
    }

    // Aguardar um pouco antes de finalizar
    std::this_thread::sleep_for(std::chrono::seconds(1));

    SystemManager::getInstance().shutdown();
}

int main() {
    std::cout << "🎬 ANDROID STREAM MANAGER - TESTE DE STREAMING" << std::endl;
    std::cout << "===============================================" << std::endl;

    testVideoStreaming();

    return 0;
}