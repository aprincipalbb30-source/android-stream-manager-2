#include "app_monitoring_widget.h"
#include <QApplication>
#include <QDesktopWidget>
#include <QListWidgetItem>
#include <QTextCursor>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QMessageBox>
#include <QIcon>
#include <QFont>
#include <algorithm>
#include <iostream>

namespace AndroidStreamManager {

AppMonitoringWidget::AppMonitoringWidget(QWidget *parent)
    : QWidget(parent)
    , isMonitoringActive(false)
    , monitoringStartTime(0)
    , lastUpdateTime(0)
    , updateTimer(new QTimer(this)) {

    setupUI();

    // Configurar timer de atualização automática
    connect(updateTimer, &QTimer::timeout, this, &AppMonitoringWidget::updateDisplay);
    updateTimer->setInterval(5000); // Atualizar a cada 5 segundos

    // Conectar sinais
    connect(startMonitoringButton, &QPushButton::clicked, this, &AppMonitoringWidget::onStartMonitoringClicked);
    connect(stopMonitoringButton, &QPushButton::clicked, this, &AppMonitoringWidget::onStopMonitoringClicked);
    connect(refreshStatsButton, &QPushButton::clicked, this, &AppMonitoringWidget::onRefreshStatsClicked);
    connect(appsListWidget, &QListWidget::currentRowChanged, this, &AppMonitoringWidget::onAppSelectionChanged);
    connect(categoryFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AppMonitoringWidget::onCategoryFilterChanged);

    // Estado inicial
    updateDisplay();
}

AppMonitoringWidget::~AppMonitoringWidget() {
    stopMonitoring();
}

void AppMonitoringWidget::setupUI() {
    setWindowTitle("Monitoramento de Apps - Android Stream Manager");
    setMinimumSize(1000, 700);
    resize(1200, 800);

    mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(10, 10, 10, 10);

    // Título principal
    QLabel* titleLabel = new QLabel("📊 Monitoramento de Aplicativos");
    titleLabel->setStyleSheet("font-weight: bold; font-size: 16px; margin-bottom: 10px;");
    mainLayout->addWidget(titleLabel);

    // Splitter principal
    mainSplitter = new QSplitter(Qt::Horizontal);
    mainSplitter->setChildrenCollapsible(false);

    // Painel esquerdo - Lista de apps
    setupAppList();

    // Painel direito - Detalhes e controles
    setupDetailsPanel();

    mainLayout->addWidget(mainSplitter, 1);

    // Status bar inferior
    QHBoxLayout* statusLayout = new QHBoxLayout();
    QLabel* statusLabel = new QLabel("Status: Parado");
    statusLabel->setStyleSheet("color: red; font-weight: bold;");
    statusLayout->addWidget(statusLabel);

    statusLayout->addStretch();

    QLabel* versionLabel = new QLabel("v1.0.0");
    versionLabel->setStyleSheet("color: gray;");
    statusLayout->addWidget(versionLabel);

    mainLayout->addLayout(statusLayout);
}

void AppMonitoringWidget::setupAppList() {
    appsPanel = new QWidget();
    appsPanel->setMinimumWidth(300);
    appsPanel->setMaximumWidth(400);

    appsLayout = new QVBoxLayout(appsPanel);
    appsLayout->setSpacing(5);

    // Título da lista
    appsTitleLabel = new QLabel("📱 Aplicativos Monitorados");
    appsTitleLabel->setStyleSheet("font-weight: bold; font-size: 14px; margin-bottom: 5px;");
    appsLayout->addWidget(appsTitleLabel);

    // Lista de apps
    appsListWidget = new QListWidget();
    appsListWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    appsListWidget->setStyleSheet(
        "QListWidget { "
        "   border: 1px solid #ccc; "
        "   border-radius: 5px; "
        "   background-color: #f9f9f9; "
        "} "
        "QListWidget::item { "
        "   padding: 8px; "
        "   border-bottom: 1px solid #eee; "
        "} "
        "QListWidget::item:selected { "
        "   background-color: #e3f2fd; "
        "   color: #1976d2; "
        "} "
        "QListWidget::item:hover { "
        "   background-color: #f5f5f5; "
        "}"
    );
    appsLayout->addWidget(appsListWidget, 1);

    // App atual em execução
    QGroupBox* currentAppGroup = new QGroupBox("🎯 App Atual");
    QVBoxLayout* currentAppLayout = new QVBoxLayout(currentAppGroup);

    currentAppIconLabel = new QLabel("📱");
    currentAppIconLabel->setStyleSheet("font-size: 24px; text-align: center;");
    currentAppIconLabel->setAlignment(Qt::AlignCenter);
    currentAppLayout->addWidget(currentAppIconLabel);

    currentAppLabel = new QLabel("Nenhum app ativo");
    currentAppLabel->setStyleSheet("font-weight: bold; text-align: center; color: #666;");
    currentAppLabel->setAlignment(Qt::AlignCenter);
    currentAppLabel->setWordWrap(true);
    currentAppLayout->addWidget(currentAppLabel);

    appsLayout->addWidget(currentAppGroup);

    mainSplitter->addWidget(appsPanel);
}

void AppMonitoringWidget::setupDetailsPanel() {
    detailsPanel = new QWidget();
    detailsLayout = new QVBoxLayout(detailsPanel);
    detailsLayout->setSpacing(10);

    // Controles
    setupControls();

    // Estatísticas
    setupStatsDisplay();

    // Detalhes do app
    setupAppDetails();

    // Log de atividades
    setupActivityLog();

    mainSplitter->addWidget(detailsPanel);
    mainSplitter->setStretchFactor(1, 1);
}

void AppMonitoringWidget::setupControls() {
    controlsGroup = new QGroupBox("🎮 Controles");
    controlsLayout = new QHBoxLayout(controlsGroup);

    startMonitoringButton = new QPushButton("▶️ Iniciar Monitoramento");
    startMonitoringButton->setStyleSheet(
        "QPushButton { "
        "   background-color: #4CAF50; "
        "   color: white; "
        "   border: none; "
        "   padding: 8px 16px; "
        "   border-radius: 4px; "
        "   font-weight: bold; "
        "} "
        "QPushButton:hover { background-color: #45a049; } "
        "QPushButton:pressed { background-color: #3d8b40; }"
    );

    stopMonitoringButton = new QPushButton("⏹️ Parar Monitoramento");
    stopMonitoringButton->setStyleSheet(
        "QPushButton { "
        "   background-color: #f44336; "
        "   color: white; "
        "   border: none; "
        "   padding: 8px 16px; "
        "   border-radius: 4px; "
        "   font-weight: bold; "
        "} "
        "QPushButton:hover { background-color: #da190b; } "
        "QPushButton:pressed { background-color: #b71c1c; }"
    );

    refreshStatsButton = new QPushButton("🔄 Atualizar");
    refreshStatsButton->setStyleSheet(
        "QPushButton { "
        "   background-color: #2196F3; "
        "   color: white; "
        "   border: none; "
        "   padding: 8px 16px; "
        "   border-radius: 4px; "
        "   font-weight: bold; "
        "} "
        "QPushButton:hover { background-color: #1976D2; } "
        "QPushButton:pressed { background-color: #1565C0; }"
    );

    controlsLayout->addWidget(startMonitoringButton);
    controlsLayout->addWidget(stopMonitoringButton);
    controlsLayout->addWidget(refreshStatsButton);
    controlsLayout->addStretch();

    // Filtro de categoria
    filterLayout = new QHBoxLayout();
    filterLabel = new QLabel("Filtro:");
    categoryFilterCombo = new QComboBox();
    categoryFilterCombo->addItem("Todos", "all");
    categoryFilterCombo->addItem("🎯 Sensíveis", "sensitive");
    categoryFilterCombo->addItem("🏦 Bancos", "banking");
    categoryFilterCombo->addItem("₿ Crypto", "cryptocurrency");
    categoryFilterCombo->addItem("💰 Financeiro", "financial");
    categoryFilterCombo->addItem("📱 Gerais", "general");

    filterLayout->addWidget(filterLabel);
    filterLayout->addWidget(categoryFilterCombo);
    filterLayout->addStretch();

    QVBoxLayout* controlsContainer = new QVBoxLayout();
    controlsContainer->addLayout(controlsLayout);
    controlsContainer->addLayout(filterLayout);

    controlsGroup->setLayout(controlsContainer);
    detailsLayout->addWidget(controlsGroup);
}

void AppMonitoringWidget::setupStatsDisplay() {
    statsGroup = new QGroupBox("📈 Estatísticas");
    statsLayout = new QGridLayout(statsGroup);

    totalAppsLabel = new QLabel("Total de Apps: 0");
    sensitiveAppsLabel = new QLabel("Apps Sensíveis: 0");
    bankingAppsLabel = new QLabel("Apps Bancários: 0");
    cryptoAppsLabel = new QLabel("Apps Crypto: 0");
    monitoringTimeLabel = new QLabel("Tempo de Monitoramento: 00:00:00");
    lastUpdateLabel = new QLabel("Última Atualização: nunca");

    statsLayout->addWidget(totalAppsLabel, 0, 0);
    statsLayout->addWidget(sensitiveAppsLabel, 0, 1);
    statsLayout->addWidget(bankingAppsLabel, 1, 0);
    statsLayout->addWidget(cryptoAppsLabel, 1, 1);
    statsLayout->addWidget(monitoringTimeLabel, 2, 0);
    statsLayout->addWidget(lastUpdateLabel, 2, 1);

    detailsLayout->addWidget(statsGroup);
}

void AppMonitoringWidget::setupAppDetails() {
    appDetailsGroup = new QGroupBox("📋 Detalhes do App");
    appDetailsLayout = new QVBoxLayout(appDetailsGroup);

    selectedAppNameLabel = new QLabel("Nome: --");
    selectedAppPackageLabel = new QLabel("Pacote: --");
    selectedAppCategoryLabel = new QLabel("Categoria: --");
    selectedAppTimeLabel = new QLabel("Tempo Total: --");
    selectedAppLaunchesLabel = new QLabel("Execuções: --");
    selectedAppFirstSeenLabel = new QLabel("Primeira Vez: --");
    selectedAppLastSeenLabel = new QLabel("Última Vez: --");

    appDetailsLayout->addWidget(selectedAppNameLabel);
    appDetailsLayout->addWidget(selectedAppPackageLabel);
    appDetailsLayout->addWidget(selectedAppCategoryLabel);
    appDetailsLayout->addWidget(selectedAppTimeLabel);
    appDetailsLayout->addWidget(selectedAppLaunchesLabel);
    appDetailsLayout->addWidget(selectedAppFirstSeenLabel);
    appDetailsLayout->addWidget(selectedAppLastSeenLabel);

    appDetailsLayout->addStretch();

    detailsLayout->addWidget(appDetailsGroup);
}

void AppMonitoringWidget::setupActivityLog() {
    activityLogGroup = new QGroupBox("📝 Log de Atividades");
    activityLogLayout = new QVBoxLayout(activityLogGroup);

    activityLogText = new QTextEdit();
    activityLogText->setMaximumHeight(200);
    activityLogText->setReadOnly(true);
    activityLogText->setFontFamily("Consolas");
    activityLogText->setFontPointSize(9);
    activityLogText->setStyleSheet(
        "QTextEdit { "
        "   background-color: #1a1a1a; "
        "   color: #00ff00; "
        "   border: 1px solid #555; "
        "   border-radius: 3px; "
        "}"
    );

    activityLogLayout->addWidget(activityLogText);

    detailsLayout->addWidget(activityLogGroup);
}

void AppMonitoringWidget::startMonitoring() {
    if (isMonitoringActive) return;

    isMonitoringActive = true;
    monitoringStartTime = QDateTime::currentMSecsSinceEpoch();

    startMonitoringButton->setEnabled(false);
    stopMonitoringButton->setEnabled(true);

    updateTimer->start();

    // Log da atividade
    logActivity("▶️ Monitoramento de apps iniciado");

    emit monitoringStarted();

    updateDisplay();
}

void AppMonitoringWidget::stopMonitoring() {
    if (!isMonitoringActive) return;

    isMonitoringActive = false;

    startMonitoringButton->setEnabled(true);
    stopMonitoringButton->setEnabled(false);

    updateTimer->stop();

    // Log da atividade
    logActivity("⏹️ Monitoramento de apps parado");

    emit monitoringStopped();

    updateDisplay();
}

void AppMonitoringWidget::refreshData() {
    // TODO: Implementar solicitação de dados atualizados do dispositivo
    logActivity("🔄 Solicitando atualização de dados dos apps");
    updateDisplay();
}

void AppMonitoringWidget::updateDisplay() {
    updateStatsDisplay();
    updateCurrentAppDisplay();
    updateAppList();
}

void AppMonitoringWidget::updateStatsDisplay() {
    int totalApps = appUsageData.size();
    int sensitiveApps = 0;
    int bankingApps = 0;
    int cryptoApps = 0;

    for (const auto& pair : appUsageData) {
        const AppUsageData& app = pair.second;
        if (app.isSensitive) sensitiveApps++;
        if (app.category == "banking") bankingApps++;
        else if (app.category == "cryptocurrency") cryptoApps++;
    }

    totalAppsLabel->setText(QString("Total de Apps: %1").arg(totalApps));
    sensitiveAppsLabel->setText(QString("Apps Sensíveis: %1").arg(sensitiveApps));
    bankingAppsLabel->setText(QString("Apps Bancários: %1").arg(bankingApps));
    cryptoAppsLabel->setText(QString("Apps Crypto: %1").arg(cryptoApps));

    // Tempo de monitoramento
    if (isMonitoringActive && monitoringStartTime > 0) {
        qint64 elapsed = (QDateTime::currentMSecsSinceEpoch() - monitoringStartTime) / 1000;
        int hours = elapsed / 3600;
        int minutes = (elapsed % 3600) / 60;
        int seconds = elapsed % 60;
        monitoringTimeLabel->setText(QString("Tempo de Monitoramento: %1:%2:%3")
                                    .arg(hours, 2, 10, QChar('0'))
                                    .arg(minutes, 2, 10, QChar('0'))
                                    .arg(seconds, 2, 10, QChar('0')));
    }

    // Última atualização
    if (lastUpdateTime > 0) {
        QDateTime lastUpdate = QDateTime::fromMSecsSinceEpoch(lastUpdateTime);
        lastUpdateLabel->setText(QString("Última Atualização: %1")
                               .arg(lastUpdate.toString("hh:mm:ss")));
    }
}

void AppMonitoringWidget::updateCurrentAppDisplay() {
    if (currentForegroundApp.isEmpty()) {
        currentAppLabel->setText("Nenhum app ativo");
        currentAppIconLabel->setText("📱");
    } else {
        auto it = appUsageData.find(currentForegroundApp);
        if (it != appUsageData.end()) {
            const AppUsageData& app = it->second;
            currentAppLabel->setText(app.displayName);
            currentAppIconLabel->setText(app.getCategoryIcon());
        } else {
            currentAppLabel->setText("App desconhecido");
            currentAppIconLabel->setText("❓");
        }
    }
}

void AppMonitoringWidget::updateAppList() {
    appsListWidget->clear();

    QString filter = categoryFilterCombo->currentData().toString();

    for (const auto& pair : appUsageData) {
        const AppUsageData& app = pair.second;

        // Aplicar filtro
        if (filter != "all") {
            if (filter == "sensitive" && !app.isSensitive) continue;
            if (filter == "general" && app.isSensitive) continue;
            if (filter != "sensitive" && filter != "general" && app.category != filter) continue;
        }

        QListWidgetItem* item = new QListWidgetItem();
        item->setText(QString("%1 %2").arg(app.getCategoryIcon(), app.displayName));
        item->setData(Qt::UserRole, app.packageName);

        // Cor baseada na categoria
        QString color = getCategoryColor(app.category);
        item->setBackground(QColor(color));

        // Ícone de sensibilidade
        if (app.isSensitive) {
            item->setIcon(QIcon()); // TODO: Adicionar ícone real
        }

        appsListWidget->addItem(item);
    }

    // Ordenar por tempo de uso
    appsListWidget->sortItems(Qt::DescendingOrder);
}

void AppMonitoringWidget::onAppUsageUpdate(const QString& packageName, const QString& appName, bool isSensitive) {
    currentForegroundApp = packageName;

    // Adicionar ou atualizar app nos dados
    AppUsageData appData;
    auto it = appUsageData.find(packageName);
    if (it != appUsageData.end()) {
        appData = it->second;
        appData.lastTimeStamp = QDateTime::currentMSecsSinceEpoch();
        appData.launchCount++;
    } else {
        appData.packageName = packageName;
        appData.displayName = appName;
        appData.isSensitive = isSensitive;
        appData.category = getAppCategoryFromPackage(packageName);
        appData.firstTimeStamp = QDateTime::currentMSecsSinceEpoch();
        appData.lastTimeStamp = appData.firstTimeStamp;
        appData.totalTimeInForeground = 0;
        appData.launchCount = 1;

        emit appDetected(appName, appData.category);
        if (isSensitive) {
            emit sensitiveAppDetected(appName, appData.category);
        }
    }

    appUsageData[packageName] = appData;
    lastUpdateTime = QDateTime::currentMSecsSinceEpoch();

    logActivity(QString("📱 App mudado: %1").arg(appName));

    updateDisplay();
}

void AppMonitoringWidget::onAppStatsUpdate(const QString& statsJson) {
    // TODO: Parse JSON e atualizar dados
    logActivity("📊 Estatísticas de apps atualizadas");
    lastUpdateTime = QDateTime::currentMSecsSinceEpoch();
    updateDisplay();
}

void AppMonitoringWidget::onStartMonitoringClicked() {
    startMonitoring();
}

void AppMonitoringWidget::onStopMonitoringClicked() {
    stopMonitoring();
}

void AppMonitoringWidget::onRefreshStatsClicked() {
    refreshData();
}

void AppMonitoringWidget::onAppSelectionChanged() {
    QListWidgetItem* currentItem = appsListWidget->currentItem();
    if (!currentItem) {
        // Limpar detalhes
        selectedAppNameLabel->setText("Nome: --");
        selectedAppPackageLabel->setText("Pacote: --");
        selectedAppCategoryLabel->setText("Categoria: --");
        selectedAppTimeLabel->setText("Tempo Total: --");
        selectedAppLaunchesLabel->setText("Execuções: --");
        selectedAppFirstSeenLabel->setText("Primeira Vez: --");
        selectedAppLastSeenLabel->setText("Última Vez: --");
        return;
    }

    QString packageName = currentItem->data(Qt::UserRole).toString();
    auto it = appUsageData.find(packageName);
    if (it != appUsageData.end()) {
        const AppUsageData& app = it->second;

        selectedAppNameLabel->setText(QString("Nome: %1").arg(app.displayName));
        selectedAppPackageLabel->setText(QString("Pacote: %1").arg(app.packageName));
        selectedAppCategoryLabel->setText(QString("Categoria: %1 %2")
                                        .arg(app.getCategoryIcon(), app.category));
        selectedAppTimeLabel->setText(QString("Tempo Total: %1").arg(app.getFormattedTime()));
        selectedAppLaunchesLabel->setText(QString("Execuções: %1").arg(app.launchCount));
        selectedAppFirstSeenLabel->setText(QString("Primeira Vez: %1")
                                         .arg(formatTimestamp(app.firstTimeStamp)));
        selectedAppLastSeenLabel->setText(QString("Última Vez: %1")
                                        .arg(formatTimestamp(app.lastTimeStamp)));
    }
}

void AppMonitoringWidget::onCategoryFilterChanged(int index) {
    updateAppList();
}

void AppMonitoringWidget::logActivity(const QString& message) {
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString logEntry = QString("[%1] %2\n").arg(timestamp, message);

    activityLogText->append(logEntry);

    // Limitar tamanho do log
    QString currentText = activityLogText->toPlainText();
    QStringList lines = currentText.split('\n');
    if (lines.size() > 100) {
        lines = lines.mid(lines.size() - 100);
        activityLogText->setPlainText(lines.join('\n'));
    }

    // Scroll para o final
    QTextCursor cursor = activityLogText->textCursor();
    cursor.movePosition(QTextCursor::End);
    activityLogText->setTextCursor(cursor);
}

QString AppMonitoringWidget::formatTimestamp(qint64 timestamp) const {
    QDateTime dt = QDateTime::fromMSecsSinceEpoch(timestamp);
    return dt.toString("dd/MM/yyyy hh:mm:ss");
}

QString AppMonitoringWidget::getCategoryColor(const QString& category) const {
    if (category == "banking") return "#e8f5e8"; // Verde claro
    if (category == "cryptocurrency") return "#fff3e0"; // Laranja claro
    if (category == "financial") return "#f3e5f5"; // Roxo claro
    if (category == "sensitive") return "#ffebee"; // Vermelho claro
    return "#f5f5f5"; // Cinza claro
}

QString AppMonitoringWidget::getAppCategoryFromPackage(const QString& packageName) const {
    QString pkg = packageName.toLower();
    if (pkg.contains("banco") || pkg.contains("bb.") || pkg.contains("itau") ||
        pkg.contains("bradesco") || pkg.contains("santander")) {
        return "banking";
    } else if (pkg.contains("wallet") || pkg.contains("crypto") || pkg.contains("metamask") ||
               pkg.contains("binance") || pkg.contains("coinbase")) {
        return "cryptocurrency";
    } else if (pkg.contains("paypal") || pkg.contains("venmo") || pkg.contains("cashapp")) {
        return "financial";
    }
    return "general";
}

} // namespace AndroidStreamManager