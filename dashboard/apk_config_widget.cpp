#include "apk_config_widget.h"
#include <QFormLayout>
#include <QFileDialog>
#include <QMessageBox>

namespace AndroidStreamManager {

ApkConfigWidget::ApkConfigWidget(QWidget *parent)
    : QWidget(parent) {
    setupUI();
}

void ApkConfigWidget::setupUI() {
    auto *layout = new QFormLayout(this);
    
    // Nome do aplicativo
    appNameEdit = new QLineEdit("Android Stream Client");
    layout->addRow("App Name:", appNameEdit);
    
    // Servidor
    serverHostEdit = new QLineEdit("stream.example.com");
    layout->addRow("Server Host:", serverHostEdit);
    
    serverPortSpin = new QSpinBox;
    serverPortSpin->setRange(1, 65535);
    serverPortSpin->setValue(443);
    layout->addRow("Server Port:", serverPortSpin);
    
    // Visibilidade
    visibilityCombo = new QComboBox;
    visibilityCombo->addItem("Full App", 
        static_cast<int>(ApkVisibility::FULL_APP));
    visibilityCombo->addItem("Minimal UI", 
        static_cast<int>(ApkVisibility::MINIMAL_UI));
    visibilityCombo->addItem("Foreground Service", 
        static_cast<int>(ApkVisibility::FOREGROUND_SERVICE));
    layout->addRow("Visibility:", visibilityCombo);
    
    // Permissões
    auto *permissionsGroup = new QGroupBox("Permissions");
    auto *permissionsLayout = new QVBoxLayout;
    
    cameraPermissionCheck = new QCheckBox("Camera");
    microphonePermissionCheck = new QCheckBox("Microphone");
    auto *storagePermissionCheck = new QCheckBox("Storage");
    auto *networkPermissionCheck = new QCheckBox("Network");
    
    permissionsLayout->addWidget(cameraPermissionCheck);
    permissionsLayout->addWidget(microphonePermissionCheck);
    permissionsLayout->addWidget(storagePermissionCheck);
    permissionsLayout->addWidget(networkPermissionCheck);
    permissionsGroup->setLayout(permissionsLayout);
    layout->addRow(permissionsGroup);
    
    // Persistência
    persistenceCheck = new QCheckBox("Enable Background Service");
    layout->addRow("Persistence:", persistenceCheck);
    
    // Ícone
    auto *iconLayout = new QHBoxLayout;
    iconPathEdit = new QLineEdit;
    auto *selectIconButton = new QPushButton("Select...");
    connect(selectIconButton, &QPushButton::clicked,
            this, &ApkConfigWidget::onSelectIconClicked);
    
    iconLayout->addWidget(iconPathEdit);
    iconLayout->addWidget(selectIconButton);
    layout->addRow("App Icon:", iconLayout);
}

ApkConfig ApkConfigWidget::getConfig() const {
    ApkConfig config;
    config.appName = appNameEdit->text().toStdString();
    config.serverHost = serverHostEdit->text().toStdString();
    config.serverPort = serverPortSpin->value();
    config.visibility = static_cast<ApkVisibility>(
        visibilityCombo->currentData().toInt());
    config.persistenceEnabled = persistenceCheck->isChecked();
    
    // Permissões
    if (cameraPermissionCheck->isChecked())
        config.permissions.push_back(Permission::CAMERA);
    if (microphonePermissionCheck->isChecked())
        config.permissions.push_back(Permission::MICROPHONE);
    
    config.iconPath = iconPathEdit->text().toStdString();
    config.createdBy = "Operator"; // Substituir por usuário real
    
    return config;
}

void ApkConfigWidget::onSelectIconClicked() {
    QString fileName = QFileDialog::getOpenFileName(
        this, "Select App Icon", "", "Images (*.png *.jpg *.ico)");
    if (!fileName.isEmpty()) {
        iconPathEdit->setText(fileName);
    }
}

} // namespace AndroidStreamManager