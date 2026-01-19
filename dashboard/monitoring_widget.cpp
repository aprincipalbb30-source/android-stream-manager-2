#include "monitoring_widget.h"
#include <QApplication>
#include <QDesktopWidget>
#include <QHeaderView>
#include <QTableWidgetItem>
#include <QDateTime>
#include <QBrush>
#include <QColor>
#include <iostream>
#include <sstream>
#include <iomanip>

// Incluir headers do sistema de monitoramento
#include "monitoring/metrics_collector.h"
#include "monitoring/alerts_manager.h"
#include "monitoring/health_checker.h"

namespace AndroidStreamManager {

MonitoringWidget::MonitoringWidget(QWidget *parent)
    : QWidget(parent)
    , monitoringActive(false)
    , updateTimer(new QTimer(this))
    , cpuSeries(new QLineSeries())
    , cpuChartView(nullptr) {

    setupUI();
    setupCharts();

    // Configurar timer de atualização
    connect(updateTimer, &QTimer::timeout, this, &MonitoringWidget::updateMetrics);
    updateTimer->setInterval(2000); // Atualizar a cada 2 segundos

    // Configurar callbacks de alertas
    AlertsManager::getInstance().setAlertTriggeredCallback([this](const ActiveAlert& alert) {
        QString severity = QString::fromStdString(AlertsManager::getInstance().getStatusDescription(alert.severity));
        QString message = QString::fromStdString(alert.message);
        emit alertTriggered(message, severity);

        // Log do alerta
        QString logEntry = QString("[%1] 🚨 ALERTA %2: %3")
                          .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
                          .arg(severity)
                          .arg(message);
        monitoringLogs->append(logEntry);
    });

    // Configurar callback de mudança de saúde
    HealthChecker::getInstance().setHealthStatusCallback([this](HealthStatus oldStatus, HealthStatus newStatus,
                                                              const std::vector<HealthCheckResult>& results) {
        QString statusDesc = QString::fromStdString(HealthChecker::getInstance().getStatusDescription(newStatus));
        lastHealthStatus = statusDesc;

        // Log da mudança de saúde
        QString logEntry = QString("[%1] 🔍 Saúde mudou: %2")
                          .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
                          .arg(statusDesc);
        monitoringLogs->append(logEntry);
    });
}

MonitoringWidget::~MonitoringWidget() {
    stopMonitoring();
}

void MonitoringWidget::setupUI() {
    mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(10, 10, 10, 10);

    // Título
    QLabel* titleLabel = new QLabel("📊 Dashboard de Monitoramento");
    titleLabel->setStyleSheet("font-weight: bold; font-size: 16px; margin-bottom: 10px;");
    mainLayout->addWidget(titleLabel);

    // Layout horizontal principal
    QHBoxLayout* contentLayout = new QHBoxLayout();

    // Coluna esquerda - Métricas do sistema e gráficos
    QVBoxLayout* leftColumn = new QVBoxLayout();

    // Grupo de métricas do sistema
    systemMetricsGroup = new QGroupBox("💻 Sistema");
    QVBoxLayout* systemLayout = new QVBoxLayout(systemMetricsGroup);

    // CPU
    QHBoxLayout* cpuLayout = new QHBoxLayout();
    cpuLayout->addWidget(new QLabel("CPU:"));
    cpuUsageLabel = new QLabel("0.0%");
    cpuLayout->addWidget(cpuUsageLabel);
    cpuLayout->addStretch();
    systemLayout->addLayout(cpuLayout);

    cpuProgress = new QProgressBar();
    cpuProgress->setRange(0, 100);
    cpuProgress->setValue(0);
    systemLayout->addWidget(cpuProgress);

    // Memória
    QHBoxLayout* memoryLayout = new QHBoxLayout();
    memoryLayout->addWidget(new QLabel("Memória:"));
    memoryUsageLabel = new QLabel("0 MB / 0 MB");
    memoryLayout->addWidget(memoryUsageLabel);
    memoryLayout->addStretch();
    systemLayout->addLayout(memoryLayout);

    memoryProgress = new QProgressBar();
    memoryProgress->setRange(0, 100);
    memoryProgress->setValue(0);
    systemLayout->addWidget(memoryProgress);

    // Disco
    QHBoxLayout* diskLayout = new QHBoxLayout();
    diskLayout->addWidget(new QLabel("Disco:"));
    diskUsageLabel = new QLabel("0 GB / 0 GB");
    diskLayout->addWidget(diskUsageLabel);
    diskLayout->addStretch();
    systemLayout->addLayout(diskLayout);

    diskProgress = new QProgressBar();
    diskProgress->setRange(0, 100);
    diskProgress->setValue(0);
    systemLayout->addWidget(diskProgress);

    // Uptime
    QHBoxLayout* uptimeLayout = new QHBoxLayout();
    uptimeLayout->addWidget(new QLabel("Uptime:"));
    uptimeLabel = new QLabel("00:00:00");
    uptimeLayout->addWidget(uptimeLabel);
    uptimeLayout->addStretch();
    systemLayout->addLayout(uptimeLayout);

    leftColumn->addWidget(systemMetricsGroup);

    // Gráfico de CPU
    QGroupBox* chartGroup = new QGroupBox("📈 CPU ao Longo do Tempo");
    QVBoxLayout* chartLayout = new QVBoxLayout(chartGroup);

    // Criar gráfico de CPU
    QChart* cpuChart = new QChart();
    cpuChart->addSeries(cpuSeries);
    cpuChart->setTitle("");
    cpuChart->legend()->hide();

    QValueAxis* cpuAxisY = new QValueAxis();
    cpuAxisY->setRange(0, 100);
    cpuAxisY->setTitleText("%");
    cpuChart->addAxis(cpuAxisY, Qt::AlignLeft);
    cpuSeries->attachAxis(cpuAxisY);

    QDateTimeAxis* cpuAxisX = new QDateTimeAxis();
    cpuAxisX->setFormat("hh:mm:ss");
    cpuChart->addAxis(cpuAxisX, Qt::AlignBottom);
    cpuSeries->attachAxis(cpuAxisX);

    cpuChartView = new QChartView(cpuChart);
    cpuChartView->setMinimumHeight(200);
    chartLayout->addWidget(cpuChartView);

    leftColumn->addWidget(chartGroup);

    contentLayout->addLayout(leftColumn, 1);

    // Coluna direita - Dispositivos, streaming e alertas
    QVBoxLayout* rightColumn = new QVBoxLayout();

    // Grupo de dispositivos
    deviceMetricsGroup = new QGroupBox("📱 Dispositivos");
    QVBoxLayout* deviceLayout = new QVBoxLayout(deviceMetricsGroup);

    connectedDevicesLabel = new QLabel("Conectados: 0");
    totalDevicesLabel = new QLabel("Total: 0");
    streamingDevicesLabel = new QLabel("Streaming: 0");

    deviceLayout->addWidget(connectedDevicesLabel);
    deviceLayout->addWidget(totalDevicesLabel);
    deviceLayout->addWidget(streamingDevicesLabel);

    // Tabela de dispositivos
    devicesTable = new QTableWidget();
    devicesTable->setColumnCount(4);
    devicesTable->setHorizontalHeaderLabels({"ID", "Modelo", "Bateria", "Status"});
    devicesTable->setMaximumHeight(150);
    devicesTable->horizontalHeader()->setStretchLastSection(true);
    devicesTable->verticalHeader()->setVisible(false);

    deviceLayout->addWidget(devicesTable);

    rightColumn->addWidget(deviceMetricsGroup);

    // Grupo de streaming
    streamingMetricsGroup = new QGroupBox("🎥 Streaming");
    QVBoxLayout* streamingLayout = new QVBoxLayout(streamingMetricsGroup);

    activeStreamsLabel = new QLabel("Streams Ativos: 0");
    averageBitrateLabel = new QLabel("Bitrate Médio: 0 Mbps");
    averageLatencyLabel = new QLabel("Latência Média: 0 ms");
    streamSuccessRateLabel = new QLabel("Taxa de Sucesso: 100%");

    streamingLayout->addWidget(activeStreamsLabel);
    streamingLayout->addWidget(averageBitrateLabel);
    streamingLayout->addWidget(averageLatencyLabel);
    streamingLayout->addWidget(streamSuccessRateLabel);

    rightColumn->addWidget(streamingMetricsGroup);

    // Grupo de saúde
    healthGroup = new QGroupBox("🔍 Saúde do Sistema");
    QHBoxLayout* healthLayout = new QHBoxLayout(healthGroup);

    overallHealthLabel = new QLabel("Status: DESCONHECIDO");
    overallHealthLabel->setStyleSheet("font-weight: bold; color: gray;");

    lastHealthCheckLabel = new QLabel("Última verificação: nunca");

    healthCheckButton = new QPushButton("🔄 Verificar Agora");
    healthCheckButton->setMaximumWidth(120);

    healthLayout->addWidget(overallHealthLabel);
    healthLayout->addWidget(lastHealthCheckLabel);
    healthLayout->addStretch();
    healthLayout->addWidget(healthCheckButton);

    rightColumn->addWidget(healthGroup);

    contentLayout->addLayout(rightColumn, 1);

    mainLayout->addLayout(contentLayout);

    // Grupo de alertas (abaixo)
    alertsGroup = new QGroupBox("🚨 Alertas Ativos");
    QVBoxLayout* alertsLayout = new QVBoxLayout(alertsGroup);

    alertsTable = new QTableWidget();
    alertsTable->setColumnCount(4);
    alertsTable->setHorizontalHeaderLabels({"Horário", "Severidade", "Mensagem", "Status"});
    alertsTable->setMaximumHeight(150);
    alertsTable->horizontalHeader()->setStretchLastSection(true);
    alertsTable->verticalHeader()->setVisible(false);

    QHBoxLayout* alertsButtonLayout = new QHBoxLayout();
    acknowledgeAlertButton = new QPushButton("✅ Reconhecer Selecionado");
    acknowledgeAlertButton->setMaximumWidth(150);

    alertsButtonLayout->addWidget(acknowledgeAlertButton);
    alertsButtonLayout->addStretch();

    alertsLayout->addWidget(alertsTable);
    alertsLayout->addLayout(alertsButtonLayout);

    mainLayout->addWidget(alertsGroup);

    // Grupo de logs
    logsGroup = new QGroupBox("📋 Logs de Monitoramento");
    QVBoxLayout* logsLayout = new QVBoxLayout(logsGroup);

    monitoringLogs = new QTextEdit();
    monitoringLogs->setMaximumHeight(120);
    monitoringLogs->setReadOnly(true);
    monitoringLogs->setFontFamily("Consolas");
    monitoringLogs->setFontPointSize(9);
    monitoringLogs->setStyleSheet(
        "QTextEdit { "
        "   background-color: #1a1a1a; "
        "   color: #00ff00; "
        "   border: 1px solid #555; "
        "   border-radius: 3px; "
        "}"
    );

    logsLayout->addWidget(monitoringLogs);

    mainLayout->addWidget(logsGroup);

    // Conectar sinais
    connect(acknowledgeAlertButton, &QPushButton::clicked, this, &MonitoringWidget::onAlertAcknowledged);
    connect(healthCheckButton, &QPushButton::clicked, this, &MonitoringWidget::onHealthCheckRequested);
}

void MonitoringWidget::setupCharts() {
    // Configurações iniciais do gráfico já foram feitas no setupUI
}

void MonitoringWidget::startMonitoring() {
    if (monitoringActive) return;

    monitoringActive = true;
    updateTimer->start();

    monitoringLogs->append(QString("[%1] 📊 Monitoramento iniciado")
                          .arg(QDateTime::currentDateTime().toString("hh:mm:ss")));

    // Primeira atualização
    updateMetrics();
}

void MonitoringWidget::stopMonitoring() {
    monitoringActive = false;
    updateTimer->stop();

    monitoringLogs->append(QString("[%1] 🛑 Monitoramento parado")
                          .arg(QDateTime::currentDateTime().toString("hh:mm:ss")));
}

void MonitoringWidget::refreshData() {
    updateMetrics();
}

void MonitoringWidget::updateMetrics() {
    if (!monitoringActive) return;

    updateSystemMetrics();
    updateDeviceMetrics();
    updateStreamingMetrics();
    updateAlertsDisplay();
    updateHealthStatus();
}

void MonitoringWidget::updateSystemMetrics() {
    try {
        auto metrics = MetricsCollector::getInstance().getSystemMetrics();

        // CPU
        cpuUsageLabel->setText(formatPercentage(metrics.cpu_usage_percent));
        cpuProgress->setValue(static_cast<int>(metrics.cpu_usage_percent));

        // Memória
        QString memoryText = QString("%1 / %2")
                           .arg(formatBytes(metrics.memory_used_bytes))
                           .arg(formatBytes(metrics.memory_total_bytes));
        memoryUsageLabel->setText(memoryText);

        double memoryPercent = metrics.memory_total_bytes > 0 ?
            (static_cast<double>(metrics.memory_used_bytes) / metrics.memory_total_bytes) * 100.0 : 0.0;
        memoryProgress->setValue(static_cast<int>(memoryPercent));

        // Disco
        QString diskText = QString("%1 / %2")
                         .arg(formatBytes(metrics.disk_used_bytes))
                         .arg(formatBytes(metrics.disk_total_bytes));
        diskUsageLabel->setText(diskText);

        double diskPercent = metrics.disk_total_bytes > 0 ?
            (static_cast<double>(metrics.disk_used_bytes) / metrics.disk_total_bytes) * 100.0 : 0.0;
        diskProgress->setValue(static_cast<int>(diskPercent));

        // Uptime
        uptimeLabel->setText(formatDuration(metrics.uptime_seconds));

        // Atualizar gráfico de CPU
        qint64 currentTime = QDateTime::currentDateTime().toMSecsSinceEpoch();
        cpuSeries->append(currentTime, metrics.cpu_usage_percent);

        // Manter apenas os últimos 60 pontos (2 minutos)
        while (cpuSeries->count() > 60) {
            cpuSeries->remove(0);
        }

        // Atualizar eixos do gráfico
        if (cpuChartView && cpuChartView->chart()) {
            cpuChartView->chart()->axes(Qt::Horizontal).first()->setRange(
                QDateTime::fromMSecsSinceEpoch(currentTime - 120000), // 2 minutos atrás
                QDateTime::fromMSecsSinceEpoch(currentTime)
            );
        }

    } catch (const std::exception& e) {
        monitoringLogs->append(QString("[%1] ❌ Erro ao atualizar métricas do sistema: %2")
                              .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
                              .arg(e.what()));
    }
}

void MonitoringWidget::updateDeviceMetrics() {
    try {
        auto deviceMetrics = MetricsCollector::getInstance().getDeviceMetrics();

        int connectedCount = 0;
        int streamingCount = 0;

        devicesTable->setRowCount(0);

        for (const auto& device : deviceMetrics) {
            if (device.connected) connectedCount++;
            if (device.active_streams > 0) streamingCount++;

            // Adicionar à tabela
            int row = devicesTable->rowCount();
            devicesTable->insertRow(row);

            devicesTable->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(device.device_id)));
            devicesTable->setItem(row, 1, new QTableWidgetItem("Device")); // Simplificado
            devicesTable->setItem(row, 2, new QTableWidgetItem(QString("%1%").arg(device.battery_level)));
            devicesTable->setItem(row, 3, new QTableWidgetItem(device.connected ? "🟢 Conectado" : "🔴 Desconectado"));
        }

        connectedDevicesLabel->setText(QString("Conectados: %1").arg(connectedCount));
        totalDevicesLabel->setText(QString("Total: %1").arg(deviceMetrics.size()));
        streamingDevicesLabel->setText(QString("Streaming: %1").arg(streamingCount));

    } catch (const std::exception& e) {
        monitoringLogs->append(QString("[%1] ❌ Erro ao atualizar métricas de dispositivos: %2")
                              .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
                              .arg(e.what()));
    }
}

void MonitoringWidget::updateStreamingMetrics() {
    try {
        auto streamingMetrics = MetricsCollector::getInstance().getStreamingMetrics();

        activeStreamsLabel->setText(QString("Streams Ativos: %1").arg(streamingMetrics.total_active_streams));
        averageBitrateLabel->setText(QString("Bitrate Médio: %.1f Mbps").arg(streamingMetrics.average_bitrate_mbps));
        averageLatencyLabel->setText(QString("Latência Média: %.1f ms").arg(streamingMetrics.average_latency_ms));
        streamSuccessRateLabel->setText(QString("Taxa de Sucesso: %.1f%")
                                       .arg(streamingMetrics.stream_success_rate * 100.0));

    } catch (const std::exception& e) {
        monitoringLogs->append(QString("[%1] ❌ Erro ao atualizar métricas de streaming: %2")
                              .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
                              .arg(e.what()));
    }
}

void MonitoringWidget::updateAlertsDisplay() {
    try {
        auto activeAlerts = AlertsManager::getInstance().getActiveAlerts();

        alertsTable->setRowCount(0);

        for (const auto& alert : activeAlerts) {
            int row = alertsTable->rowCount();
            alertsTable->insertRow(row);

            // Horário
            QString timeStr = QDateTime::fromMSecsSinceEpoch(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    alert.createdAt.time_since_epoch()).count()
            ).toString("hh:mm:ss");

            // Severidade com cor
            QString severityStr = QString::fromStdString(
                AlertsManager::getInstance().getStatusDescription(alert.severity));
            QTableWidgetItem* severityItem = new QTableWidgetItem(severityStr);

            if (alert.severity == AlertSeverity::CRITICAL) {
                severityItem->setBackground(QBrush(QColor("#ff4444")));
                severityItem->setForeground(QBrush(QColor("#ffffff")));
            } else if (alert.severity == AlertSeverity::HIGH) {
                severityItem->setBackground(QBrush(QColor("#ff8800")));
            } else if (alert.severity == AlertSeverity::MEDIUM) {
                severityItem->setBackground(QBrush(QColor("#ffaa00")));
            }

            alertsTable->setItem(row, 0, new QTableWidgetItem(timeStr));
            alertsTable->setItem(row, 1, severityItem);
            alertsTable->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(alert.message)));
            alertsTable->setItem(row, 3, new QTableWidgetItem(QString::fromStdString(
                AlertsManager::getInstance().getStatusDescription(alert.status))));
        }

    } catch (const std::exception& e) {
        monitoringLogs->append(QString("[%1] ❌ Erro ao atualizar alertas: %2")
                              .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
                              .arg(e.what()));
    }
}

void MonitoringWidget::updateHealthStatus() {
    try {
        auto healthStatus = HealthChecker::getInstance().getCurrentStatus();
        QString statusStr = QString::fromStdString(
            HealthChecker::getInstance().getStatusDescription(healthStatus));

        overallHealthLabel->setText(QString("Status: %1").arg(statusStr));

        // Definir cor baseada no status
        QString color = getHealthStatusColor(statusStr);
        overallHealthLabel->setStyleSheet(QString("font-weight: bold; color: %1;").arg(color));

        // Última verificação
        auto stats = HealthChecker::getInstance().getStats();
        QString lastCheckStr = QDateTime::fromMSecsSinceEpoch(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                stats.lastCheckTime.time_since_epoch()).count()
        ).toString("hh:mm:ss");

        lastHealthCheckLabel->setText(QString("Última verificação: %1").arg(lastCheckStr));

    } catch (const std::exception& e) {
        monitoringLogs->append(QString("[%1] ❌ Erro ao atualizar status de saúde: %2")
                              .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
                              .arg(e.what()));
    }
}

void MonitoringWidget::onAlertAcknowledged() {
    int currentRow = alertsTable->currentRow();
    if (currentRow < 0) {
        QMessageBox::warning(this, "Aviso", "Selecione um alerta para reconhecer.");
        return;
    }

    // Obter ID do alerta da tabela (simplificado)
    QTableWidgetItem* timeItem = alertsTable->item(currentRow, 0);
    if (timeItem) {
        // Em implementação real, manteríamos IDs dos alertas
        monitoringLogs->append(QString("[%1] ✅ Alerta reconhecido")
                              .arg(QDateTime::currentDateTime().toString("hh:mm:ss")));

        // Remover da tabela
        alertsTable->removeRow(currentRow);
    }
}

void MonitoringWidget::onHealthCheckRequested() {
    healthCheckButton->setEnabled(false);
    healthCheckButton->setText("🔄 Verificando...");

    // Executar verificação em thread separada
    QTimer::singleShot(100, [this]() {
        try {
            auto status = HealthChecker::getInstance().performHealthCheck();
            QString statusStr = QString::fromStdString(
                HealthChecker::getInstance().getStatusDescription(status));

            monitoringLogs->append(QString("[%1] 🔍 Verificação de saúde concluída: %2")
                                  .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
                                  .arg(statusStr));

        } catch (const std::exception& e) {
            monitoringLogs->append(QString("[%1] ❌ Erro na verificação de saúde: %2")
                                  .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
                                  .arg(e.what()));
        }

        healthCheckButton->setEnabled(true);
        healthCheckButton->setText("🔄 Verificar Agora");
    });
}

// ========== HELPERS ==========

QString MonitoringWidget::formatBytes(uint64_t bytes) const {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unitIndex = 0;
    double value = static_cast<double>(bytes);

    while (value >= 1024.0 && unitIndex < 4) {
        value /= 1024.0;
        unitIndex++;
    }

    return QString("%1 %2").arg(value, 0, 'f', 1).arg(units[unitIndex]);
}

QString MonitoringWidget::formatPercentage(double value) const {
    return QString("%1%").arg(value, 0, 'f', 1);
}

QString MonitoringWidget::formatDuration(std::chrono::seconds duration) const {
    auto hours = std::chrono::duration_cast<std::chrono::hours>(duration);
    auto minutes = std::chrono::duration_cast<std::chrono::minutes>(duration - hours);
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration - hours - minutes);

    return QString("%1:%2:%3")
        .arg(hours.count(), 2, 10, QChar('0'))
        .arg(minutes.count(), 2, 10, QChar('0'))
        .arg(seconds.count(), 2, 10, QChar('0'));
}

QString MonitoringWidget::getSeverityColor(const QString& severity) const {
    if (severity == "CRITICAL") return "#ff4444";
    if (severity == "HIGH") return "#ff8800";
    if (severity == "MEDIUM") return "#ffaa00";
    if (severity == "LOW") return "#44ff44";
    return "#888888";
}

QString MonitoringWidget::getHealthStatusColor(const QString& status) const {
    if (status == "HEALTHY") return "#44ff44";
    if (status == "DEGRADED") return "#ffaa00";
    if (status == "UNHEALTHY") return "#ff4444";
    return "#888888";
}

} // namespace AndroidStreamManager