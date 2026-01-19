#include "main_window.h"
#include "monitoring_widget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QTextEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QProgressBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QListWidget>
#include <QSplitter>
#include <QStatusBar>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QTimer>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QSystemTrayIcon>
#include <QCloseEvent>
#include <QApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QDir>
#include <QStandardPaths>
#include <QSettings>
#include <iostream>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , updateTimer(new QTimer(this))
    , deviceManager(nullptr)
    , apkBuilder(nullptr) {

    // Configurar janela
    setWindowTitle("Android Stream Manager v1.0.0");
    setMinimumSize(1200, 800);
    resize(1400, 900);

    // Configurar ícones (placeholders - em produção usar recursos reais)
    setWindowIcon(QIcon(":/icons/app_icon.png"));

    // Inicializar componentes do sistema
    initializeSystemComponents();

    // Configurar interface
    setupUI();

    // Configurar conexões de sinais/slots
    setupConnections();

    // Carregar configurações salvas
    loadSettings();

    // Iniciar timer de atualização
    updateTimer->start(5000); // Atualizar a cada 5 segundos

    // Status inicial
    statusLabel->setText("Sistema inicializado - Pronto");
}

MainWindow::~MainWindow() {
    saveSettings();
    updateTimer->stop();
}

void MainWindow::initializeSystemComponents() {
    try {
        // TODO: Inicializar DeviceManager e ApkBuilder quando estiverem disponíveis
        // Por enquanto, criar placeholders

        std::cout << "Componentes do sistema inicializados" << std::endl;

    } catch (const std::exception& e) {
        QMessageBox::warning(this, "Aviso de Inicialização",
                           QString("Erro ao inicializar componentes: %1").arg(e.what()));
    }
}

void MainWindow::setupUI() {
    // Criar barra de menus
    setupMenuBar();

    // Widget central
    QWidget* centralWidget = new QWidget;
    setCentralWidget(centralWidget);

    // Layout principal
    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);

    // Splitter principal (painéis esquerdo e direito)
    QSplitter* mainSplitter = new QSplitter(Qt::Horizontal);
    mainLayout->addWidget(mainSplitter);

    // Painel esquerdo - Lista de dispositivos e controles
    setupDeviceControlPanel(mainSplitter);

    // Painel direito - Configuração APK e logs
    setupRightPanel(mainSplitter);

    // Configurar proporções do splitter
    mainSplitter->setStretchFactor(0, 1);
    mainSplitter->setStretchFactor(1, 1);

    // Barra de status
    setupStatusBar();

    // Conectar sinais do timer
    connect(updateTimer, &QTimer::timeout, this, &MainWindow::onUpdateStatistics);
}

void MainWindow::setupMenuBar() {
    QMenuBar* menuBar = new QMenuBar(this);
    setMenuBar(menuBar);

    // Menu Arquivo
    QMenu* fileMenu = menuBar->addMenu("&Arquivo");

    QAction* connectAction = fileMenu->addAction("&Conectar ao Servidor");
    connectAction->setShortcut(QKeySequence("Ctrl+C"));
    connectAction->setStatusTip("Conectar ao servidor de streaming");

    QAction* disconnectAction = fileMenu->addAction("&Desconectar");
    disconnectAction->setShortcut(QKeySequence("Ctrl+D"));
    disconnectAction->setStatusTip("Desconectar do servidor");

    fileMenu->addSeparator();

    QAction* exitAction = fileMenu->addAction("Sai&r");
    exitAction->setShortcut(QKeySequence("Ctrl+Q"));
    exitAction->setStatusTip("Sair da aplicação");
    connect(exitAction, &QAction::triggered, this, &QWidget::close);

    // Menu Dispositivos
    QMenu* deviceMenu = menuBar->addMenu("&Dispositivos");

    QAction* refreshDevicesAction = deviceMenu->addAction("&Atualizar Lista");
    refreshDevicesAction->setShortcut(QKeySequence("F5"));
    refreshDevicesAction->setStatusTip("Atualizar lista de dispositivos conectados");

    deviceMenu->addSeparator();

    QAction* deviceSettingsAction = deviceMenu->addAction("&Configurações de Dispositivo");
    deviceSettingsAction->setStatusTip("Configurar parâmetros do dispositivo");

    // Menu Build
    QMenu* buildMenu = menuBar->addMenu("&Build");

    QAction* newBuildAction = buildMenu->addAction("&Novo Build APK");
    newBuildAction->setShortcut(QKeySequence("Ctrl+N"));
    newBuildAction->setStatusTip("Criar novo build de APK");

    QAction* buildHistoryAction = buildMenu->addAction("&Histórico de Builds");
    buildHistoryAction->setStatusTip("Ver histórico de builds realizados");

    // Menu Ferramentas
    QMenu* toolsMenu = menuBar->addMenu("&Ferramentas");

    QAction* serverSettingsAction = toolsMenu->addAction("&Configurações do Servidor");
    serverSettingsAction->setStatusTip("Configurar conexão com servidor");

    QAction* clearLogsAction = toolsMenu->addAction("&Limpar Logs");
    clearLogsAction->setStatusTip("Limpar logs do sistema");

    // Menu Monitoramento
    QMenu* monitoringMenu = menuBar->addMenu("&Monitoramento");

    QAction* monitoringDashboardAction = monitoringMenu->addAction("&Dashboard de Monitoramento");
    monitoringDashboardAction->setShortcut(QKeySequence("Ctrl+M"));
    monitoringDashboardAction->setStatusTip("Abrir dashboard de monitoramento");
    connect(monitoringDashboardAction, &QAction::triggered, this, &MainWindow::showMonitoringDashboard);

    QAction* refreshMonitoringAction = monitoringMenu->addAction("&Atualizar Métricas");
    refreshMonitoringAction->setShortcut(QKeySequence("F12"));
    refreshMonitoringAction->setStatusTip("Atualizar todas as métricas");
    connect(refreshMonitoringAction, &QAction::triggered, this, &MainWindow::refreshMonitoringData);

    // Menu Streaming
    QMenu* streamingMenu = menuBar->addMenu("&Streaming");

    QAction* streamingViewerAction = streamingMenu->addAction("&Visualizador de Streaming");
    streamingViewerAction->setShortcut(QKeySequence("Ctrl+S"));
    streamingViewerAction->setStatusTip("Abrir mini-emulador para visualização de streaming");
    connect(streamingViewerAction, &QAction::triggered, this, &MainWindow::showStreamingViewer);

    QAction* startStreamAction = streamingMenu->addAction("&Iniciar Streaming");
    startStreamAction->setShortcut(QKeySequence("Ctrl+Shift+S"));
    startStreamAction->setStatusTip("Iniciar streaming do dispositivo selecionado");
    connect(startStreamAction, &QAction::triggered, this, &MainWindow::onStartStreamClicked);

    QAction* stopStreamAction = streamingMenu->addAction("&Parar Streaming");
    stopStreamAction->setShortcut(QKeySequence("Ctrl+Shift+X"));
    stopStreamAction->setStatusTip("Parar streaming atual");
    connect(stopStreamAction, &QAction::triggered, this, &MainWindow::onPauseStreamClicked);

    streamingMenu->addSeparator();

    QAction* lockScreenAction = streamingMenu->addAction("🔒 &Bloquear Tela Remota");
    lockScreenAction->setShortcut(QKeySequence("Ctrl+Shift+L"));
    lockScreenAction->setStatusTip("Bloquear tela do dispositivo remoto");
    connect(lockScreenAction, &QAction::triggered, this, &MainWindow::onLockRemoteScreen);

    QAction* unlockScreenAction = streamingMenu->addAction("🔓 &Desbloquear Tela Remota");
    unlockScreenAction->setShortcut(QKeySequence("Ctrl+Shift+U"));
    unlockScreenAction->setStatusTip("Desbloquear tela do dispositivo remoto");
    connect(unlockScreenAction, &QAction::triggered, this, &MainWindow::onUnlockRemoteScreen);

    streamingMenu->addSeparator();

    QAction* appMonitoringAction = streamingMenu->addAction("📊 &Monitoramento de Apps");
    appMonitoringAction->setShortcut(QKeySequence("Ctrl+Shift+A"));
    appMonitoringAction->setStatusTip("Monitorar uso de aplicativos no dispositivo remoto");
    connect(appMonitoringAction, &QAction::triggered, this, &MainWindow::showAppMonitoringWidget);

    // Menu Ajuda
    QMenu* helpMenu = menuBar->addMenu("Aj&uda");

    QAction* aboutAction = helpMenu->addAction("&Sobre");
    aboutAction->setStatusTip("Sobre o Android Stream Manager");
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAboutDialog);

    QAction* documentationAction = helpMenu->addAction("&Documentação");
    documentationAction->setStatusTip("Abrir documentação");
    connect(documentationAction, &QAction::triggered, this, &MainWindow::openDocumentation);

    // Conectar ações
    connect(connectAction, &QAction::triggered, this, &MainWindow::onConnectToServer);
    connect(disconnectAction, &QAction::triggered, this, &MainWindow::onDisconnectFromServer);
    connect(refreshDevicesAction, &QAction::triggered, this, &MainWindow::updateDeviceList);
    connect(newBuildAction, &QAction::triggered, this, &MainWindow::onBuildApkClicked);
    connect(serverSettingsAction, &QAction::triggered, this, &MainWindow::showServerSettings);
}

void MainWindow::setupDeviceControlPanel(QSplitter* parent) {
    QWidget* devicePanel = new QWidget;
    parent->addWidget(devicePanel);

    QVBoxLayout* layout = new QVBoxLayout(devicePanel);

    // Título
    QLabel* titleLabel = new QLabel("Controle de Dispositivos");
    titleLabel->setStyleSheet("font-weight: bold; font-size: 14px; margin-bottom: 10px;");
    layout->addWidget(titleLabel);

    // Lista de dispositivos
    deviceList = new QListWidget;
    deviceList->setMaximumWidth(300);
    deviceList->setMinimumWidth(250);
    deviceList->setStyleSheet(
        "QListWidget { border: 1px solid #555; border-radius: 5px; }"
        "QListWidget::item { padding: 5px; border-bottom: 1px solid #333; }"
        "QListWidget::item:selected { background-color: #2a82da; }"
    );
    layout->addWidget(deviceList);

    // Botões de controle
    QHBoxLayout* buttonLayout = new QHBoxLayout;

    QPushButton* startStreamBtn = new QPushButton("▶️ Iniciar Stream");
    QPushButton* pauseStreamBtn = new QPushButton("⏸️ Pausar");
    QPushButton* stopStreamBtn = new QPushButton("⏹️ Parar");

    startStreamBtn->setStyleSheet("QPushButton { padding: 8px; margin: 2px; }");
    pauseStreamBtn->setStyleSheet("QPushButton { padding: 8px; margin: 2px; }");
    stopStreamBtn->setStyleSheet("QPushButton { padding: 8px; margin: 2px; }");

    buttonLayout->addWidget(startStreamBtn);
    buttonLayout->addWidget(pauseStreamBtn);
    buttonLayout->addWidget(stopStreamBtn);

    layout->addLayout(buttonLayout);

    // Informações do dispositivo selecionado
    QGroupBox* deviceInfoGroup = new QGroupBox("Informações do Dispositivo");
    QVBoxLayout* deviceInfoLayout = new QVBoxLayout(deviceInfoGroup);

    QLabel* deviceIdLabel = new QLabel("ID: <selecionar dispositivo>");
    QLabel* deviceModelLabel = new QLabel("Modelo: -");
    QLabel* androidVersionLabel = new QLabel("Android: -");
    QLabel* batteryLabel = new QLabel("Bateria: -");
    QLabel* statusLabel = new QLabel("Status: Desconectado");

    deviceInfoLayout->addWidget(deviceIdLabel);
    deviceInfoLayout->addWidget(deviceModelLabel);
    deviceInfoLayout->addWidget(androidVersionLabel);
    deviceInfoLayout->addWidget(batteryLabel);
    deviceInfoLayout->addWidget(statusLabel);

    layout->addWidget(deviceInfoGroup);

    // Conectar sinais dos botões
    connect(startStreamBtn, &QPushButton::clicked, this, &MainWindow::onStartStreamClicked);
    connect(pauseStreamBtn, &QPushButton::clicked, this, &MainWindow::onPauseStreamClicked);
    connect(stopStreamBtn, &QPushButton::clicked, this, &MainWindow::onStopStreamClicked);
    connect(deviceList, &QListWidget::currentRowChanged, this, &MainWindow::onDeviceSelectionChanged);
}

void MainWindow::setupRightPanel(QSplitter* parent) {
    QWidget* rightPanel = new QWidget;
    parent->addWidget(rightPanel);

    QVBoxLayout* layout = new QVBoxLayout(rightPanel);

    // Abas para diferentes funcionalidades
    QTabWidget* tabWidget = new QTabWidget;
    layout->addWidget(tabWidget);

    // Aba 1: Configuração APK
    setupApkConfigPanel(tabWidget);

    // Aba 2: Histórico de Builds
    setupBuildHistoryPanel(tabWidget);

    // Aba 3: Logs do Sistema
    setupEventLogPanel(tabWidget);

    // Barra de progresso (inicialmente oculta)
    buildProgress = new QProgressBar;
    buildProgress->setVisible(false);
    buildProgress->setRange(0, 100);
    layout->addWidget(buildProgress);
}

void MainWindow::setupApkConfigPanel(QTabWidget* parent) {
    QWidget* apkConfigWidget = new QWidget;
    parent->addTab(apkConfigWidget, "Configuração APK");

    QVBoxLayout* layout = new QVBoxLayout(apkConfigWidget);

    // Informações básicas do app
    QGroupBox* basicInfoGroup = new QGroupBox("Informações Básicas");
    QFormLayout* basicForm = new QFormLayout(basicInfoGroup);

    appNameEdit = new QLineEdit("My Streaming App");
    QLineEdit* packageNameEdit = new QLineEdit("com.company.streaming");
    QSpinBox* versionCodeSpin = new QSpinBox;
    versionCodeSpin->setRange(1, 999);
    versionCodeSpin->setValue(1);
    QLineEdit* versionNameEdit = new QLineEdit("1.0.0");

    basicForm->addRow("Nome do App:", appNameEdit);
    basicForm->addRow("Pacote:", packageNameEdit);
    basicForm->addRow("Versão Code:", versionCodeSpin);
    basicForm->addRow("Versão Name:", versionNameEdit);

    layout->addWidget(basicInfoGroup);

    // Configurações do servidor
    QGroupBox* serverGroup = new QGroupBox("Configurações do Servidor");
    QFormLayout* serverForm = new QFormLayout(serverGroup);

    serverHostEdit = new QLineEdit("stream-server.local");
    serverPortSpin = new QSpinBox;
    serverPortSpin->setRange(1, 65535);
    serverPortSpin->setValue(8443);

    serverForm->addRow("Host:", serverHostEdit);
    serverForm->addRow("Porta:", serverPortSpin);

    layout->addWidget(serverGroup);

    // Permissões
    QGroupBox* permissionsGroup = new QGroupBox("Permissões");
    QVBoxLayout* permissionsLayout = new QVBoxLayout(permissionsGroup);

    cameraPermissionCheck = new QCheckBox("Câmera");
    microphonePermissionCheck = new QCheckBox("Microfone");
    QCheckBox* storagePermissionCheck = new QCheckBox("Armazenamento");
    QCheckBox* locationPermissionCheck = new QCheckBox("Localização");

    cameraPermissionCheck->setChecked(true);
    microphonePermissionCheck->setChecked(true);
    storagePermissionCheck->setChecked(true);

    permissionsLayout->addWidget(cameraPermissionCheck);
    permissionsLayout->addWidget(microphonePermissionCheck);
    permissionsLayout->addWidget(storagePermissionCheck);
    permissionsLayout->addWidget(locationPermissionCheck);

    layout->addWidget(permissionsGroup);

    // Opções avançadas
    QGroupBox* advancedGroup = new QGroupBox("Opções Avançadas");
    QVBoxLayout* advancedLayout = new QVBoxLayout(advancedGroup);

    visibilityCombo = new QComboBox;
    visibilityCombo->addItem("Público", "public");
    visibilityCombo->addItem("Interno", "internal");
    visibilityCombo->addItem("Debug", "debug");

    persistenceCheck = new QCheckBox("Persistência de dados");
    persistenceCheck->setChecked(true);

    advancedLayout->addWidget(new QLabel("Visibilidade:"));
    advancedLayout->addWidget(visibilityCombo);
    advancedLayout->addWidget(persistenceCheck);

    layout->addWidget(advancedGroup);

    // Botão de build
    QPushButton* buildButton = new QPushButton("🚀 Construir APK");
    buildButton->setStyleSheet(
        "QPushButton { "
        "   background-color: #2a82da; "
        "   color: white; "
        "   padding: 10px; "
        "   font-weight: bold; "
        "   border-radius: 5px; "
        "}"
        "QPushButton:hover { background-color: #3a92ea; }"
    );

    layout->addWidget(buildButton);
    layout->addStretch();

    connect(buildButton, &QPushButton::clicked, this, &MainWindow::onBuildApkClicked);
}

void MainWindow::setupBuildHistoryPanel(QTabWidget* parent) {
    QWidget* buildHistoryWidget = new QWidget;
    parent->addTab(buildHistoryWidget, "Histórico de Builds");

    QVBoxLayout* layout = new QVBoxLayout(buildHistoryWidget);

    // Tabela de histórico
    buildHistoryTable = new QTableWidget;
    buildHistoryTable->setColumnCount(5);
    buildHistoryTable->setHorizontalHeaderLabels({"ID", "App Name", "Versão", "Data", "Status"});

    // Configurar tabela
    buildHistoryTable->setAlternatingRowColors(true);
    buildHistoryTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    buildHistoryTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    buildHistoryTable->horizontalHeader()->setStretchLastSection(true);

    layout->addWidget(buildHistoryTable);

    // Botões de ação
    QHBoxLayout* buttonLayout = new QHBoxLayout;

    QPushButton* refreshBtn = new QPushButton("🔄 Atualizar");
    QPushButton* downloadBtn = new QPushButton("📥 Download APK");
    QPushButton* deleteBtn = new QPushButton("🗑️ Excluir");

    buttonLayout->addWidget(refreshBtn);
    buttonLayout->addWidget(downloadBtn);
    buttonLayout->addWidget(deleteBtn);
    buttonLayout->addStretch();

    layout->addLayout(buttonLayout);

    // Conectar sinais
    connect(refreshBtn, &QPushButton::clicked, this, &MainWindow::updateBuildHistory);
}

void MainWindow::setupEventLogPanel(QTabWidget* parent) {
    QWidget* eventLogWidget = new QWidget;
    parent->addTab(eventLogWidget, "Logs do Sistema");

    QVBoxLayout* layout = new QVBoxLayout(eventLogWidget);

    // Área de log
    eventLog = new QTextEdit;
    eventLog->setReadOnly(true);
    eventLog->setFontFamily("Consolas");
    eventLog->setFontPointSize(9);
    eventLog->setStyleSheet(
        "QTextEdit { "
        "   background-color: #1a1a1a; "
        "   color: #00ff00; "
        "   border: 1px solid #555; "
        "   border-radius: 3px; "
        "}"
    );

    layout->addWidget(eventLog);

    // Controles de log
    QHBoxLayout* logControls = new QHBoxLayout;

    QPushButton* clearLogBtn = new QPushButton("🧹 Limpar");
    QPushButton* saveLogBtn = new QPushButton("💾 Salvar");
    QCheckBox* autoScrollCheck = new QCheckBox("Auto-scroll");
    autoScrollCheck->setChecked(true);

    logControls->addWidget(clearLogBtn);
    logControls->addWidget(saveLogBtn);
    logControls->addWidget(autoScrollCheck);
    logControls->addStretch();

    layout->addLayout(logControls);

    // Conectar sinais
    connect(clearLogBtn, &QPushButton::clicked, eventLog, &QTextEdit::clear);
    connect(saveLogBtn, &QPushButton::clicked, this, &MainWindow::saveEventLog);
}

void MainWindow::setupStatusBar() {
    statusLabel = new QLabel("Pronto");
    statusBar()->addWidget(statusLabel);

    statusBar()->addPermanentWidget(new QLabel("v1.0.0"));
}

void MainWindow::setupConnections() {
    // Conexões já foram feitas nos métodos setup*
}

// ========== MONITORAMENTO ==========

void MainWindow::showMonitoringDashboard() {
    // Criar widget de monitoramento se não existir
    if (!monitoringWidget) {
        monitoringWidget = new MonitoringWidget(this);
        monitoringWidget->setWindowTitle("Dashboard de Monitoramento - Android Stream Manager");
        monitoringWidget->resize(1000, 700);

        // Conectar sinais
        connect(monitoringWidget, &MonitoringWidget::alertTriggered,
                this, &MainWindow::onAlertReceived);
    }

    monitoringWidget->startMonitoring();
    monitoringWidget->show();
    monitoringWidget->activateWindow();
}

void MainWindow::refreshMonitoringData() {
    if (monitoringWidget) {
        monitoringWidget->refreshData();
    }

    showEventLog("Sistema", "Dados de monitoramento atualizados");
}

void MainWindow::onAlertReceived(const QString& message, const QString& severity) {
    // Mostrar alerta no log principal
    showEventLog("ALERTA", QString("%1: %2").arg(severity).arg(message).toStdString());

    // Mostrar notificação se for crítica
    if (severity == "CRITICAL" || severity == "HIGH") {
        QMessageBox::warning(this, "Alerta do Sistema",
                           QString("🚨 %1\n\n%2").arg(severity).arg(message));
    }
}

void MainWindow::updateDeviceList() {
    deviceList->clear();

    // TODO: Obter lista real de dispositivos do DeviceManager
    // Por enquanto, adicionar alguns dispositivos de exemplo

    QListWidgetItem* item1 = new QListWidgetItem("📱 Device_001 - Galaxy S21");
    item1->setData(Qt::UserRole, "device_001");
    deviceList->addItem(item1);

    QListWidgetItem* item2 = new QListWidgetItem("📱 Device_002 - Pixel 6");
    item2->setData(Qt::UserRole, "device_002");
    deviceList->addItem(item2);

    QListWidgetItem* item3 = new QListWidgetItem("📱 Device_003 - iPhone 13 (Emulado)");
    item3->setData(Qt::UserRole, "device_003");
    deviceList->addItem(item3);

    showEventLog("Sistema", "Lista de dispositivos atualizada");
}

void MainWindow::updateBuildHistory() {
    buildHistoryTable->setRowCount(0);

    // TODO: Obter histórico real do DatabaseManager
    // Por enquanto, adicionar alguns builds de exemplo

    int row = 0;

    // Build 1
    buildHistoryTable->insertRow(row);
    buildHistoryTable->setItem(row, 0, new QTableWidgetItem("build_001"));
    buildHistoryTable->setItem(row, 1, new QTableWidgetItem("My Streaming App"));
    buildHistoryTable->setItem(row, 2, new QTableWidgetItem("1.0.0"));
    buildHistoryTable->setItem(row, 3, new QTableWidgetItem("2024-01-15 10:30"));
    buildHistoryTable->setItem(row, 4, new QTableWidgetItem("✅ Sucesso"));

    row++;

    // Build 2
    buildHistoryTable->insertRow(row);
    buildHistoryTable->setItem(row, 0, new QTableWidgetItem("build_002"));
    buildHistoryTable->setItem(row, 1, new QTableWidgetItem("Corporate Monitor"));
    buildHistoryTable->setItem(row, 2, new QTableWidgetItem("2.1.0"));
    buildHistoryTable->setItem(row, 3, new QTableWidgetItem("2024-01-14 15:45"));
    buildHistoryTable->setItem(row, 4, new QTableWidgetItem("✅ Sucesso"));

    showEventLog("Sistema", "Histórico de builds atualizado");
}

// ========== SLOTS ==========

void MainWindow::onBuildApkClicked() {
    if (!apkBuilder) {
        QMessageBox::warning(this, "Erro", "Construtor de APK não disponível");
        return;
    }

    // Mostrar barra de progresso
    buildProgress->setVisible(true);
    buildProgress->setValue(0);

    // Desabilitar botão durante build
    QPushButton* buildBtn = qobject_cast<QPushButton*>(sender());
    if (buildBtn) {
        buildBtn->setEnabled(false);
    }

    showEventLog("Build", "Iniciando construção do APK...");

    // TODO: Implementar build real
    // Simular progresso
    QTimer::singleShot(1000, [this]() {
        buildProgress->setValue(25);
        showEventLog("Build", "Compilando recursos...");
    });

    QTimer::singleShot(2000, [this]() {
        buildProgress->setValue(50);
        showEventLog("Build", "Gerando bytecode...");
    });

    QTimer::singleShot(3000, [this]() {
        buildProgress->setValue(75);
        showEventLog("Build", "Empacotando APK...");
    });

    QTimer::singleShot(4000, [this, buildBtn]() {
        buildProgress->setValue(100);
        showEventLog("Build", "APK construído com sucesso!");

        QMessageBox::information(this, "Sucesso",
                               "APK construído com sucesso!\n"
                               "Arquivo: streaming_app_v1.0.0.apk");

        buildProgress->setVisible(false);
        if (buildBtn) {
            buildBtn->setEnabled(true);
        }

        updateBuildHistory();
    });
}

void MainWindow::onStartStreamClicked() {
    QString deviceId = getSelectedDeviceId();
    if (deviceId.isEmpty()) {
        QMessageBox::warning(this, "Aviso", "Selecione um dispositivo primeiro");
        return;
    }

    showEventLog("Streaming", QString("Iniciando stream para dispositivo: %1").arg(deviceId).toStdString());

    // TODO: Implementar streaming real
    QMessageBox::information(this, "Streaming",
                           QString("Stream iniciado para: %1").arg(deviceId));
}

void MainWindow::onPauseStreamClicked() {
    QString deviceId = getSelectedDeviceId();
    if (deviceId.isEmpty()) {
        QMessageBox::warning(this, "Aviso", "Selecione um dispositivo primeiro");
        return;
    }

    showEventLog("Streaming", QString("Pausando stream para dispositivo: %1").arg(deviceId).toStdString());
}

void MainWindow::onStopStreamClicked() {
    QString deviceId = getSelectedDeviceId();
    if (deviceId.isEmpty()) {
        QMessageBox::warning(this, "Aviso", "Selecione um dispositivo primeiro");
        return;
    }

    showEventLog("Streaming", QString("Parando stream para dispositivo: %1").arg(deviceId).toStdString());
}

void MainWindow::onRestartConnectionClicked() {
    showEventLog("Sistema", "Reiniciando conexão com servidor...");
    // TODO: Implementar reconexão
}

void MainWindow::onTerminateSessionClicked() {
    QString deviceId = getSelectedDeviceId();
    if (deviceId.isEmpty()) {
        QMessageBox::warning(this, "Aviso", "Selecione um dispositivo primeiro");
        return;
    }

    showEventLog("Sistema", QString("Terminando sessão para: %1").arg(deviceId).toStdString());
}

void MainWindow::onDeviceSelectionChanged() {
    QString deviceId = getSelectedDeviceId();
    if (!deviceId.isEmpty()) {
        showEventLog("Interface", QString("Dispositivo selecionado: %1").arg(deviceId).toStdString());
    }
}

void MainWindow::onUpdateStatistics() {
    // TODO: Atualizar estatísticas reais do sistema
    // Por enquanto, apenas atualizar timestamp
    static int counter = 0;
    counter++;

    if (counter % 12 == 0) { // A cada minuto
        showEventLog("Sistema", "Estatísticas atualizadas");
    }
}

void MainWindow::onSelectIconClicked() {
    QString fileName = QFileDialog::getOpenFileName(this,
        "Selecionar Ícone do App",
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
        "Imagens (*.png *.jpg *.jpeg *.ico)");

    if (!fileName.isEmpty()) {
        showEventLog("Configuração", QString("Ícone selecionado: %1").arg(fileName).toStdString());
    }
}

void MainWindow::onPermissionToggled() {
    showEventLog("Configuração", "Permissões atualizadas");
}

void MainWindow::onVisibilityChanged(int index) {
    QString visibility = visibilityCombo->itemData(index).toString();
    showEventLog("Configuração", QString("Visibilidade alterada para: %1").arg(visibility).toStdString());
}

// ========== HELPERS ==========

QString MainWindow::getSelectedDeviceId() const {
    QListWidgetItem* currentItem = deviceList->currentItem();
    if (currentItem) {
        return currentItem->data(Qt::UserRole).toString();
    }
    return QString();
}

void MainWindow::showEventLog(const std::string& component, const std::string& message) {
    if (!eventLog) return;

    QDateTime now = QDateTime::currentDateTime();
    QString timestamp = now.toString("yyyy-MM-dd hh:mm:ss");

    QString logEntry = QString("[%1] [%2] %3\n")
                      .arg(timestamp)
                      .arg(QString::fromStdString(component))
                      .arg(QString::fromStdString(message));

    eventLog->append(logEntry);

    // Auto-scroll se habilitado
    QTextCursor cursor = eventLog->textCursor();
    cursor.movePosition(QTextCursor::End);
    eventLog->setTextCursor(cursor);
}

void MainWindow::saveEventLog() {
    QString fileName = QFileDialog::getSaveFileName(this,
        "Salvar Log",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/stream_manager_log.txt",
        "Arquivos de Texto (*.txt);;Todos os Arquivos (*)");

    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << eventLog->toPlainText();
            file.close();
            QMessageBox::information(this, "Sucesso", "Log salvo com sucesso!");
        } else {
            QMessageBox::warning(this, "Erro", "Erro ao salvar log!");
        }
    }
}

// ========== MENU ACTIONS ==========

void MainWindow::onConnectToServer() {
    bool ok;
    QString serverAddress = QInputDialog::getText(this, "Conectar ao Servidor",
                                                "Endereço do servidor:", QLineEdit::Normal,
                                                "ws://localhost:8443", &ok);

    if (ok && !serverAddress.isEmpty()) {
        showEventLog("Servidor", QString("Conectando a: %1").arg(serverAddress).toStdString());
        // TODO: Implementar conexão real
    }
}

void MainWindow::onDisconnectFromServer() {
    showEventLog("Servidor", "Desconectando do servidor...");
    // TODO: Implementar desconexão
}

void MainWindow::showServerSettings() {
    QMessageBox::information(this, "Configurações do Servidor",
                           "Funcionalidade em desenvolvimento.\n"
                           "Será possível configurar:\n"
                           "- Endereço do servidor\n"
                           "- Porta de conexão\n"
                           "- Certificados SSL\n"
                           "- Timeouts de conexão");
}

void MainWindow::showAboutDialog() {
    QMessageBox::about(this, "Sobre Android Stream Manager",
        "<h2>Android Stream Manager v1.0.0</h2>"
        "<p>Sistema corporativo para gerenciamento remoto de dispositivos Android.</p>"
        "<p><b>Funcionalidades:</b></p>"
        "<ul>"
        "<li>Streaming em tempo real</li>"
        "<li>Construção automatizada de APKs</li>"
        "<li>Controle remoto de dispositivos</li>"
        "<li>Interface gráfica moderna</li>"
        "<li>Segurança enterprise</li>"
        "</ul>"
        "<p><b>Desenvolvido com:</b> C++, Qt6, SQLite, OpenSSL</p>"
        "<p>© 2024 Android Stream Manager Team</p>");
}

void MainWindow::openDocumentation() {
    QDesktopServices::openUrl(QUrl("https://github.com/aprincipalbb30-source/android-stream-manager"));
}

// ========== SETTINGS ==========

void MainWindow::loadSettings() {
    QSettings settings("StreamManager", "AndroidStreamManager");

    // Restaurar geometria da janela
    restoreGeometry(settings.value("geometry").toByteArray());

    // Restaurar configurações de APK
    appNameEdit->setText(settings.value("apk/appName", "My Streaming App").toString());
    serverHostEdit->setText(settings.value("apk/serverHost", "stream-server.local").toString());
    serverPortSpin->setValue(settings.value("apk/serverPort", 8443).toInt());
    cameraPermissionCheck->setChecked(settings.value("apk/cameraPermission", true).toBool());
    microphonePermissionCheck->setChecked(settings.value("apk/microphonePermission", true).toBool());

    showEventLog("Sistema", "Configurações carregadas");
}

void MainWindow::saveSettings() {
    QSettings settings("StreamManager", "AndroidStreamManager");

    // Salvar geometria da janela
    settings.setValue("geometry", saveGeometry());

    // Salvar configurações de APK
    settings.setValue("apk/appName", appNameEdit->text());
    settings.setValue("apk/serverHost", serverHostEdit->text());
    settings.setValue("apk/serverPort", serverPortSpin->value());
    settings.setValue("apk/cameraPermission", cameraPermissionCheck->isChecked());
    settings.setValue("apk/microphonePermission", microphonePermissionCheck->isChecked());

    showEventLog("Sistema", "Configurações salvas");
}

// ========== EVENTS ==========

void MainWindow::closeEvent(QCloseEvent *event) {
    // Perguntar confirmação se houver streams ativos
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "Confirmar Saída",
        "Tem certeza que deseja sair?\n"
        "Streams ativos serão interrompidos.",
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        saveSettings();
        event->accept();
    } else {
        event->ignore();
    }
}

// ========== STREAMING VIEWER ==========

void MainWindow::showStreamingViewer() {
    // Criar visualizador de streaming se não existir
    if (!streamingViewer) {
        streamingViewer = new StreamingViewer(this);
        streamingViewer->setWindowTitle("Mini Android Emulator - Android Stream Manager");
        streamingViewer->resize(1000, 700);

        // Conectar sinais
        connect(streamingViewer, &StreamingViewer::streamingStarted,
                this, &MainWindow::onStreamingStarted);
        connect(streamingViewer, &StreamingViewer::streamingStopped,
                this, &MainWindow::onStreamingStopped);
        connect(streamingViewer, &StreamingViewer::errorOccurred,
                this, &MainWindow::onStreamingError);
    }

    // Obter dispositivo selecionado
    QListWidgetItem* selectedItem = deviceList->currentItem();
    if (selectedItem) {
        QString deviceId = selectedItem->text().split(" - ").first();
        QString deviceName = selectedItem->text();

        streamingViewer->setDeviceInfo(deviceId, deviceName);
        streamingViewer->startStreaming(deviceId);
    } else {
        QMessageBox::information(this, "Selecionar Dispositivo",
                               "Por favor, selecione um dispositivo na lista antes de abrir o visualizador de streaming.");
        return;
    }

    streamingViewer->show();
    streamingViewer->activateWindow();
}

void MainWindow::onStreamingStarted(const QString& deviceId) {
    showEventLog("STREAMING", "Streaming iniciado para dispositivo: " + deviceId.toStdString());
}

void MainWindow::onStreamingStopped(const QString& deviceId) {
    showEventLog("STREAMING", "Streaming parado para dispositivo: " + deviceId.toStdString());
}

void MainWindow::onStreamingError(const QString& error) {
    showEventLog("STREAMING", "Erro no streaming: " + error.toStdString());
    QMessageBox::warning(this, "Erro no Streaming", error);
}

// ========== CONTROLE REMOTO DE TELA ==========

void MainWindow::onLockRemoteScreen() {
    QListWidgetItem* selectedItem = deviceList->currentItem();
    if (!selectedItem) {
        QMessageBox::information(this, "Selecionar Dispositivo",
                               "Por favor, selecione um dispositivo na lista para bloquear a tela remotamente.");
        return;
    }

    QString deviceId = selectedItem->text().split(" - ").first();

    // TODO: Implementar envio de comando de bloqueio via WebSocket para o dispositivo
    // Por enquanto, apenas log
    showEventLog("REMOTE_CONTROL", "Comando de bloqueio enviado para dispositivo: " + deviceId.toStdString());

    QMessageBox::information(this, "Bloqueio Remoto",
                           QString("Comando de bloqueio de tela enviado para o dispositivo %1.\n\n"
                                   "O dispositivo irá mostrar uma tela de 'Atualização do Android' "
                                   "que impede interação local mas permite controle remoto total.").arg(deviceId));
}

void MainWindow::onUnlockRemoteScreen() {
    QListWidgetItem* selectedItem = deviceList->currentItem();
    if (!selectedItem) {
        QMessageBox::information(this, "Selecionar Dispositivo",
                               "Por favor, selecione um dispositivo na lista para desbloquear a tela remotamente.");
        return;
    }

    QString deviceId = selectedItem->text().split(" - ").first();

    // TODO: Implementar envio de comando de desbloqueio via WebSocket para o dispositivo
    showEventLog("REMOTE_CONTROL", "Comando de desbloqueio enviado para dispositivo: " + deviceId.toStdString());

    QMessageBox::information(this, "Desbloqueio Remoto",
                           QString("Comando de desbloqueio de tela enviado para o dispositivo %1.").arg(deviceId));
}

// ========== MONITORAMENTO DE APPS ==========

void MainWindow::showAppMonitoringWidget() {
    // Criar widget de monitoramento de apps se não existir
    if (!appMonitoringWidget) {
        appMonitoringWidget = new AppMonitoringWidget(this);
        appMonitoringWidget->setWindowTitle("Monitoramento de Apps - Android Stream Manager");
        appMonitoringWidget->resize(1200, 800);

        // Conectar sinais
        connect(appMonitoringWidget, &AppMonitoringWidget::appDetected,
                this, &MainWindow::onAppDetected);
        connect(appMonitoringWidget, &AppMonitoringWidget::sensitiveAppDetected,
                this, &MainWindow::onSensitiveAppDetected);
    }

    appMonitoringWidget->show();
    appMonitoringWidget->activateWindow();
}

void MainWindow::onAppDetected(const QString& appName, const QString& category) {
    showEventLog("APP_MONITOR", QString("App detectado: %1 (%2)").arg(appName, category).toStdString());
}

void MainWindow::onSensitiveAppDetected(const QString& appName, const QString& category) {
    showEventLog("SENSITIVE_APP", QString("🚨 App sensível detectado: %1 (%2)").arg(appName, category).toStdString());

    // Mostrar notificação especial para apps sensíveis
    QMessageBox::information(this, "App Sensível Detectado",
                           QString("Um aplicativo sensível foi detectado:\n\n"
                                   "📱 %1\n"
                                   "🏷️ Categoria: %2\n\n"
                                   "Este tipo de app está sendo monitorado.").arg(appName, category));
}

void MainWindow::showEvent(QShowEvent *event) {
    QMainWindow::showEvent(event);

    // Inicializar lista de dispositivos quando a janela for mostrada
    if (deviceList->count() == 0) {
        updateDeviceList();
    }
}