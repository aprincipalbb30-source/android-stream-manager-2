#include "stream_server.h"
#include <iostream>
#include <csignal>
#include <atomic>

std::atomic<bool> running(true);

void signalHandler(int signal) {
    std::cout << "\nRecebido sinal " << signal << ", finalizando..." << std::endl;
    running = false;
}

int main(int argc, char* argv[]) {
    // Configurar handlers de sinal
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    std::cout << "========================================" << std::endl;
    std::cout << "   Android Stream Manager Server" << std::endl;
    std::cout << "========================================" << std::endl;

    try {
        // Configurações do servidor
        int port = 8443;
        std::string certPath = "";
        std::string keyPath = "";

        // Parse de argumentos de linha de comando
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "--port" && i + 1 < argc) {
                port = std::stoi(argv[++i]);
            } else if (arg == "--cert" && i + 1 < argc) {
                certPath = argv[++i];
            } else if (arg == "--key" && i + 1 < argc) {
                keyPath = argv[++i];
            } else if (arg == "--help" || arg == "-h") {
                std::cout << "Uso: " << argv[0] << " [opções]" << std::endl;
                std::cout << "Opções:" << std::endl;
                std::cout << "  --port <porta>    Porta do servidor (padrão: 8443)" << std::endl;
                std::cout << "  --cert <arquivo>  Caminho para certificado SSL" << std::endl;
                std::cout << "  --key <arquivo>   Caminho para chave privada SSL" << std::endl;
                std::cout << "  --help, -h        Mostra esta ajuda" << std::endl;
                return 0;
            }
        }

        std::cout << "Configurações:" << std::endl;
        std::cout << "  Porta: " << port << std::endl;
        if (!certPath.empty()) {
            std::cout << "  Certificado SSL: " << certPath << std::endl;
            std::cout << "  Chave privada: " << keyPath << std::endl;
        } else {
            std::cout << "  Modo: HTTP (sem SSL)" << std::endl;
        }
        std::cout << std::endl;

        // Criar servidor
        AndroidStreamManager::StreamServer server;

        // Configurar callbacks
        server.setDeviceConnectedCallback([](const std::string& deviceId, const AndroidStreamManager::DeviceInfo& info) {
            std::cout << "✅ Dispositivo conectado: " << deviceId << " (" << info.deviceModel << ")" << std::endl;
        });

        server.setDeviceDisconnectedCallback([](const std::string& deviceId) {
            std::cout << "❌ Dispositivo desconectado: " << deviceId << std::endl;
        });

        server.setMessageReceivedCallback([](const std::string& deviceId, const AndroidStreamManager::ControlMessage& message) {
            std::cout << "📨 Mensagem recebida de " << deviceId << std::endl;
        });

        server.setStreamDataCallback([](const std::string& deviceId, const AndroidStreamManager::StreamData& data) {
            std::cout << "📊 Dados de stream recebidos de " << deviceId << " (" << data.data.size() << " bytes)" << std::endl;
        });

        // Inicializar servidor
        if (!server.initialize(port, certPath, keyPath)) {
            std::cerr << "❌ Falha ao inicializar servidor" << std::endl;
            return 1;
        }

        // Iniciar servidor
        if (!server.start()) {
            std::cerr << "❌ Falha ao iniciar servidor" << std::endl;
            return 1;
        }

        std::cout << "🚀 Servidor iniciado com sucesso!" << std::endl;
        std::cout << "📡 Ouvindo na porta " << port << std::endl;
        std::cout << "🌐 Endpoints disponíveis:" << std::endl;
        std::cout << "  GET  /api/health     - Status do servidor" << std::endl;
        std::cout << "  GET  /api/stats      - Estatísticas" << std::endl;
        std::cout << "  GET  /api/devices    - Lista de dispositivos" << std::endl;
        std::cout << "  POST /api/devices/{id}/control - Controle de dispositivo" << std::endl;
        std::cout << std::endl;
        std::cout << "⚡ Pressione Ctrl+C para finalizar" << std::endl;
        std::cout << "========================================" << std::endl;

        // Loop principal
        while (running) {
            // Mostrar estatísticas periodicamente
            static auto lastStats = std::chrono::system_clock::now();
            auto now = std::chrono::system_clock::now();

            if (std::chrono::duration_cast<std::chrono::seconds>(now - lastStats).count() >= 60) {
                auto stats = server.getStats();
                std::cout << "📊 Stats - Dispositivos: " << stats.connectedDevices
                          << ", Streams: " << stats.activeStreams
                          << ", Uptime: " << stats.uptime.count() << "s" << std::endl;
                lastStats = now;
            }

            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        std::cout << "\n🛑 Finalizando servidor..." << std::endl;
        server.stop();

        std::cout << "✅ Servidor finalizado com sucesso!" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "❌ Erro fatal: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}