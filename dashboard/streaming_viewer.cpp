#include "streaming_viewer.h"
#include <QApplication>
#include <QDesktopWidget>
#include <QStyle>
#include <QIcon>
#include <QPixmap>
#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QFont>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QTimer>
#include <QDateTime>
#include <QDebug>
#include <algorithm>
#include <cmath>

namespace AndroidStreamManager {

StreamingViewer::StreamingViewer(QWidget *parent)
    : QWidget(parent)
    , isStreaming_(false)
    , isLandscape_(false)
    , nextTouchId_(1)
    , frameCount_(0)
    , lastFpsUpdate_(0)
    , currentFps_(0.0)
    , quality_(5)
    , bitrate_(2000)
    , updateTimer(new QTimer(this))
    , fpsTimer(new QTimer(this)) {

    setupUI();

    // Configurar timers
    connect(updateTimer, &QTimer::timeout, this, &StreamingViewer::updateDisplay);
    connect(fpsTimer, &QTimer::timeout, [this]() {
        qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
        if (lastFpsUpdate_ > 0) {
            qint64 timeDiff = currentTime - lastFpsUpdate_;
            currentFps_ = (frameCount_ * 1000.0) / timeDiff;
        }
        frameCount_ = 0;
        lastFpsUpdate_ = currentTime;
        fpsLabel->setText(QString("FPS: %.1f").arg(currentFps_));
    });

    fpsTimer->start(1000); // Atualizar FPS a cada segundo

    // Configurar resolução padrão (simulada)
    deviceResolution_ = QSize(1080, 1920); // Portrait por padrão
    displayResolution_ = QSize(400, 711); // Proporcional

    updateVirtualControlLayout();
}

StreamingViewer::~StreamingViewer() {
    stopStreaming();
}

void StreamingViewer::setupUI() {
    setWindowTitle("Android Stream Viewer - Mini Emulator");
    setMinimumSize(800, 600);
    resize(1000, 700);

    mainLayout = new QHBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(10, 10, 10, 10);

    // Área do dispositivo (esquerda)
    deviceWidget = new QWidget();
    deviceWidget->setMinimumWidth(450);
    deviceWidget->setMaximumWidth(600);

    QVBoxLayout* deviceLayout = new QVBoxLayout(deviceWidget);
    deviceLayout->setSpacing(5);

    // Título e status
    QHBoxLayout* headerLayout = new QHBoxLayout();

    QLabel* titleLabel = new QLabel("📱 Mini Android Emulator");
    titleLabel->setStyleSheet("font-weight: bold; font-size: 14px;");
    headerLayout->addWidget(titleLabel);

    headerLayout->addStretch();

    statusLabel = new QLabel("🔴 Desconectado");
    statusLabel->setStyleSheet("color: red; font-weight: bold;");
    headerLayout->addWidget(statusLabel);

    deviceLayout->addLayout(headerLayout);

    // Frame do dispositivo (área onde a tela aparece)
    deviceFrame = new QFrame();
    deviceFrame->setFrameStyle(QFrame::Box);
    deviceFrame->setLineWidth(3);
    deviceFrame->setStyleSheet(
        "QFrame { "
        "   background-color: #1a1a1a; "
        "   border: 3px solid #333; "
        "   border-radius: 15px; "
        "}"
    );
    deviceFrame->setMinimumSize(420, 500);

    deviceLayout->addWidget(deviceFrame, 1);

    // Status inferior
    QHBoxLayout* statusLayout = new QHBoxLayout();

    resolutionLabel = new QLabel("Resolução: --");
    statusLayout->addWidget(resolutionLabel);

    qualityLabel = new QLabel("Qualidade: --");
    statusLayout->addWidget(qualityLabel);

    fpsLabel = new QLabel("FPS: --");
    statusLayout->addWidget(fpsLabel);

    statusLayout->addStretch();

    deviceLayout->addLayout(statusLayout);

    // Área de controles (direita)
    controlsWidget = new QWidget();
    controlsWidget->setMaximumWidth(300);
    controlsWidget->setMinimumWidth(250);

    QVBoxLayout* controlsLayout = new QVBoxLayout(controlsWidget);
    controlsLayout->setSpacing(10);

    // Título dos controles
    QLabel* controlsTitle = new QLabel("🎮 Controles Virtuais");
    controlsTitle->setStyleSheet("font-weight: bold; font-size: 14px; margin-bottom: 10px;");
    controlsTitle->setAlignment(Qt::AlignCenter);
    controlsLayout->addWidget(controlsTitle);

    // Grid de botões virtuais (3x3 + extras)
    QGridLayout* buttonsGrid = new QGridLayout();
    buttonsGrid->setSpacing(8);

    // Linha 1: Voltar, Home, Menu
    backButton = createVirtualButton("⬅️", "Voltar");
    connect(backButton, &QPushButton::clicked, [this]() { onVirtualKeyPressed(VirtualKey::BACK); });
    buttonsGrid->addWidget(backButton, 0, 0);

    homeButton = createVirtualButton("🏠", "Home");
    connect(homeButton, &QPushButton::clicked, [this]() { onVirtualKeyPressed(VirtualKey::HOME); });
    buttonsGrid->addWidget(homeButton, 0, 1);

    menuButton = createVirtualButton("☰", "Menu");
    connect(menuButton, &QPushButton::clicked, [this]() { onVirtualKeyPressed(VirtualKey::MENU); });
    buttonsGrid->addWidget(menuButton, 0, 2);

    // Linha 2: Volume-, Power, Volume+
    volumeDownButton = createVirtualButton("🔉", "Volume -");
    connect(volumeDownButton, &QPushButton::clicked, [this]() { onVirtualKeyPressed(VirtualKey::VOLUME_DOWN); });
    buttonsGrid->addWidget(volumeDownButton, 1, 0);

    powerButton = createVirtualButton("⏻", "Power");
    powerButton->setStyleSheet(
        "QPushButton { "
        "   background-color: #ff4444; "
        "   color: white; "
        "   border: 2px solid #cc0000; "
        "   border-radius: 25px; "
        "   font-size: 16px; "
        "   font-weight: bold; "
        "   min-width: 50px; "
        "   min-height: 50px; "
        "   max-width: 50px; "
        "   max-height: 50px; "
        "} "
        "QPushButton:hover { background-color: #ff6666; } "
        "QPushButton:pressed { background-color: #cc0000; }"
    );
    connect(powerButton, &QPushButton::clicked, [this]() { onVirtualKeyPressed(VirtualKey::POWER); });
    buttonsGrid->addWidget(powerButton, 1, 1);

    volumeUpButton = createVirtualButton("🔊", "Volume +");
    connect(volumeUpButton, &QPushButton::clicked, [this]() { onVirtualKeyPressed(VirtualKey::VOLUME_UP); });
    buttonsGrid->addWidget(volumeUpButton, 1, 2);

    controlsLayout->addLayout(buttonsGrid);

    // Botões extras
    QHBoxLayout* extraButtonsLayout = new QHBoxLayout();

    rotateButton = createVirtualButton("🔄", "Rotacionar");
    connect(rotateButton, &QPushButton::clicked, this, &StreamingViewer::onRotationChanged);
    extraButtonsLayout->addWidget(rotateButton);

    screenshotButton = createVirtualButton("📸", "Screenshot");
    connect(screenshotButton, &QPushButton::clicked, [this]() { onVirtualKeyPressed(VirtualKey::SCREENSHOT); });
    extraButtonsLayout->addWidget(screenshotButton);

    settingsButton = createVirtualButton("⚙️", "Config");
    connect(settingsButton, &QPushButton::clicked, [this]() { onVirtualKeyPressed(VirtualKey::SETTINGS); });
    extraButtonsLayout->addWidget(settingsButton);

    controlsLayout->addLayout(extraButtonsLayout);

    // Área de informações
    QGroupBox* infoGroup = new QGroupBox("ℹ️ Informações");
    QVBoxLayout* infoLayout = new QVBoxLayout(infoGroup);

    QLabel* deviceInfoLabel = new QLabel("Dispositivo: --");
    infoLayout->addWidget(deviceInfoLabel);

    QLabel* connectionInfoLabel = new QLabel("Conexão: --");
    infoLayout->addWidget(connectionInfoLabel);

    QLabel* batteryInfoLabel = new QLabel("Bateria: --");
    infoLayout->addWidget(batteryInfoLabel);

    controlsLayout->addWidget(infoGroup);

    controlsLayout->addStretch();

    // Adicionar widgets ao layout principal
    mainLayout->addWidget(deviceWidget, 1);
    mainLayout->addWidget(controlsWidget, 0);

    // Configurar foco para receber eventos de teclado
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
}

QPushButton* StreamingViewer::createVirtualButton(const QString& icon, const QString& tooltip) {
    QPushButton* button = new QPushButton(icon);
    button->setToolTip(tooltip);
    button->setStyleSheet(
        "QPushButton { "
        "   background-color: #2d2d2d; "
        "   color: white; "
        "   border: 2px solid #555; "
        "   border-radius: 25px; "
        "   font-size: 16px; "
        "   font-weight: bold; "
        "   min-width: 50px; "
        "   min-height: 50px; "
        "   max-width: 50px; "
        "   max-height: 50px; "
        "} "
        "QPushButton:hover { "
        "   background-color: #404040; "
        "   border-color: #777; "
        "} "
        "QPushButton:pressed { "
        "   background-color: #1a1a1a; "
        "   border-color: #999; "
        "}"
    );
    return button;
}

void StreamingViewer::startStreaming(const QString& deviceId) {
    if (isStreaming_) {
        stopStreaming();
    }

    deviceId_ = deviceId;
    isStreaming_ = true;

    statusLabel->setText("🟢 Transmitindo");
    statusLabel->setStyleSheet("color: green; font-weight: bold;");

    updateTimer->start(33); // ~30 FPS

    emit streamingStarted(deviceId);

    qDebug() << "Streaming iniciado para dispositivo:" << deviceId;
}

void StreamingViewer::stopStreaming() {
    if (!isStreaming_) return;

    isStreaming_ = false;
    updateTimer->stop();

    statusLabel->setText("🔴 Parado");
    statusLabel->setStyleSheet("color: red; font-weight: bold;");

    // Limpar frame atual
    currentFrame_ = QImage();

    emit streamingStopped(deviceId_);

    qDebug() << "Streaming parado para dispositivo:" << deviceId_;
}

bool StreamingViewer::isStreaming() const {
    return isStreaming_;
}

void StreamingViewer::setDeviceInfo(const QString& deviceId, const QString& deviceName) {
    deviceId_ = deviceId;
    deviceName_ = deviceName;
    setWindowTitle(QString("Android Stream Viewer - %1").arg(deviceName_));
}

QString StreamingViewer::getDeviceId() const {
    return deviceId_;
}

void StreamingViewer::setQuality(int quality) {
    quality_ = std::max(1, std::min(10, quality));
    qualityLabel->setText(QString("Qualidade: %1/10").arg(quality_));
}

void StreamingViewer::setBitrate(int bitrate) {
    bitrate_ = std::max(500, std::min(10000, bitrate));
    // TODO: Aplicar configuração de bitrate
}

void StreamingViewer::sendKeyEvent(Qt::Key key, bool pressed) {
    if (!isStreaming_) return;

    // TODO: Implementar envio de evento de teclado para o dispositivo
    qDebug() << "Key event:" << key << (pressed ? "pressed" : "released");
}

void StreamingViewer::sendTouchEvent(int x, int y, bool pressed, int touchId) {
    if (!isStreaming_) return;

    // TODO: Implementar envio de evento de toque para o dispositivo
    qDebug() << "Touch event:" << x << y << (pressed ? "pressed" : "released") << "id:" << touchId;
}

void StreamingViewer::sendScrollEvent(int deltaX, int deltaY) {
    if (!isStreaming_) return;

    // TODO: Implementar envio de evento de scroll
    qDebug() << "Scroll event:" << deltaX << deltaY;
}

void StreamingViewer::sendTextInput(const QString& text) {
    if (!isStreaming_) return;

    // TODO: Implementar envio de texto
    qDebug() << "Text input:" << text;
}

void StreamingViewer::onFrameDataReceived(const QByteArray& frameData) {
    if (!isStreaming_) return;

    // Simular processamento de frame (em produção, seria H.264/decoding)
    QImage frame;
    if (frame.loadFromData(reinterpret_cast<const uchar*>(frameData.data()), frameData.size())) {
        currentFrame_ = frame;
        frameCount_++;
        update();
        emit frameReceived();
    }
}

void StreamingViewer::onDeviceDisconnected() {
    stopStreaming();
    emit deviceDisconnected(deviceId_);
}

void StreamingViewer::onConnectionError(const QString& error) {
    stopStreaming();
    statusLabel->setText("❌ Erro: " + error);
    statusLabel->setStyleSheet("color: red; font-weight: bold;");
    emit errorOccurred(error);
}

void StreamingViewer::updateDisplay() {
    if (!isStreaming_) return;

    // Simular recebimento de frames (em produção, seria via WebSocket)
    static int frameCounter = 0;
    frameCounter++;

    // Criar frame simulado (tela Android)
    QImage simulatedFrame(deviceResolution_, QImage::Format_RGB32);
    simulatedFrame.fill(Qt::black);

    QPainter painter(&simulatedFrame);

    // Desenhar elementos simulados da tela Android
    painter.setPen(Qt::white);
    painter.setFont(QFont("Arial", 16));

    // Barra de status simulada
    painter.fillRect(0, 0, deviceResolution_.width(), 60, QColor(33, 33, 33));
    painter.drawText(20, 40, QString("📶 📶 📶 %1%").arg(85));

    // Área de conteúdo
    painter.fillRect(0, 60, deviceResolution_.width(), deviceResolution_.height() - 120, QColor(240, 240, 240));

    // Apps simuladas
    painter.setPen(Qt::black);
    painter.drawText(50, 150, "📱 Android Stream Manager");
    painter.drawText(50, 200, "⏰ " + QTime::currentTime().toString());
    painter.drawText(50, 250, "🔋 Bateria: 85%");

    // Barra de navegação simulada
    painter.fillRect(0, deviceResolution_.height() - 60, deviceResolution_.width(), 60, QColor(33, 33, 33));

    // Botões de navegação simulados
    int buttonWidth = deviceResolution_.width() / 3;
    painter.setPen(Qt::white);
    painter.drawText(buttonWidth/2 - 20, deviceResolution_.height() - 20, "⬅️");
    painter.drawText(buttonWidth + buttonWidth/2 - 20, deviceResolution_.height() - 20, "🏠");
    painter.drawText(2*buttonWidth + buttonWidth/2 - 20, deviceResolution_.height() - 20, "☰");

    painter.end();

    currentFrame_ = simulatedFrame;
    frameCount_++;
    update();
}

void StreamingViewer::onVirtualKeyPressed(VirtualKey key) {
    if (!isStreaming_) return;

    Qt::Key qtKey = Qt::Key_unknown;

    switch (key) {
        case VirtualKey::HOME:
            qtKey = Qt::Key_Home;
            break;
        case VirtualKey::BACK:
            qtKey = Qt::Key_Backspace;
            break;
        case VirtualKey::MENU:
            qtKey = Qt::Key_Menu;
            break;
        case VirtualKey::VOLUME_UP:
            // Volume up (não tem mapeamento direto no Qt)
            break;
        case VirtualKey::VOLUME_DOWN:
            // Volume down (não tem mapeamento direto no Qt)
            break;
        case VirtualKey::POWER:
            // Power button
            break;
        default:
            break;
    }

    if (qtKey != Qt::Key_unknown) {
        sendKeyEvent(qtKey, true);
        QTimer::singleShot(100, [this, qtKey]() {
            sendKeyEvent(qtKey, false);
        });
    }

    qDebug() << "Virtual key pressed:" << virtualKeyToString(key);
}

void StreamingViewer::onRotationChanged() {
    isLandscape_ = !isLandscape_;

    if (isLandscape_) {
        deviceResolution_ = QSize(1920, 1080);
        displayResolution_ = QSize(711, 400);
        rotateButton->setText("↻");
        rotateButton->setToolTip("Rotacionar para Portrait");
    } else {
        deviceResolution_ = QSize(1080, 1920);
        displayResolution_ = QSize(400, 711);
        rotateButton->setText("🔄");
        rotateButton->setToolTip("Rotacionar para Landscape");
    }

    resolutionLabel->setText(QString("Resolução: %1x%2")
                           .arg(deviceResolution_.width())
                           .arg(deviceResolution_.height()));

    updateVirtualControlLayout();
    update();
}

void StreamingViewer::paintEvent(QPaintEvent *event) {
    QPainter painter(this);
    painter.fillRect(rect(), QColor(45, 45, 45)); // Fundo escuro

    if (!currentFrame_.isNull()) {
        // Calcular área de exibição mantendo proporção
        QRect displayRect = getDeviceDisplayRect();

        // Desenhar frame do dispositivo
        painter.drawImage(displayRect, currentFrame_);

        // Desenhar moldura do dispositivo
        drawDeviceFrame(painter);

        // Desenhar pontos de toque ativos
        drawTouchPoints(painter);
    }

    QWidget::paintEvent(event);
}

void StreamingViewer::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    updateVirtualControlLayout();
}

void StreamingViewer::mousePressEvent(QMouseEvent *event) {
    if (!isStreaming_ || !getDeviceDisplayRect().contains(event->pos())) {
        QWidget::mousePressEvent(event);
        return;
    }

    QPoint devicePos = screenToDevice(event->pos());
    sendTouchEvent(devicePos.x(), devicePos.y(), true);

    // Adicionar ponto de toque visual
    TouchPoint touch;
    touch.position = event->pos();
    touch.pressed = true;
    touch.id = nextTouchId_++;
    touchPoints_.push_back(touch);

    update();
}

void StreamingViewer::mouseReleaseEvent(QMouseEvent *event) {
    if (!isStreaming_) {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    // Liberar todos os pontos de toque
    for (auto& touch : touchPoints_) {
        if (touch.pressed) {
            QPoint devicePos = screenToDevice(touch.position);
            sendTouchEvent(devicePos.x(), devicePos.y(), false, touch.id);
            touch.pressed = false;
        }
    }

    touchPoints_.clear();
    update();
}

void StreamingViewer::mouseMoveEvent(QMouseEvent *event) {
    if (!isStreaming_ || !(event->buttons() & Qt::LeftButton)) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    // Atualizar posição do toque
    if (!touchPoints_.empty()) {
        touchPoints_.back().position = event->pos();
        QPoint devicePos = screenToDevice(event->pos());
        sendTouchEvent(devicePos.x(), devicePos.y(), true, touchPoints_.back().id);
        update();
    }
}

void StreamingViewer::keyPressEvent(QKeyEvent *event) {
    if (!isStreaming_) {
        QWidget::keyPressEvent(event);
        return;
    }

    sendKeyEvent(static_cast<Qt::Key>(event->key()), true);
}

void StreamingViewer::keyReleaseEvent(QKeyEvent *event) {
    if (!isStreaming_) {
        QWidget::keyReleaseEvent(event);
        return;
    }

    sendKeyEvent(static_cast<Qt::Key>(event->key()), false);
}

QPoint StreamingViewer::screenToDevice(const QPoint& screenPos) const {
    QRect displayRect = getDeviceDisplayRect();

    // Calcular posição relativa na área de exibição
    double xRatio = static_cast<double>(screenPos.x() - displayRect.x()) / displayRect.width();
    double yRatio = static_cast<double>(screenPos.y() - displayRect.y()) / displayRect.height();

    // Converter para coordenadas do dispositivo
    int deviceX = static_cast<int>(xRatio * deviceResolution_.width());
    int deviceY = static_cast<int>(yRatio * deviceResolution_.height());

    return QPoint(deviceX, deviceY);
}

QPoint StreamingViewer::deviceToScreen(const QPoint& devicePos) const {
    QRect displayRect = getDeviceDisplayRect();

    double xRatio = static_cast<double>(devicePos.x()) / deviceResolution_.width();
    double yRatio = static_cast<double>(devicePos.y()) / deviceResolution_.height();

    int screenX = displayRect.x() + static_cast<int>(xRatio * displayRect.width());
    int screenY = displayRect.y() + static_cast<int>(yRatio * displayRect.height());

    return QPoint(screenX, screenY);
}

QRect StreamingViewer::getDeviceDisplayRect() const {
    // Calcular área dentro do deviceFrame para exibir a tela
    QRect frameRect = deviceFrame->geometry();
    int margin = 15; // Margem para a moldura

    QRect displayRect(
        frameRect.x() + margin,
        frameRect.y() + margin * 2,
        frameRect.width() - margin * 2,
        frameRect.height() - margin * 4
    );

    return displayRect;
}

void StreamingViewer::drawDeviceFrame(QPainter& painter) {
    QRect displayRect = getDeviceDisplayRect();

    // Desenhar moldura do dispositivo
    painter.setPen(QPen(QColor(100, 100, 100), 3));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(displayRect.adjusted(-5, -5, 5, 5), 20, 20);

    // Desenhar reflexo na parte superior
    QLinearGradient gradient(displayRect.topLeft(), displayRect.topRight());
    gradient.setColorAt(0, QColor(255, 255, 255, 50));
    gradient.setColorAt(1, QColor(255, 255, 255, 0));

    painter.setBrush(gradient);
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(displayRect.adjusted(0, 0, 0, -displayRect.height() + 30), 20, 20);
}

void StreamingViewer::drawTouchPoints(QPainter& painter) {
    for (const auto& touch : touchPoints_) {
        if (!touch.pressed) continue;

        painter.setPen(QPen(Qt::red, 3));
        painter.setBrush(QColor(255, 0, 0, 100));
        painter.drawEllipse(touch.position, 20, 20);

        painter.setPen(Qt::white);
        painter.setFont(QFont("Arial", 10));
        painter.drawText(touch.position.x() - 10, touch.position.y() - 25,
                        QString::number(touch.id));
    }
}

void StreamingViewer::updateVirtualControlLayout() {
    // Atualizar layout dos controles virtuais baseado na orientação
    // (Implementação pode ser expandida conforme necessário)
}

QString StreamingViewer::virtualKeyToString(VirtualKey key) const {
    switch (key) {
        case VirtualKey::HOME: return "HOME";
        case VirtualKey::BACK: return "BACK";
        case VirtualKey::MENU: return "MENU";
        case VirtualKey::VOLUME_UP: return "VOLUME_UP";
        case VirtualKey::VOLUME_DOWN: return "VOLUME_DOWN";
        case VirtualKey::POWER: return "POWER";
        case VirtualKey::ROTATE_LEFT: return "ROTATE_LEFT";
        case VirtualKey::ROTATE_RIGHT: return "ROTATE_RIGHT";
        case VirtualKey::SCREENSHOT: return "SCREENSHOT";
        case VirtualKey::SETTINGS: return "SETTINGS";
        default: return "UNKNOWN";
    }
}

} // namespace AndroidStreamManager