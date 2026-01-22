#include "apk_config_widget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QStandardPaths>
#include <iostream>

ApkConfigWidget::ApkConfigWidget(QWidget *parent)
    : QWidget(parent)
    , appNameEdit(nullptr)
    , packageNameEdit(nullptr)
    , versionCodeSpin(nullptr)
    , versionNameEdit(nullptr)
    , serverHostEdit(nullptr)
    , serverPortSpin(nullptr)
    , iconPathLabel(nullptr)
    , selectIconButton(nullptr) {

    setupUI();
    setupConnections();
    loadDefaultValues();
}

ApkConfigWidget::~ApkConfigWidget() {
    // Cleanup automático pelo Qt
}

void ApkConfigWidget::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Título
    QLabel* titleLabel = new QLabel("Configuração do APK");
    titleLabel->setStyleSheet("font-weight: bold; font-size: 16px; margin-bottom: 10px;");
    mainLayout->addWidget(titleLabel);

    // Grupo: Informações Básicas
    QGroupBox* basicGroup = new QGroupBox("Informações Básicas do App");
    QFormLayout* basicLayout = new QFormLayout(basicGroup);

    appNameEdit = new QLineEdit("My Streaming App");
    packageNameEdit = new QLineEdit("com.company.streaming");
    versionCodeSpin = new QSpinBox();
    versionCodeSpin->setRange(1, 999999);
    versionCodeSpin->setValue(1);
    versionNameEdit = new QLineEdit("1.0.0");

    // Validação do package name
    connect(packageNameEdit, &QLineEdit::textChanged, this, &ApkConfigWidget::validatePackageName);

    basicLayout->addRow("Nome do Aplicativo:", appNameEdit);
    basicLayout->addRow("Nome do Pacote:", packageNameEdit);
    basicLayout->addRow("Código da Versão:", versionCodeSpin);
    basicLayout->addRow("Nome da Versão:", versionNameEdit);

    mainLayout->addWidget(basicGroup);

    // Grupo: Configurações do Servidor
    QGroupBox* serverGroup = new QGroupBox("Configurações do Servidor");
    QFormLayout* serverLayout = new QFormLayout(serverGroup);

    serverHostEdit = new QLineEdit("stream-server.local");
    serverPortSpin = new QSpinBox();
    serverPortSpin->setRange(1, 65535);
    serverPortSpin->setValue(8443);

    serverLayout->addRow("Host do Servidor:", serverHostEdit);
    serverLayout->addRow("Porta do Servidor:", serverPortSpin);

    mainLayout->addWidget(serverGroup);

    // Grupo: Aparência
    QGroupBox* appearanceGroup = new QGroupBox("Aparência");
    QVBoxLayout* appearanceLayout = new QVBoxLayout(appearanceGroup);

    // Ícone do app
    QHBoxLayout* iconLayout = new QHBoxLayout();
    QLabel* iconLabel = new QLabel("Ícone do App:");
    iconPathLabel = new QLabel("Nenhum ícone selecionado");
    iconPathLabel->setStyleSheet("border: 1px solid #ccc; padding: 5px; background: #f9f9f9;");
    selectIconButton = new QPushButton("Selecionar...");

    iconLayout->addWidget(iconLabel);
    iconLayout->addWidget(iconPathLabel, 1);
    iconLayout->addWidget(selectIconButton);

    appearanceLayout->addLayout(iconLayout);

    // Tema
    QHBoxLayout* themeLayout = new QHBoxLayout();
    QLabel* themeLabel = new QLabel("Tema:");
    themeCombo = new QComboBox();
    themeCombo->addItem("Claro", "light");
    themeCombo->addItem("Escuro", "dark");
    themeCombo->addItem("Sistema", "system");
    themeCombo->setCurrentIndex(0);

    themeLayout->addWidget(themeLabel);
    themeLayout->addWidget(themeCombo);
    themeLayout->addStretch();

    appearanceLayout->addLayout(themeLayout);

    mainLayout->addWidget(appearanceGroup);

    // Grupo: Permissões
    QGroupBox* permissionsGroup = new QGroupBox("Permissões Necessárias");
    QVBoxLayout* permissionsLayout = new QVBoxLayout(permissionsGroup);

    // Permissões essenciais
    cameraCheck = new QCheckBox("Câmera (para captura de tela)");
    microphoneCheck = new QCheckBox("Microfone (para áudio)");
    storageCheck = new QCheckBox("Armazenamento (para cache e logs)");
    locationCheck = new QCheckBox("Localização (opcional)");
    internetCheck = new QCheckBox("Internet (essencial)");
    wakeLockCheck = new QCheckBox("Manter Tela Ligada");

    // Configurar valores padrão
    cameraCheck->setChecked(true);
    microphoneCheck->setChecked(true);
    storageCheck->setChecked(true);
    internetCheck->setChecked(true);
    internetCheck->setEnabled(false); // Sempre necessário
    wakeLockCheck->setChecked(true);

    permissionsLayout->addWidget(internetCheck);
    permissionsLayout->addWidget(cameraCheck);
    permissionsLayout->addWidget(microphoneCheck);
    permissionsLayout->addWidget(storageCheck);
    permissionsLayout->addWidget(locationCheck);
    permissionsLayout->addWidget(wakeLockCheck);

    mainLayout->addWidget(permissionsGroup);

    // Grupo: Opções Avançadas
    QGroupBox* advancedGroup = new QGroupBox("Opções Avançadas");
    advancedGroup->setCheckable(true);
    advancedGroup->setChecked(false); // Inicialmente recolhido

    QVBoxLayout* advancedLayout = new QVBoxLayout(advancedGroup);

    // Configurações de build
    QFormLayout* buildLayout = new QFormLayout();

    minSdkSpin = new QSpinBox();
    minSdkSpin->setRange(16, 34);
    minSdkSpin->setValue(23);

    targetSdkSpin = new QSpinBox();
    targetSdkSpin->setRange(16, 34);
    targetSdkSpin->setValue(33);

    compileSdkSpin = new QSpinBox();
    compileSdkSpin->setRange(16, 34);
    compileSdkSpin->setValue(33);

    buildLayout->addRow("Min SDK Version:", minSdkSpin);
    buildLayout->addRow("Target SDK Version:", targetSdkSpin);
    buildLayout->addRow("Compile SDK Version:", compileSdkSpin);

    advancedLayout->addLayout(buildLayout);

    // Opções de otimização
    enableProguardCheck = new QCheckBox("Habilitar ProGuard (otimização)");
    enableDebugCheck = new QCheckBox("Modo Debug");
    enableAnalyticsCheck = new QCheckBox("Habilitar Analytics");

    enableProguardCheck->setChecked(true);
    enableDebugCheck->setChecked(false);
    enableAnalyticsCheck->setChecked(false);

    advancedLayout->addWidget(enableProguardCheck);
    advancedLayout->addWidget(enableDebugCheck);
    advancedLayout->addWidget(enableAnalyticsCheck);

    mainLayout->addWidget(advancedGroup);

    // Botão de validação
    validateButton = new QPushButton("✅ Validar Configuração");
    validateButton->setStyleSheet(
        "QPushButton { background-color: #4CAF50; color: white; padding: 8px; "
        "border-radius: 4px; font-weight: bold; }"
        "QPushButton:hover { background-color: #45a049; }"
    );

    mainLayout->addWidget(validateButton);

    // Espaçamento final
    mainLayout->addStretch();
}

void ApkConfigWidget::setupConnections() {
    connect(selectIconButton, &QPushButton::clicked, this, &ApkConfigWidget::onSelectIconClicked);
    connect(validateButton, &QPushButton::clicked, this, &ApkConfigWidget::validateConfiguration);
    connect(themeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ApkConfigWidget::onThemeChanged);
}

void ApkConfigWidget::loadDefaultValues() {
    // Valores padrão já foram definidos na criação dos widgets
}

ApkConfig ApkConfigWidget::getConfiguration() const {
    ApkConfig config;

    // Informações básicas
    config.appName = appNameEdit->text().toStdString();
    config.packageName = packageNameEdit->text().toStdString();
    config.versionCode = versionCodeSpin->value();
    config.versionName = versionNameEdit->text().toStdString();

    // Servidor
    config.serverUrl = "wss://" + serverHostEdit->text().toStdString() + ":" +
                      std::to_string(serverPortSpin->value());

    // Permissões
    if (cameraCheck->isChecked()) config.permissions.push_back("CAMERA");
    if (microphoneCheck->isChecked()) config.permissions.push_back("RECORD_AUDIO");
    if (storageCheck->isChecked()) {
        config.permissions.push_back("READ_EXTERNAL_STORAGE");
        config.permissions.push_back("WRITE_EXTERNAL_STORAGE");
    }
    if (locationCheck->isChecked()) {
        config.permissions.push_back("ACCESS_FINE_LOCATION");
        config.permissions.push_back("ACCESS_COARSE_LOCATION");
    }
    if (wakeLockCheck->isChecked()) config.permissions.push_back("WAKE_LOCK");

    // Sempre adicionar INTERNET
    config.permissions.push_back("INTERNET");

    // SDK versions
    config.minSdkVersion = minSdkSpin->value();
    config.targetSdkVersion = targetSdkSpin->value();
    config.compileSdkVersion = compileSdkSpin->value();

    // Aparência
    config.iconPath = selectedIconPath_.toStdString();
    config.theme = themeCombo->currentData().toString().toStdString();

    // Opções
    config.enableDebug = enableDebugCheck->isChecked();
    config.enableProguard = enableProguardCheck->isChecked();

    return config;
}

void ApkConfigWidget::setConfiguration(const ApkConfig& config) {
    // Informações básicas
    appNameEdit->setText(QString::fromStdString(config.appName));
    packageNameEdit->setText(QString::fromStdString(config.packageName));
    versionCodeSpin->setValue(config.versionCode);
    versionNameEdit->setText(QString::fromStdString(config.versionName));

    // Servidor (parse da URL)
    // TODO: Implementar parsing da URL do servidor

    // Permissões
    cameraCheck->setChecked(hasPermission(config.permissions, "CAMERA"));
    microphoneCheck->setChecked(hasPermission(config.permissions, "RECORD_AUDIO"));
    storageCheck->setChecked(hasPermission(config.permissions, "READ_EXTERNAL_STORAGE"));
    locationCheck->setChecked(hasPermission(config.permissions, "ACCESS_FINE_LOCATION"));
    wakeLockCheck->setChecked(hasPermission(config.permissions, "WAKE_LOCK"));

    // SDK versions
    minSdkSpin->setValue(config.minSdkVersion);
    targetSdkSpin->setValue(config.targetSdkVersion);
    compileSdkSpin->setValue(config.compileSdkVersion);

    // Aparência
    selectedIconPath_ = QString::fromStdString(config.iconPath);
    updateIconDisplay();

    // Tema
    int themeIndex = themeCombo->findData(QString::fromStdString(config.theme));
    if (themeIndex >= 0) {
        themeCombo->setCurrentIndex(themeIndex);
    }

    // Opções
    enableDebugCheck->setChecked(config.enableDebug);
    enableProguardCheck->setChecked(config.enableProguard);
}

bool ApkConfigWidget::validateConfiguration() {
    QStringList errors;

    // Validar nome do app
    if (appNameEdit->text().trimmed().isEmpty()) {
        errors << "Nome do aplicativo não pode estar vazio";
    }

    // Validar package name
    QString packageName = packageNameEdit->text().trimmed();
    if (packageName.isEmpty()) {
        errors << "Nome do pacote não pode estar vazio";
    } else if (!isValidPackageName(packageName)) {
        errors << "Nome do pacote inválido (deve seguir padrão Java)";
    }

    // Validar servidor
    if (serverHostEdit->text().trimmed().isEmpty()) {
        errors << "Host do servidor não pode estar vazio";
    }

    // Validar versões do SDK
    int minSdk = minSdkSpin->value();
    int targetSdk = targetSdkSpin->value();
    int compileSdk = compileSdkSpin->value();

    if (targetSdk < minSdk) {
        errors << "Target SDK deve ser maior ou igual ao Min SDK";
    }

    if (compileSdk < targetSdk) {
        errors << "Compile SDK deve ser maior ou igual ao Target SDK";
    }

    // Validar permissões (pelo menos internet)
    if (!internetCheck->isChecked()) {
        errors << "Permissão de Internet é obrigatória";
    }

    // Mostrar resultado
    if (errors.isEmpty()) {
        QMessageBox::information(this, "Validação Bem-Sucedida",
                               "✅ Todas as configurações são válidas!\n\n"
                               "O APK pode ser construído com essas configurações.");
        emit configurationValidated(true);
        return true;
    } else {
        QString errorMessage = "❌ Problemas encontrados:\n\n" + errors.join("\n");
        QMessageBox::warning(this, "Validação Falhou", errorMessage);
        emit configurationValidated(false);
        return false;
    }
}

void ApkConfigWidget::resetToDefaults() {
    // Reset para valores padrão
    appNameEdit->setText("My Streaming App");
    packageNameEdit->setText("com.company.streaming");
    versionCodeSpin->setValue(1);
    versionNameEdit->setText("1.0.0");

    serverHostEdit->setText("stream-server.local");
    serverPortSpin->setValue(8443);

    selectedIconPath_.clear();
    updateIconDisplay();

    themeCombo->setCurrentIndex(0);

    cameraCheck->setChecked(true);
    microphoneCheck->setChecked(true);
    storageCheck->setChecked(true);
    locationCheck->setChecked(false);
    wakeLockCheck->setChecked(true);

    minSdkSpin->setValue(23);
    targetSdkSpin->setValue(33);
    compileSdkSpin->setValue(33);

    enableProguardCheck->setChecked(true);
    enableDebugCheck->setChecked(false);
    enableAnalyticsCheck->setChecked(false);

    emit configurationChanged();
}

void ApkConfigWidget::saveConfiguration(const QString& filePath) {
    // TODO: Implementar salvamento em arquivo JSON
    QMessageBox::information(this, "Salvar Configuração",
                           "Funcionalidade de salvar configuração em desenvolvimento.\n"
                           "Por enquanto, use as configurações atuais.");
}

void ApkConfigWidget::loadConfiguration(const QString& filePath) {
    // TODO: Implementar carregamento de arquivo JSON
    QMessageBox::information(this, "Carregar Configuração",
                           "Funcionalidade de carregar configuração em desenvolvimento.\n"
                           "Por enquanto, use os valores padrão.");
}

// ========== SLOTS PRIVADOS ==========

void ApkConfigWidget::onSelectIconClicked() {
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "Selecionar Ícone do Aplicativo",
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
        "Imagens (*.png *.jpg *.jpeg *.ico *.svg)"
    );

    if (!fileName.isEmpty()) {
        selectedIconPath_ = fileName;
        updateIconDisplay();

        emit configurationChanged();
        emit iconSelected(fileName);
    }
}

void ApkConfigWidget::onThemeChanged(int index) {
    QString theme = themeCombo->currentData().toString();
    emit themeChanged(theme);
    emit configurationChanged();
}

void ApkConfigWidget::validatePackageName() {
    QString packageName = packageNameEdit->text();
    bool isValid = isValidPackageName(packageName);

    if (isValid) {
        packageNameEdit->setStyleSheet("");
    } else {
        packageNameEdit->setStyleSheet("border: 1px solid red;");
    }
}

void ApkConfigWidget::updateIconDisplay() {
    if (selectedIconPath_.isEmpty()) {
        iconPathLabel->setText("Nenhum ícone selecionado");
    } else {
        // Mostrar apenas o nome do arquivo
        QFileInfo fileInfo(selectedIconPath_);
        iconPathLabel->setText(fileInfo.fileName());
    }
}

// ========== HELPERS ==========

bool ApkConfigWidget::isValidPackageName(const QString& packageName) const {
    // Regex simplificado para validar package name Java
    // Deve começar com letra, conter apenas letras, números, pontos e underscores
    // Não pode ter dois pontos consecutivos

    if (packageName.isEmpty()) return false;

    // Verificar caracteres válidos
    for (QChar c : packageName) {
        if (!c.isLetterOrNumber() && c != '.' && c != '_') {
            return false;
        }
    }

    // Verificar se não começa ou termina com ponto
    if (packageName.startsWith('.') || packageName.endsWith('.')) {
        return false;
    }

    // Verificar se não há dois pontos consecutivos
    if (packageName.contains("..")) {
        return false;
    }

    // Deve ter pelo menos um ponto
    if (!packageName.contains('.')) {
        return false;
    }

    return true;
}

bool ApkConfigWidget::hasPermission(const std::vector<std::string>& permissions,
                                   const std::string& permission) const {
    return std::find(permissions.begin(), permissions.end(), permission) != permissions.end();
}