#ifndef STREAMING_VIEWER_H
#define STREAMING_VIEWER_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QImage>
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QFrame>
#include <QSplitter>
#include <QWebSocket>
#include <QUrl>
#include <memory>
#include <vector>
#include <atomic>

QT_BEGIN_NAMESPACE
class QPaintEvent;
class QResizeEvent;
QT_END_NAMESPACE

extern "C" {
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

class QPaintEvent;
class QResizeEvent;
QT_END_NAMESPACE

namespace AndroidStreamManager {

enum class VirtualKey {
    HOME,
    BACK,
    MENU,
    VOLUME_UP,
    VOLUME_DOWN,
    POWER,
    ROTATE_LEFT,
    ROTATE_RIGHT,
    SCREENSHOT,
    SETTINGS
};

struct TouchPoint {
    QPoint position;
    bool pressed;
    int id;
};

class StreamingViewer : public QWidget {
    Q_OBJECT

public:
    explicit StreamingViewer(QWidget *parent = nullptr);
    ~StreamingViewer();

    void startStreaming(const QString& deviceId);
    void stopStreaming();
    bool isStreaming() const;

    void setDeviceInfo(const QString& deviceId, const QString& deviceName);
    QString getDeviceId() const;

    // Controle de qualidade
    void setQuality(int quality); // 1-10
    void setBitrate(int bitrate); // kbps

    // Controles manuais
    void sendKeyEvent(Qt::Key key, bool pressed);
    void sendTouchEvent(int x, int y, bool pressed, int touchId = 0);
    void sendScrollEvent(int deltaX, int deltaY);
    void sendTextInput(const QString& text);

signals:
    void streamingStarted(const QString& deviceId);
    void streamingStopped(const QString& deviceId);
    void frameReceived();
    void errorOccurred(const QString& error);
    void deviceDisconnected(const QString& deviceId);

public slots:
    void onFrameDataReceived(const QByteArray& frameData);
    void onVideoFrameReceived(const QString& deviceId, const QByteArray& encodedData,
                             qint64 timestamp, bool isKeyFrame, int width, int height);
    void onDeviceDisconnected();
    void onConnectionError(const QString& error);

    // WebSocket slots
    void onWebSocketConnected();
    void onWebSocketDisconnected();
    void onWebSocketError(QAbstractSocket::SocketError error);
    void onWebSocketMessageReceived(const QString& message);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

private slots:
    void updateDisplay();
    void onVirtualKeyPressed(VirtualKey key);
    void onRotationChanged();

private:
    void setupUI();
    void setupVirtualControls();
    void setupDeviceFrame();
    void updateVirtualControlLayout();

    // Conversão de coordenadas
    QPoint screenToDevice(const QPoint& screenPos) const;
    QPoint deviceToScreen(const QPoint& devicePos) const;
    QRect getDeviceDisplayRect() const;

    // Renderização
    void drawDeviceFrame(QPainter& painter);
    void drawVirtualControls(QPainter& painter);
    void drawTouchPoints(QPainter& painter);

    // Utilitários
    QString virtualKeyToString(VirtualKey key) const;
    QPixmap getVirtualKeyIcon(VirtualKey key) const;
    QColor getVirtualKeyColor(VirtualKey key) const;

    // Decodificação de vídeo
    void processIncomingFrameData(const QByteArray& frameData);
    void decodeH264Frame(const QByteArray& encodedData, qint64 timestamp,
                        bool isKeyFrame, int width, int height);
    void connectToVideoStream();
    
    // FFmpeg decoding
    bool initializeFFmpegDecoder(int width, int height);
    void cleanupFFmpegDecoder();
    QImage decodeWithFFmpeg(const std::vector<uint8_t>& h264Data);

    // Helper para converter AVFrame para QImage
    QImage avFrameToQImage(AVFrame* frame);

    // WebSocket connection
    void connectToServer(const QString& serverUrl = "ws://localhost:8443");
    void disconnectFromServer();
    void sendWebSocketMessage(const QString& message);
    void authenticateWithServer();

    // WebSocket para conectar ao servidor
    QWebSocket* webSocket_;
    bool isConnected_;
    QString serverUrl_;

    // Estado interno
    QString deviceId_;
    QString deviceName_;
    bool isStreaming_;
    bool isLandscape_;

    // Frame atual
    QImage currentFrame_;
    QSize deviceResolution_; // Resolução real do dispositivo
    QSize displayResolution_; // Resolução de exibição

    // Touch handling
    std::vector<TouchPoint> touchPoints_;
    int nextTouchId_;

    // Controles virtuais
    QRect virtualControlsArea_;
    std::map<VirtualKey, QRect> virtualKeyRects_;

    // UI Components
    QHBoxLayout* mainLayout;
    QWidget* deviceWidget;
    QWidget* controlsWidget;

    // Device display area
    QLabel* deviceLabel;
    QFrame* deviceFrame;

    // Virtual controls
    QPushButton* homeButton;
    QPushButton* backButton;
    QPushButton* menuButton;
    QPushButton* volumeUpButton;
    QPushButton* volumeDownButton;
    QPushButton* powerButton;
    QPushButton* rotateButton;
    QPushButton* screenshotButton;
    QPushButton* settingsButton;

    // Status
    QLabel* statusLabel;
    QLabel* resolutionLabel;
    QLabel* qualityLabel;
    QLabel* fpsLabel;

    // Timers
    QTimer* updateTimer;
    QTimer* fpsTimer;

    // Estatísticas
    int frameCount_;
    qint64 lastFpsUpdate_;
    double currentFps_;

    // Configurações
    int quality_;
    int bitrate_;

    // FFmpeg members
    AVCodecContext* codecContext_ = nullptr;
    const AVCodec* codec_ = nullptr;
    AVFrame* frame_ = nullptr;
    AVFrame* rgbFrame_ = nullptr;
    AVPacket* packet_ = nullptr;
    SwsContext* swsContext_ = nullptr;
    std::vector<uint8_t> rgbBuffer_;
};

} // namespace AndroidStreamManager

#endif // STREAMING_VIEWER_H