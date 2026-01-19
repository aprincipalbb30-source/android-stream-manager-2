#ifndef APK_CONFIG_WIDGET_H
#define APK_CONFIG_WIDGET_H

#include <QWidget>
#include <QString>
#include <shared/apk_config.h>

class QLabel;
class QLineEdit;
class QSpinBox;
class QComboBox;
class QCheckBox;
class QPushButton;

class ApkConfigWidget : public QWidget {
    Q_OBJECT

public:
    explicit ApkConfigWidget(QWidget *parent = nullptr);
    ~ApkConfigWidget();

    // API principal
    ApkConfig getConfiguration() const;
    void setConfiguration(const ApkConfig& config);
    bool validateConfiguration();

    // Utilitários
    void resetToDefaults();
    void saveConfiguration(const QString& filePath);
    void loadConfiguration(const QString& filePath);

signals:
    void configurationChanged();
    void configurationValidated(bool isValid);
    void iconSelected(const QString& iconPath);
    void themeChanged(const QString& theme);

private slots:
    void onSelectIconClicked();
    void onThemeChanged(int index);
    void validatePackageName();

private:
    void setupUI();
    void setupConnections();
    void loadDefaultValues();

    // Helpers
    bool isValidPackageName(const QString& packageName) const;
    bool hasPermission(const std::vector<std::string>& permissions, const std::string& permission) const;
    void updateIconDisplay();

    // Widgets de informações básicas
    QLineEdit* appNameEdit;
    QLineEdit* packageNameEdit;
    QSpinBox* versionCodeSpin;
    QLineEdit* versionNameEdit;

    // Widgets de servidor
    QLineEdit* serverHostEdit;
    QSpinBox* serverPortSpin;

    // Widgets de aparência
    QLabel* iconPathLabel;
    QPushButton* selectIconButton;
    QComboBox* themeCombo;

    // Widgets de permissões
    QCheckBox* cameraCheck;
    QCheckBox* microphoneCheck;
    QCheckBox* storageCheck;
    QCheckBox* locationCheck;
    QCheckBox* internetCheck;
    QCheckBox* wakeLockCheck;

    // Widgets avançados
    QSpinBox* minSdkSpin;
    QSpinBox* targetSdkSpin;
    QSpinBox* compileSdkSpin;
    QCheckBox* enableProguardCheck;
    QCheckBox* enableDebugCheck;
    QCheckBox* enableAnalyticsCheck;

    // Botão de validação
    QPushButton* validateButton;

    // Estado interno
    QString selectedIconPath_;
};

#endif // APK_CONFIG_WIDGET_H