#include <QApplication>
#include <QMessageBox>
#include "dashboard/main_window.h"
#include "dashboard/apk_config_widget.h"
#include <iostream>

int testDashboard(int argc, char* argv[]) {
    std::cout << "=== Teste Dashboard Qt ===" << std::endl;

    QApplication app(argc, argv);

    // Testar widget de configuração APK
    std::cout << "Testando ApkConfigWidget..." << std::endl;
    ApkConfigWidget configWidget;

    // Simular configuração
    ApkConfig config;
    config.appName = "Test App";
    config.packageName = "com.test.app";
    config.versionCode = 1;
    config.serverUrl = "wss://test-server.com:8443";
    config.permissions = {"CAMERA", "RECORD_AUDIO", "INTERNET"};

    configWidget.setConfiguration(config);
    configWidget.show();

    // Testar validação
    bool isValid = configWidget.validateConfiguration();
    std::cout << "Configuração válida: " << (isValid ? "Sim" : "Não") << std::endl;

    // Obter configuração
    ApkConfig retrievedConfig = configWidget.getConfiguration();
    std::cout << "App Name: " << retrievedConfig.appName << std::endl;
    std::cout << "Package: " << retrievedConfig.packageName << std::endl;
    std::cout << "Permissões: " << retrievedConfig.permissions.size() << std::endl;

    // Testar MainWindow (só se não estiver em modo headless)
    char* display = getenv("DISPLAY");
    if (display && strlen(display) > 0) {
        std::cout << "Testando MainWindow..." << std::endl;

        MainWindow window;
        window.show();

        // Executar por alguns segundos
        QTimer::singleShot(2000, [&app]() {
            std::cout << "MainWindow testado com sucesso" << std::endl;
            app.quit();
        });

        return app.exec();
    } else {
        std::cout << "Ambiente headless detectado - pulando teste visual" << std::endl;
        return 0;
    }
}

int main(int argc, char* argv[]) {
    return testDashboard(argc, argv);
}