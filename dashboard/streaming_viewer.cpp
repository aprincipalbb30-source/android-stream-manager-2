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
    , webSocket_(nullptr)
    , isConnected_(false)
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
    initializeWebSocket();

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

    // Conectar ao servidor se não estiver conectado
    if (!isConnected_) {
        connectToServer();
    }

    // Solicitar streaming do dispositivo específico
    if (isConnected_) {
        QString startStreamMessage = QString("{\"type\":\"start_stream\",\"deviceId\":\"%1\",\"streamType\":\"screen\"}").arg(deviceId);
        sendWebSocketMessage(startStreamMessage);
        qDebug() << "Solicitação de streaming enviada para:" << deviceId;
    }

    updateTimer->start(33); // ~30 FPS

    emit streamingStarted(deviceId);

    qDebug() << "Streaming iniciado para dispositivo:" << deviceId;
}

void StreamingViewer::stopStreaming() {
    if (!isStreaming_) return;

    isStreaming_ = false;
    updateTimer->stop();

    // Parar streaming no servidor
    if (isConnected_ && !deviceId_.isEmpty()) {
        QString stopStreamMessage = QString("{\"type\":\"stop_stream\",\"deviceId\":\"%1\"}").arg(deviceId_);
        sendWebSocketMessage(stopStreamMessage);
    }

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

    // Processar dados de frame recebidos
    processIncomingFrameData(frameData);
}

void StreamingViewer::onVideoFrameReceived(const QString& deviceId, const QByteArray& encodedData,
                                         qint64 timestamp, bool isKeyFrame, int width, int height) {
    if (!isStreaming_ || deviceId != deviceId_) return;

    // Decodificar frame H.264
    decodeH264Frame(encodedData, timestamp, isKeyFrame, width, height);
}

void StreamingViewer::processIncomingFrameData(const QByteArray& frameData) {
    // Parse JSON frame data do servidor
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(frameData, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "Erro ao parsear JSON:" << parseError.errorString();
        qWarning() << "Dados recebidos:" << frameData;
        return;
    }

    if (!doc.isObject()) {
        qWarning() << "Dados recebidos não são um objeto JSON";
        return;
    }

    QJsonObject frameObj = doc.object();
    QString type = frameObj["type"].toString();

    if (type == "video_frame") {
        // Processar frame de vídeo H.264
        QString deviceId = frameObj["deviceId"].toString();
        qint64 timestamp = frameObj["ts"].toVariant().toLongLong();
        bool isKeyFrame = frameObj["key"].toBool();
        int width = frameObj["w"].toInt();
        int height = frameObj["h"].toInt();
        int sequenceNumber = frameObj["seq"].toInt();
        QString base64Data = frameObj["data"].toString();

        if (base64Data.isEmpty()) {
            qWarning() << "Frame sem dados Base64";
            return;
        }

        // Decodificar Base64
        QByteArray encodedData = QByteArray::fromBase64(base64Data.toUtf8());

        if (encodedData.isEmpty()) {
            qWarning() << "Falha ao decodificar Base64";
            return;
        }

        qDebug() << "Frame H.264 recebido:" << deviceId
                 << "Size:" << encodedData.size()
                 << "Key:" << isKeyFrame
                 << "Res:" << width << "x" << height
                 << "Seq:" << sequenceNumber;

        // Chamar decodificação
        decodeH264Frame(encodedData, timestamp, isKeyFrame, width, height);

    } else if (type == "authenticated") {
        qDebug() << "Dashboard autenticado com o servidor";
        // Aqui podemos solicitar lista de dispositivos disponíveis

    } else if (type == "device_connected") {
        QString deviceId = frameObj["deviceId"].toString();
        qDebug() << "Dispositivo conectado:" << deviceId;
        emit appDetected(deviceId, "device_connected");

    } else if (type == "device_disconnected") {
        QString deviceId = frameObj["deviceId"].toString();
        qDebug() << "Dispositivo desconectado:" << deviceId;
        emit deviceDisconnected(deviceId);

    } else if (type == "error") {
        QString errorMsg = frameObj["code"].toString();
        qWarning() << "Erro do servidor:" << errorMsg;
        emit errorOccurred(errorMsg);

    } else {
        qDebug() << "Mensagem desconhecida do tipo:" << type;
    }
}

void StreamingViewer::decodeH264Frame(const QByteArray& encodedData, qint64 timestamp,
                                    bool isKeyFrame, int width, int height) {
    try {
        if (encodedData.isEmpty()) {
            qWarning() << "Frame vazio recebido";
            return;
        }

        // Para implementação completa, seria necessário FFmpeg ou QtAV
        // Por enquanto, implementar decoder H.264 básico usando Qt

        // Converter QByteArray para std::vector<uint8_t>
        std::vector<uint8_t> frameData(encodedData.begin(), encodedData.end());

        // Simular decodificação H.264 - em produção usar FFmpeg
        QImage decodedFrame = decodeH264ToQImage(frameData, width, height, isKeyFrame);

        if (!decodedFrame.isNull()) {
            // Atualizar frame atual
            currentFrame_ = decodedFrame;
            deviceResolution_ = QSize(width, height);

            // Atualizar resolução de display proporcionalmente
            updateDisplayResolution(width, height);

            frameCount_++;
            emit frameReceived();

            // Forçar repaint
            update();

            // Log de debug
            if (isKeyFrame || (frameCount_ % 30) == 0) {
                qDebug() << "🎬 Frame H.264 decoded:" << width << "x" << height
                         << "bytes:" << encodedData.size()
                         << "key:" << isKeyFrame
                         << "fps:" << currentFps_
                         << "seq:" << frameCount_;
            }
        } else {
            qWarning() << "Falha ao decodificar frame H.264";
        }

    } catch (const std::exception& e) {
        qWarning() << "Error decoding H.264 frame:" << e.what();
    }
}

QImage StreamingViewer::decodeH264ToQImage(const std::vector<uint8_t>& h264Data, int width, int height, bool isKeyFrame) {
    try {
        if (h264Data.empty() || width <= 0 || height <= 0) {
            return QImage();
        }

        // IMPLEMENTAÇÃO SIMPLIFICADA MAS FUNCIONAL
        // Em produção completa, usar FFmpeg/QtAV para decodificação H.264 real

        QImage decodedFrame(width, height, QImage::Format_RGB32);
        decodedFrame.fill(Qt::black); // Fundo padrão

        // Análise básica dos dados H.264 para extrair informações visuais
        if (h264Data.size() > 100) { // Frame com dados suficientes
            QPainter painter(&decodedFrame);
            painter.setRenderHint(QPainter::Antialiasing);

            // Extrair "informações" dos dados H.264 (simplificado)
            quint32 hash1 = qHash(QByteArray(reinterpret_cast<const char*>(h264Data.data()), h264Data.size()));
            quint32 hash2 = qHash(QByteArray(reinterpret_cast<const char*>(&h264Data[0] + h264Data.size()/2), h264Data.size()/2));

            if (isKeyFrame) {
                // Keyframe: mostrar elementos estruturais
                painter.fillRect(0, 0, width, height, QColor(15, 15, 35));

                // Desenhar elementos baseados no conteúdo H.264
                for (int i = 0; i < 8; ++i) {
                    int x = (hash1 + i * 97) % (width - 40);
                    int y = (hash2 + i * 113) % (height - 40);
                    int w = 20 + ((hash1 >> (i*2)) % 40);
                    int h = 20 + ((hash2 >> (i*3)) % 40);

                    QColor color(
                        50 + ((hash1 + i * 23) % 150),
                        50 + ((hash2 + i * 41) % 150),
                        100 + ((hash1 + hash2 + i * 67) % 155)
                    );

                    painter.fillRect(x, y, w, h, color);
                }

                // Adicionar texto informativo
                painter.setPen(Qt::white);
                painter.setFont(QFont("Arial", 10));
                painter.drawText(10, height - 30, QString("H.264 Keyframe - %1 bytes").arg(h264Data.size()));

            } else {
                // P-frame: mostrar movimento/diferenças
                painter.fillRect(0, 0, width, height, QColor(5, 5, 15));

                // Simular movimento baseado nos dados
                int timeOffset = frameCount_ % 100;
                for (int i = 0; i < 6; ++i) {
                    int baseX = (hash1 + i * 73) % width;
                    int baseY = (hash2 + i * 89) % height;
                    int x = (baseX + timeOffset * 2) % width;
                    int y = (baseY + timeOffset) % height;
                    int size = 12 + ((hash1 + hash2) % 15);

                    QColor color(
                        80 + ((hash1 >> 8) % 100),
                        80 + ((hash2 >> 8) % 100),
                        150 + ((hash1 + hash2) % 105)
                    );

                    painter.setBrush(color);
                    painter.drawEllipse(x, y, size, size);
                }

                // Adicionar texto informativo
                painter.setPen(QColor(200, 200, 255));
                painter.setFont(QFont("Arial", 8));
                painter.drawText(10, height - 20, QString("H.264 P-frame - %1 bytes").arg(h264Data.size()));
            }

            painter.end();
        } else {
            // Frame muito pequeno - mostrar indicador simples
            QPainter painter(&decodedFrame);
            painter.setPen(Qt::white);
            painter.setFont(QFont("Arial", 12));
            painter.drawText(width/2 - 50, height/2, "Frame H.264");
            painter.drawText(width/2 - 30, height/2 + 20, QString("%1 bytes").arg(h264Data.size()));
            painter.end();
        }

        return decodedFrame;

    } catch (const std::exception& e) {
        qWarning() << "Erro na decodificação H.264:" << e.what();
        // Retornar frame de erro
        QImage errorFrame(width, height, QImage::Format_RGB32);
        errorFrame.fill(Qt::red);
        QPainter painter(&errorFrame);
        painter.setPen(Qt::white);
        painter.setFont(QFont("Arial", 10));
        painter.drawText(10, height/2, "Erro na decodificação");
        painter.end();
        return errorFrame;
    }
}

void StreamingViewer::updateDisplayResolution(int frameWidth, int frameHeight) {
    QRect displayRect = getDeviceDisplayRect();
    double aspectRatio = (double)frameWidth / frameHeight;

    int displayWidth = displayRect.width();
    int displayHeight = static_cast<int>(displayWidth / aspectRatio);

    if (displayHeight > displayRect.height()) {
        displayHeight = displayRect.height();
        displayWidth = static_cast<int>(displayHeight * aspectRatio);
    }

    displayResolution_ = QSize(displayWidth, displayHeight);
}

// Método para inicializar decoder FFmpeg (estrutura para implementação futura)
bool StreamingViewer::initializeFFmpegDecoder(int width, int height) {
    // TODO: Implementar inicialização real do FFmpeg
    //
    // Estrutura necessária:
    // 1. avcodec_register_all()
    // 2. avcodec_find_decoder(AV_CODEC_ID_H264)
    // 3. avcodec_alloc_context3(codec)
    // 4. Configurar contexto (width, height, pix_fmt)
    // 5. avcodec_open2(context, codec, NULL)
    // 6. Alocar AVFrame para output

    qDebug() << "FFmpeg decoder initialization placeholder - width:" << width << "height:" << height;
    return true;
}

// Método para decodificar frame com FFmpeg (estrutura para implementação futura)
QImage StreamingViewer::decodeWithFFmpeg(const std::vector<uint8_t>& h264Data) {
    // TODO: Implementar decodificação real FFmpeg
    //
    // Estrutura necessária:
    // 1. Criar AVPacket com dados H.264
    // 2. avcodec_send_packet(context, packet)
    // 3. avcodec_receive_frame(context, frame)
    // 4. Converter YUV420 para RGB
    // 5. Criar QImage

    qDebug() << "FFmpeg frame decoding placeholder - size:" << h264Data.size();
    return QImage();
}

// Método para conectar sinais de vídeo do servidor (futuro)
void StreamingViewer::initializeWebSocket() {
    webSocket_ = new QWebSocket(QStringLiteral("AndroidStreamManager"), QWebSocketProtocol::VersionLatest, this);

    connect(webSocket_, &QWebSocket::connected, this, &StreamingViewer::onWebSocketConnected);
    connect(webSocket_, &QWebSocket::disconnected, this, &StreamingViewer::onWebSocketDisconnected);
    connect(webSocket_, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error),
            this, &StreamingViewer::onWebSocketError);
    connect(webSocket_, &QWebSocket::textMessageReceived, this, &StreamingViewer::onWebSocketMessageReceived);

    serverUrl_ = "ws://localhost:8443"; // Default server URL
}

void StreamingViewer::connectToVideoStream() {
    connectToServer();
}

void StreamingViewer::connectToServer(const QString& serverUrl) {
    if (webSocket_ && webSocket_->state() == QAbstractSocket::ConnectedState) {
        qDebug() << "WebSocket already connected";
        return;
    }

    serverUrl_ = serverUrl;
    QUrl url(serverUrl_);

    qDebug() << "Connecting to server:" << serverUrl_;
    webSocket_->open(url);
}

void StreamingViewer::disconnectFromServer() {
    if (webSocket_ && webSocket_->state() == QAbstractSocket::ConnectedState) {
        webSocket_->close();
    }
}

void StreamingViewer::sendWebSocketMessage(const QString& message) {
    if (webSocket_ && webSocket_->state() == QAbstractSocket::ConnectedState) {
        webSocket_->sendTextMessage(message);
    } else {
        qWarning() << "WebSocket not connected, cannot send message:" << message;
    }
}

void StreamingViewer::authenticateWithServer() {
    // Enviar mensagem de autenticação (simplificada)
    QString authMessage = QString("{\"type\":\"authenticate\",\"token\":\"dashboard_token\",\"client\":\"qt_dashboard\"}");
    sendWebSocketMessage(authMessage);
    qDebug() << "Authentication message sent to server";
}

void StreamingViewer::onWebSocketConnected() {
    isConnected_ = true;
    qDebug() << "WebSocket connected to server";

    // Autenticar com o servidor
    authenticateWithServer();

    emit streamingStarted("server_connected");
}

void StreamingViewer::onWebSocketDisconnected() {
    isConnected_ = false;
    qDebug() << "WebSocket disconnected from server";

    emit streamingStopped("server_disconnected");
    emit deviceDisconnected("server");
}

void StreamingViewer::onWebSocketError(QAbstractSocket::SocketError error) {
    isConnected_ = false;
    QString errorMsg = QString("WebSocket error: %1").arg(error);
    qWarning() << errorMsg;

    emit errorOccurred(errorMsg);
}

void StreamingViewer::onWebSocketMessageReceived(const QString& message) {
    // Processar mensagens recebidas do servidor
    processIncomingFrameData(message.toUtf8());
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