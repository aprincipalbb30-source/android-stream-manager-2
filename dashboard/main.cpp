#include <QApplication>
#include <QStyleFactory>
#include <QDir>
#include <QStandardPaths>
#include <QMessageBox>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QIcon>
#include <QTranslator>
#include <QLocale>
#include <iostream>
#include "main_window.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    // Configurar aplicação
    app.setApplicationName("Android Stream Manager");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("StreamManager Corp");
    app.setOrganizationDomain("streammanager.local");

    // Definir ícone da aplicação
    app.setWindowIcon(QIcon(":/icons/app_icon.png"));

    // Configurar estilo
    app.setStyle(QStyleFactory::create("Fusion"));

    // Paleta escura moderna
    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::WindowText, Qt::white);
    darkPalette.setColor(QPalette::Base, QColor(25, 25, 25));
    darkPalette.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::ToolTipBase, Qt::white);
    darkPalette.setColor(QPalette::ToolTipText, Qt::black);
    darkPalette.setColor(QPalette::Text, Qt::white);
    darkPalette.setColor(QPalette::Button, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::ButtonText, Qt::white);
    darkPalette.setColor(QPalette::BrightText, Qt::red);
    darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));
    darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
    darkPalette.setColor(QPalette::HighlightedText, Qt::black);

    app.setPalette(darkPalette);

    // Configurar traduções
    QTranslator translator;
    if (translator.load(QLocale(), "android_stream_manager", "_", ":/translations")) {
        app.installTranslator(&translator);
    }

    // Verificar se podemos criar a janela principal
    try {
        MainWindow window;

        // Configurar tray icon (opcional)
        QSystemTrayIcon* trayIcon = nullptr;
        if (QSystemTrayIcon::isSystemTrayAvailable()) {
            trayIcon = new QSystemTrayIcon(QIcon(":/icons/tray_icon.png"), &app);
            trayIcon->setToolTip("Android Stream Manager");

            // Menu do tray
            QMenu* trayMenu = new QMenu();
            QAction* showAction = trayMenu->addAction("Mostrar");
            QAction* quitAction = trayMenu->addAction("Sair");

            QObject::connect(showAction, &QAction::triggered, [&window]() {
                window.show();
                window.activateWindow();
            });

            QObject::connect(quitAction, &QAction::triggered, &app, &QApplication::quit);

            trayIcon->setContextMenu(trayMenu);
            trayIcon->show();

            // Minimizar para tray
            QObject::connect(&window, &MainWindow::minimizedToTray, [trayIcon]() {
                trayIcon->showMessage("Android Stream Manager",
                                    "Aplicação minimizada para a bandeja do sistema",
                                    QSystemTrayIcon::Information, 2000);
            });
        }

        // Mostrar janela
        window.show();

        // Executar aplicação
        int result = app.exec();

        // Limpar
        delete trayIcon;

        return result;

    } catch (const std::exception& e) {
        QMessageBox::critical(nullptr, "Erro Fatal",
                            QString("Erro ao inicializar aplicação: %1").arg(e.what()));
        return 1;
    }
}