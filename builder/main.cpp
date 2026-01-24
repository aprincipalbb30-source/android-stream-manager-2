#include <iostream>
#include <string>
#include <vector>
#include "builder/apk_generator.h"
#include "shared/apk_config.h"

// Função para exibir a ajuda
void show_usage(const std::string& name) {
    std::cerr << "Uso: " << name << " <opções>\n"
              << "Opções:\n"
              << "\t-h,--help\t\tMostrar esta mensagem de ajuda\n"
              << "\t--app-name <nome>\tNome do aplicativo (ex: 'Meu App')\n"
              << "\t--pkg-name <pacote>\tNome do pacote (ex: 'com.exemplo.meuapp')\n"
              << "\t--server-url <url>\tURL do servidor de streaming\n"
              << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc <= 1) {
        show_usage(argv[0]);
        return 1;
    }

    AndroidStreamManager::ApkConfig config;

    // TODO: Implementar um parser de argumentos de linha de comando mais robusto (ex: cxxopts)
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "-h") || (arg == "--help")) {
            show_usage(argv[0]);
            return 0;
        }
        // Adicionar mais parsing de argumentos aqui
    }

    std::cout << "Iniciando o gerador de APK..." << std::endl;

    // Esta é uma chamada de exemplo. Os valores devem vir dos argumentos.
    AndroidStreamManager::ApkGenerator generator;
    generator.generate(config);

    std::cout << "Processo do gerador de APK concluído." << std::endl;

    return 0;
}
