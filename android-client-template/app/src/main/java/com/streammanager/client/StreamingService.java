package com.streammanager.client;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.PixelFormat;
import android.hardware.display.DisplayManager;
import android.hardware.display.VirtualDisplay;
import android.media.AudioFormat;
import android.media.AudioRecord;
import android.media.Image;
import android.media.ImageReader;
import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaFormat;
import android.media.MediaRecorder;
import android.media.projection.MediaProjection;
import android.media.projection.MediaProjectionManager;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.IBinder;
import android.os.Looper;
import android.os.Message;
import android.util.Log;
import android.view.Display;
import android.view.WindowManager;
import androidx.annotation.Nullable;
import androidx.core.app.NotificationCompat;
import org.json.JSONObject;
import org.json.JSONException;
import java.io.ByteArrayOutputStream;
import java.util.concurrent.atomic.AtomicBoolean;

public class StreamingService extends Service {
    private static final String TAG = "StreamingService";
    private static final String CHANNEL_ID = "StreamManagerService";
    private static final int NOTIFICATION_ID = 1001;
    private static final int MSG_START_SCREEN_CAPTURE = 1;
    private static final int MSG_STOP_SCREEN_CAPTURE = 2;
    private static final int MSG_START_AUDIO_CAPTURE = 3;
    private static final int MSG_STOP_AUDIO_CAPTURE = 4;

    private static StreamingService instance;

    // Componentes de captura
    private MediaProjectionManager projectionManager;
    private MediaProjection mediaProjection;
    private VirtualDisplay virtualDisplay;
    private ScreenCaptureCallback screenCaptureCallback;

    // H.264 Encoding
    private MediaCodec videoEncoder;
    private ImageReader imageReader;
    private Handler encodingHandler;
    private HandlerThread encodingThread;

    // Captura de áudio
    private AudioRecord audioRecord;
    private AudioCaptureThread audioCaptureThread;
    private AtomicBoolean isAudioCapturing = new AtomicBoolean(false);

    // Controle de streaming
    private AtomicBoolean isStreaming = new AtomicBoolean(false);
    private HandlerThread handlerThread;
    private ServiceHandler serviceHandler;

    // Configurações
    private int screenWidth;
    private int screenHeight;
    private int screenDensity;

    public static synchronized StreamingService getInstance() {
        return instance;
    }

    @Override
    public void onCreate() {
        super.onCreate();
        instance = this;

        Log.i(TAG, "StreamingService criado");

        // Inicializar componentes
        initializeScreenCapture();
        initializeAudioCapture();
        initializeVideoEncoding();
        createNotificationChannel();
        startForeground(NOTIFICATION_ID, createNotification());

        // Handler thread para operações em background
        handlerThread = new HandlerThread("StreamingService");
        handlerThread.start();
        serviceHandler = new ServiceHandler(handlerThread.getLooper());
    }

    private void initializeScreenCapture() {
        projectionManager = (MediaProjectionManager)
            getSystemService(Context.MEDIA_PROJECTION_SERVICE);

        // Obter dimensões da tela
        WindowManager wm = (WindowManager) getSystemService(Context.WINDOW_SERVICE);
        Display display = wm.getDefaultDisplay();
        android.graphics.Point size = new android.graphics.Point();
        display.getSize(size);
        screenWidth = size.x;
        screenHeight = size.y;
        screenDensity = getResources().getDisplayMetrics().densityDpi;

        Log.i(TAG, String.format("Tela: %dx%d @%d", screenWidth, screenHeight, screenDensity));
    }

    private void initializeAudioCapture() {
        int bufferSize = AudioRecord.getMinBufferSize(
            44100, // Sample rate
            AudioFormat.CHANNEL_IN_MONO,
            AudioFormat.ENCODING_PCM_16BIT
        );

        if (bufferSize > 0) {
            audioRecord = new AudioRecord(
                MediaRecorder.AudioSource.MIC,
                44100,
                AudioFormat.CHANNEL_IN_MONO,
                AudioFormat.ENCODING_PCM_16BIT,
                bufferSize * 2
            );
        }
    }

    private void initializeVideoEncoding() {
        // Criar thread dedicado para encoding de vídeo (background priority)
        encodingThread = new HandlerThread("VideoEncoding", android.os.Process.THREAD_PRIORITY_BACKGROUND);
        encodingThread.start();
        encodingHandler = new Handler(encodingThread.getLooper());

        // Inicializar encoder na thread de encoding
        encodingHandler.post(() -> {
            try {
                initializeEncoderInBackground();
            } catch (Exception e) {
                Log.e(TAG, "Failed to initialize video encoding in background", e);
                videoEncoder = null;
            }
        });
    }

    private void initializeEncoderInBackground() throws Exception {
        // Configurar MediaCodec para H.264 encoding
        videoEncoder = MediaCodec.createEncoderByType(MediaFormat.MIMETYPE_VIDEO_AVC);
        MediaFormat format = MediaFormat.createVideoFormat(
            MediaFormat.MIMETYPE_VIDEO_AVC,
            screenWidth,
            screenHeight
        );

        // Configurações otimizadas de encoding
        format.setInteger(MediaFormat.KEY_BIT_RATE, 1500000); // 1.5 Mbps (reduzido para estabilidade)
        format.setInteger(MediaFormat.KEY_FRAME_RATE, 25); // 25 FPS (reduzido para performance)
        format.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, 3); // Keyframe a cada 3 segundos
        format.setInteger(MediaFormat.KEY_COLOR_FORMAT,
            MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface);

        // Configurações de qualidade
        format.setInteger(MediaFormat.KEY_BITRATE_MODE, MediaCodecInfo.EncoderCapabilities.BITRATE_MODE_VBR);
        format.setInteger(MediaFormat.KEY_PROFILE, MediaCodecInfo.CodecProfileLevel.AVCProfileMain);
        format.setInteger(MediaFormat.KEY_LEVEL, MediaCodecInfo.CodecProfileLevel.AVCLevel31);

        videoEncoder.configure(format, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE);
        videoEncoder.start();

        Log.i(TAG, "Video encoder initialized successfully: " + screenWidth + "x" + screenHeight +
              " @25fps, 1.5Mbps, Main profile");
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        Log.i(TAG, "StreamingService iniciado");

        // Manter serviço vivo
        return START_STICKY;
    }

    public void handleControlCommand(String action, JSONObject params) {
        Log.i(TAG, "Comando de controle: " + action);

        Message msg = serviceHandler.obtainMessage();

        try {
            switch (action) {
                case "START_SCREEN_STREAM":
                    msg.what = MSG_START_SCREEN_CAPTURE;
                    msg.obj = params;
                    break;

                case "STOP_SCREEN_STREAM":
                    msg.what = MSG_STOP_SCREEN_CAPTURE;
                    break;

                case "START_AUDIO_STREAM":
                    msg.what = MSG_START_AUDIO_CAPTURE;
                    msg.obj = params;
                    break;

                case "STOP_AUDIO_STREAM":
                    msg.what = MSG_STOP_AUDIO_CAPTURE;
                    break;

                default:
                    Log.w(TAG, "Comando desconhecido: " + action);
                    return;
            }

            serviceHandler.sendMessage(msg);

        } catch (Exception e) {
            Log.e(TAG, "Erro ao processar comando", e);
        }
    }

    public void startStreaming(String streamType) {
        Log.i(TAG, "Iniciando streaming: " + streamType);

        switch (streamType) {
            case "screen":
                startScreenCapture();
                break;
            case "audio":
                startAudioCapture();
                break;
            case "both":
                startScreenCapture();
                startAudioCapture();
                break;
        }

        isStreaming.set(true);
        updateNotification("Streaming ativo: " + streamType);
    }

    public void stopStreaming() {
        Log.i(TAG, "Parando streaming");

        stopScreenCapture();
        stopAudioCapture();

        isStreaming.set(false);
        updateNotification("Streaming parado");
    }

    private void startScreenCapture() {
        if (mediaProjection == null) {
            Log.w(TAG, "MediaProjection não inicializado");
            return;
        }

        // Inicializar callback com ImageReader
        screenCaptureCallback = new ScreenCaptureCallback(this);
        screenCaptureCallback.initialize(screenWidth, screenHeight, screenDensity);

        // Criar VirtualDisplay com surface do ImageReader
        virtualDisplay = mediaProjection.createVirtualDisplay(
            "StreamManagerScreen",
            screenWidth,
            screenHeight,
            screenDensity,
            DisplayManager.VIRTUAL_DISPLAY_FLAG_PUBLIC,
            screenCaptureCallback.getSurface(),
            null,
            null
        );

        Log.i(TAG, "Captura de tela real iniciada: " + screenWidth + "x" + screenHeight);
    }

    private void stopScreenCapture() {
        if (virtualDisplay != null) {
            virtualDisplay.release();
            virtualDisplay = null;
        }

        if (screenCaptureCallback != null) {
            screenCaptureCallback.release();
            screenCaptureCallback = null;
        }

        // Parar encoder de vídeo
        if (videoEncoder != null) {
            try {
                videoEncoder.stop();
                videoEncoder.release();
            } catch (Exception e) {
                Log.e(TAG, "Error stopping video encoder", e);
            }
            videoEncoder = null;
        }

        Log.i(TAG, "Captura de tela parada");
    }

    private void startAudioCapture() {
        if (audioRecord == null) {
            Log.e(TAG, "AudioRecord não inicializado");
            return;
        }

        try {
            audioRecord.startRecording();
            isAudioCapturing.set(true);

            audioCaptureThread = new AudioCaptureThread();
            audioCaptureThread.start();

            Log.i(TAG, "Captura de áudio iniciada");

        } catch (Exception e) {
            Log.e(TAG, "Erro ao iniciar captura de áudio", e);
        }
    }

    private void stopAudioCapture() {
        isAudioCapturing.set(false);

        if (audioCaptureThread != null) {
            audioCaptureThread.interrupt();
            audioCaptureThread = null;
        }

        if (audioRecord != null) {
            try {
                audioRecord.stop();
            } catch (Exception e) {
                Log.e(TAG, "Erro ao parar AudioRecord", e);
            }
        }

        Log.i(TAG, "Captura de áudio parada");
    }

    // Método chamado quando um frame é capturado
    public void processCapturedFrame(Image image) {
        if (!isStreaming.get() || videoEncoder == null || image == null) {
            return;
        }

        // Processar na thread de encoding para não bloquear UI
        encodingHandler.post(() -> {
            Bitmap bitmap = null;
            try {
                bitmap = convertImageToBitmap(image);
                if (bitmap != null) {
                    encodeFrameToH264(bitmap);
                }

            } catch (Exception e) {
                Log.e(TAG, "Error processing captured frame in background", e);
            } finally {
                // Sempre liberar bitmap para evitar memory leaks
                if (bitmap != null) {
                    bitmap.recycle();
                }
            }
        });
    }

    private Bitmap convertImageToBitmap(Image image) {
        try {
            Image.Plane[] planes = image.getPlanes();
            if (planes.length == 0) return null;

            Image.Plane plane = planes[0];
            int width = image.getWidth();
            int height = image.getHeight();
            int pixelStride = plane.getPixelStride();
            int rowStride = plane.getRowStride();
            int rowPadding = rowStride - pixelStride * width;

            // Criar bitmap RGBA
            Bitmap bitmap = Bitmap.createBitmap(
                width + rowPadding / pixelStride,
                height,
                Bitmap.Config.ARGB_8888
            );

            bitmap.copyPixelsFromBuffer(plane.getBuffer());
            return bitmap;

        } catch (Exception e) {
            Log.e(TAG, "Error converting Image to Bitmap", e);
            return null;
        }
    }

    private void encodeFrameToH264(Bitmap bitmap) {
        if (videoEncoder == null || bitmap == null) return;

        try {
            // Obter input buffer do encoder
            int inputBufferIndex = videoEncoder.dequeueInputBuffer(10000); // 10ms timeout
            if (inputBufferIndex >= 0) {
                ByteBuffer inputBuffer = videoEncoder.getInputBuffer(inputBufferIndex);

                if (inputBuffer != null) {
                    inputBuffer.clear();

                    // Criar array de bytes do bitmap (RGBA)
                    int bitmapSize = bitmap.getByteCount();
                    byte[] bitmapBytes = new byte[bitmapSize];
                    ByteBuffer bitmapBuffer = ByteBuffer.allocate(bitmapSize);
                    bitmap.copyPixelsToBuffer(bitmapBuffer);
                    bitmapBuffer.rewind();
                    bitmapBuffer.get(bitmapBytes);

                    // Copiar para input buffer do encoder
                    inputBuffer.put(bitmapBytes);

                    // Timestamp para sincronização
                    long presentationTimeUs = System.nanoTime() / 1000;

                    videoEncoder.queueInputBuffer(
                        inputBufferIndex,
                        0,
                        bitmapBytes.length,
                        presentationTimeUs,
                        0
                    );

                    // Processar frames codificados
                    processEncodedFrames();
                }
            } else {
                Log.w(TAG, "No input buffer available for encoding");
            }

        } catch (Exception e) {
            Log.e(TAG, "Error encoding frame to H.264", e);
        }
    }

    // Método para ajustar bitrate dinamicamente baseado na qualidade da rede
    public void adjustVideoBitrate(int networkQuality) {
        if (videoEncoder == null) return;

        encodingHandler.post(() -> {
            try {
                Bundle params = new Bundle();

                // Ajustar bitrate baseado na qualidade da rede (0-100)
                int baseBitrate = 1000000; // 1 Mbps base
                int adjustedBitrate = baseBitrate * networkQuality / 100;
                adjustedBitrate = Math.max(500000, Math.min(3000000, adjustedBitrate)); // 0.5-3 Mbps

                params.putInt(MediaCodec.PARAMETER_KEY_VIDEO_BITRATE, adjustedBitrate);
                videoEncoder.setParameters(params);

                Log.d(TAG, "Video bitrate adjusted to: " + adjustedBitrate + " bps (quality: " + networkQuality + "%)");

            } catch (Exception e) {
                Log.e(TAG, "Error adjusting video bitrate", e);
            }
        });
    }

    private void processEncodedFrames() {
        MediaCodec.BufferInfo bufferInfo = new MediaCodec.BufferInfo();

        int outputBufferIndex = videoEncoder.dequeueOutputBuffer(bufferInfo, 0);
        while (outputBufferIndex >= 0) {
            try {
                ByteBuffer outputBuffer = videoEncoder.getOutputBuffer(outputBufferIndex);
                if (outputBuffer != null) {
                    byte[] frameData = new byte[bufferInfo.size];
                    outputBuffer.get(frameData);

                    // Enviar frame via WebSocket
                    sendFrameToServer(frameData, bufferInfo.presentationTimeUs,
                                    (bufferInfo.flags & MediaCodec.BUFFER_FLAG_KEY_FRAME) != 0);
                }

                videoEncoder.releaseOutputBuffer(outputBufferIndex, false);
                outputBufferIndex = videoEncoder.dequeueOutputBuffer(bufferInfo, 0);

            } catch (Exception e) {
                Log.e(TAG, "Error processing encoded frame", e);
                break;
            }
        }
    }

    private void sendFrameToServer(byte[] frameData, long timestampUs, boolean isKeyFrame) {
        if (frameData == null || frameData.length == 0) return;

        try {
            // Criar mensagem compacta de frame
            JSONObject frameMessage = new JSONObject();
            frameMessage.put("type", "video_frame");
            frameMessage.put("ts", timestampUs); // timestamp abreviado
            frameMessage.put("key", isKeyFrame); // isKeyFrame abreviado
            frameMessage.put("w", screenWidth);  // width abreviado
            frameMessage.put("h", screenHeight); // height abreviado
            frameMessage.put("seq", frameSequenceNumber++); // número de sequência

            // Usar Base64 mais eficiente com padding removido
            frameMessage.put("data", android.util.Base64.encodeToString(
                frameData, android.util.Base64.NO_WRAP | android.util.Base64.NO_PADDING));

            // Enviar via NetworkManager
            NetworkManager.getInstance().sendMessage(frameMessage.toString());

            // Log de debug (apenas para keyframes ou a cada 100 frames)
            if (isKeyFrame || (frameSequenceNumber % 100) == 0) {
                Log.d(TAG, "Frame sent: " + frameData.length + " bytes, key=" + isKeyFrame +
                      ", seq=" + frameSequenceNumber);
            }

        } catch (JSONException e) {
            Log.e(TAG, "Error creating frame message", e);
        } catch (Exception e) {
            Log.e(TAG, "Error sending frame to server", e);
        }
    }

    // Contador de sequência para debugging e sincronização
    private long frameSequenceNumber = 0;

    public void setMediaProjection(MediaProjection projection) {
        this.mediaProjection = projection;
        Log.i(TAG, "MediaProjection configurado");
    }

    private void createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            NotificationChannel channel = new NotificationChannel(
                CHANNEL_ID,
                "Stream Manager Service",
                NotificationManager.IMPORTANCE_LOW
            );
            channel.setDescription("Serviço de streaming em background");

            NotificationManager manager = getSystemService(NotificationManager.class);
            manager.createNotificationChannel(channel);
        }
    }

    private Notification createNotification() {
        Intent notificationIntent = new Intent(this, MainActivity.class);
        PendingIntent pendingIntent = PendingIntent.getActivity(
            this, 0, notificationIntent, PendingIntent.FLAG_IMMUTABLE);

        return new NotificationCompat.Builder(this, CHANNEL_ID)
            .setContentTitle("Stream Manager")
            .setContentText("Serviço de streaming ativo")
            .setSmallIcon(android.R.drawable.ic_menu_camera)
            .setContentIntent(pendingIntent)
            .setOngoing(true)
            .build();
    }

    private void updateNotification(String status) {
        Notification notification = new NotificationCompat.Builder(this, CHANNEL_ID)
            .setContentTitle("Stream Manager")
            .setContentText(status)
            .setSmallIcon(android.R.drawable.ic_menu_camera)
            .setOngoing(true)
            .build();

        NotificationManager manager = getSystemService(NotificationManager.class);
        manager.notify(NOTIFICATION_ID, notification);
    }

    @Nullable
    @Override
    public IBinder onBind(Intent intent) {
        return null; // Não é um serviço bound
    }

    @Override
    public void onDestroy() {
        super.onDestroy();

        stopStreaming();

        if (mediaProjection != null) {
            mediaProjection.stop();
        }

        if (handlerThread != null) {
            handlerThread.quitSafely();
        }

        if (encodingThread != null) {
            encodingThread.quitSafely();
        }

        instance = null;
        Log.i(TAG, "StreamingService destruído");
    }

    // Handler para operações em background
    private final class ServiceHandler extends Handler {
        public ServiceHandler(Looper looper) {
            super(looper);
        }

        @Override
        public void handleMessage(Message msg) {
            switch (msg.what) {
                case MSG_START_SCREEN_CAPTURE:
                    startScreenCapture();
                    break;
                case MSG_STOP_SCREEN_CAPTURE:
                    stopScreenCapture();
                    break;
                case MSG_START_AUDIO_CAPTURE:
                    startAudioCapture();
                    break;
                case MSG_STOP_AUDIO_CAPTURE:
                    stopAudioCapture();
                    break;
            }
        }
    }

    // Thread para captura de áudio
    private class AudioCaptureThread extends Thread {
        @Override
        public void run() {
            android.os.Process.setThreadPriority(android.os.Process.THREAD_PRIORITY_AUDIO);

            int bufferSize = 1024;
            short[] buffer = new short[bufferSize];

            while (isAudioCapturing.get() && !isInterrupted()) {
                try {
                    int readSize = audioRecord.read(buffer, 0, bufferSize);
                    if (readSize > 0) {
                        // Processar e enviar dados de áudio
                        sendAudioData(buffer, readSize);
                    }
                } catch (Exception e) {
                    Log.e(TAG, "Erro na captura de áudio", e);
                    break;
                }
            }
        }
    }

    private void sendAudioData(short[] data, int length) {
        // Compactar e enviar via NetworkManager
        try {
            // Compressão simples (em produção usar codec adequado)
            ByteArrayOutputStream baos = new ByteArrayOutputStream();
            for (int i = 0; i < length; i++) {
                baos.write(data[i] & 0xFF);
                baos.write((data[i] >> 8) & 0xFF);
            }

            JSONObject audioMessage = new JSONObject();
            audioMessage.put("type", "audio_data");
            audioMessage.put("data", baos.toByteArray());
            audioMessage.put("timestamp", System.currentTimeMillis());

            NetworkManager.getInstance().sendMessage(audioMessage.toString());

        } catch (JSONException e) {
            Log.e(TAG, "Erro ao enviar dados de áudio", e);
        }
    }

    // Classe para captura de tela (simplificada)
    private static class ScreenCaptureCallback implements ImageReader.OnImageAvailableListener {
        private final StreamingService service;
        private ImageReader imageReader;
        private android.view.Surface surface;

        public ScreenCaptureCallback(StreamingService service) {
            this.service = service;
        }

        public void initialize(int width, int height, int density) {
            imageReader = ImageReader.newInstance(width, height, PixelFormat.RGBA_8888, 2);
            imageReader.setOnImageAvailableListener(this, service.encodingHandler);

            surface = imageReader.getSurface();
            Log.i(TAG, "ScreenCaptureCallback initialized: " + width + "x" + height);
        }

        @Override
        public void onImageAvailable(ImageReader reader) {
            Image image = null;
            try {
                image = reader.acquireLatestImage();
                if (image != null) {
                    service.processCapturedFrame(image);
                }
            } catch (Exception e) {
                Log.e(TAG, "Error processing captured frame", e);
            } finally {
                if (image != null) {
                    image.close();
                }
            }
        }

        public android.view.Surface getSurface() {
            return surface;
        }

        public void release() {
            if (imageReader != null) {
                imageReader.close();
                imageReader = null;
            }
            surface = null;
            Log.i(TAG, "ScreenCaptureCallback released");
        }
    }
}