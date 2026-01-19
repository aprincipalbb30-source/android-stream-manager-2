#ifndef MONITORING_WIDGET_H
#define MONITORING_WIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QProgressBar>
#include <QTableWidget>
#include <QTextEdit>
#include <QTimer>
#include <QPushButton>
#include <QChartView>
#include <QLineSeries>
#include <QChart>
#include <QValueAxis>
#include <QDateTimeAxis>
#include <memory>
#include <vector>
#include <deque>

QT_BEGIN_NAMESPACE
class QChart;
class QLineSeries;
class QValueAxis;
class QDateTimeAxis;
QT_END_NAMESPACE

namespace AndroidStreamManager {

class MonitoringWidget : public QWidget {
    Q_OBJECT

public:
    explicit MonitoringWidget(QWidget *parent = nullptr);
    ~MonitoringWidget();

    void startMonitoring();
    void stopMonitoring();
    void refreshData();

signals:
    void alertTriggered(const QString& message, const QString& severity);

private slots:
    void updateMetrics();
    void onAlertAcknowledged();
    void onHealthCheckRequested();

private:
    void setupUI();
    void setupCharts();
    void setupAlertsTable();
    void updateSystemMetrics();
    void updateDeviceMetrics();
    void updateStreamingMetrics();
    void updateAlertsDisplay();
    void updateHealthStatus();

    // Helpers para formatação
    QString formatBytes(uint64_t bytes) const;
    QString formatPercentage(double value) const;
    QString formatDuration(std::chrono::seconds duration) const;
    QString getSeverityColor(const QString& severity) const;
    QString getHealthStatusColor(const QString& status) const;

    // Widgets principais
    QVBoxLayout* mainLayout;

    // Métricas do sistema
    QGroupBox* systemMetricsGroup;
    QLabel* cpuUsageLabel;
    QLabel* memoryUsageLabel;
    QLabel* diskUsageLabel;
    QLabel* uptimeLabel;
    QProgressBar* cpuProgress;
    QProgressBar* memoryProgress;
    QProgressBar* diskProgress;

    // Gráfico de CPU
    QChartView* cpuChartView;
    QLineSeries* cpuSeries;
    std::deque<QPointF> cpuHistory;

    // Métricas de dispositivos
    QGroupBox* deviceMetricsGroup;
    QLabel* connectedDevicesLabel;
    QLabel* totalDevicesLabel;
    QLabel* streamingDevicesLabel;
    QTableWidget* devicesTable;

    // Métricas de streaming
    QGroupBox* streamingMetricsGroup;
    QLabel* activeStreamsLabel;
    QLabel* averageBitrateLabel;
    QLabel* averageLatencyLabel;
    QLabel* streamSuccessRateLabel;

    // Alertas
    QGroupBox* alertsGroup;
    QTableWidget* alertsTable;
    QPushButton* acknowledgeAlertButton;

    // Status de saúde
    QGroupBox* healthGroup;
    QLabel* overallHealthLabel;
    QLabel* lastHealthCheckLabel;
    QPushButton* healthCheckButton;

    // Logs de monitoramento
    QGroupBox* logsGroup;
    QTextEdit* monitoringLogs;

    // Timer para atualização
    QTimer* updateTimer;

    // Estado interno
    bool monitoringActive;
    QString lastHealthStatus;
};

} // namespace AndroidStreamManager

#endif // MONITORING_WIDGET_H