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
import android.graphics.Color;
import java.util.concurrent.LinkedBlockingQueue;
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

    // Monitoramento de Bitrate Dinâmico
    private Handler monitoringHandler;
    private HandlerThread monitoringThread;
    private Runnable bitrateAdjustmentRunnable;

    // Buffer Pooling para reduzir GC
    private static final int BITMAP_POOL_SIZE = 3;
    private final LinkedBlockingQueue<Bitmap> bitmapPool = new LinkedBlockingQueue<>();
    private final LinkedBlockingQueue<byte[]> byteBufferPool = new LinkedBlockingQueue<>();

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
        initializeBitrateMonitoring();
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

    private void initializeBitrateMonitoring() {
        monitoringThread = new HandlerThread("BitrateMonitor");
        monitoringThread.start();
        monitoringHandler = new Handler(monitoringThread.getLooper());

        bitrateAdjustmentRunnable = new Runnable() {
            @Override
            public void run() {
                if (!isStreaming.get()) {
                    return; // Para o runnable se o streaming parou
                }

                // Em um cenário real, obteríamos a qualidade da rede do NetworkManager
                // (ex: baseado em latência, perda de pacotes, ou tipo de conexão).
                // Aqui, simulamos uma qualidade de rede flutuante.
                int networkQuality = 50 + (int)(Math.random() * 50); // Simula qualidade entre 50% e 100%
                adjustVideoBitrate(networkQuality);

                monitoringHandler.postDelayed(this, 5000); // Reagendar para daqui a 5 segundos
            }
        };
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

        // Iniciar monitoramento de bitrate
        monitoringHandler.post(bitrateAdjustmentRunnable);
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

        if (monitoringHandler != null && bitrateAdjustmentRunnable != null) {
            monitoringHandler.removeCallbacks(bitrateAdjustmentRunnable);
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

        // Verificar saúde do sistema antes de processar
        if (!isSystemHealthy()) {
            Log.w(TAG, "System not healthy, skipping frame processing");
            return;
        }

        // Processar na thread de encoding para não bloquear UI
        encodingHandler.post(() -> {
            Bitmap bitmap = null;
            long startTime = System.currentTimeMillis();

            try {
                bitmap = convertImageToBitmap(image);
                if (bitmap != null) {
                    encodeFrameToH264(bitmap);

                    long processingTime = System.currentTimeMillis() - startTime;
                    if (processingTime > 100) { // Log se demorar mais que 100ms
                        Log.w(TAG, "Frame processing took " + processingTime + "ms");
                    }
                } else {
                    Log.e(TAG, "Failed to convert image to bitmap");
                    reportError("BITMAP_CONVERSION_FAILED");
                }

            } catch (OutOfMemoryError e) {
                Log.e(TAG, "Out of memory during frame processing", e);
                reportError("OUT_OF_MEMORY");
                cleanupResources();

            } catch (Exception e) {
                Log.e(TAG, "Unexpected error processing captured frame in background", e);
                reportError("FRAME_PROCESSING_FAILED");
            } finally {
                // Sempre liberar bitmap usando o pool
                releaseBitmapToPool(bitmap);
            }
        });
    }

    private boolean isSystemHealthy() {
        // Verificar saúde básica do sistema
        if (encodingHandler == null || !encodingHandler.getLooper().getThread().isAlive()) {
            Log.e(TAG, "Encoding thread not healthy");
            return false;
        }

        if (videoEncoder == null) {
            Log.e(TAG, "Video encoder not available");
            return false;
        }

        // Verificar uso de memória (simplificado)
        Runtime runtime = Runtime.getRuntime();
        long usedMemory = runtime.totalMemory() - runtime.freeMemory();
        long maxMemory = runtime.maxMemory();
        double memoryUsage = (double) usedMemory / maxMemory;

        if (memoryUsage > 0.85) { // Mais de 85% de memória usada
            Log.w(TAG, "High memory usage: " + (memoryUsage * 100) + "%");
            return false;
        }

        return true;
    }

    private void reportError(String errorCode) {
        frameStats.errors++;

        // Reportar erro para o servidor via WebSocket
        try {
            JSONObject errorMessage = new JSONObject();
            errorMessage.put("type", "error");
            errorMessage.put("code", errorCode);
            errorMessage.put("timestamp", System.currentTimeMillis());
            errorMessage.put("component", "streaming_service");
            errorMessage.put("framesSent", frameStats.framesSent);
            errorMessage.put("errors", frameStats.errors);

            NetworkManager.getInstance().sendMessage(errorMessage.toString());
            Log.w(TAG, "Error reported to server: " + errorCode + " (total errors: " + frameStats.errors + ")");

        } catch (JSONException e) {
            Log.e(TAG, "Error reporting error to server", e);
        }
    }

    private void cleanupResources() {
        Log.i(TAG, "Performing emergency cleanup");

        // Liberar pool de bitmaps
        try {
            for (Bitmap bitmap : bitmapPool) {
                if (bitmap != null && !bitmap.isRecycled()) {
                    bitmap.recycle();
                }
            }
            bitmapPool.clear();
        } catch (Exception e) {
            Log.e(TAG, "Error during bitmap pool cleanup", e);
        }

        // Forçar GC
        System.gc();
        System.runFinalization();
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

            int bitmapWidth = width + rowPadding / pixelStride;

            // Obter bitmap do pool ou criar novo
            Bitmap bitmap = acquireBitmapFromPool(bitmapWidth, height);
            if (bitmap == null) {
                bitmap = Bitmap.createBitmap(bitmapWidth, height, Bitmap.Config.ARGB_8888);
                Log.d(TAG, "Created new bitmap: " + bitmapWidth + "x" + height);
            } else {
                Log.d(TAG, "Reused bitmap from pool: " + bitmapWidth + "x" + height);
            }

            bitmap.copyPixelsFromBuffer(plane.getBuffer());
            return bitmap;

        } catch (Exception e) {
            Log.e(TAG, "Error converting Image to Bitmap", e);
            return null;
        }
    }

    private Bitmap acquireBitmapFromPool(int width, int height) {
        try {
            // Procurar bitmap compatível no pool
            for (Bitmap bitmap : bitmapPool) {
                if (!bitmap.isRecycled() && bitmap.getWidth() == width && bitmap.getHeight() == height) {
                    bitmapPool.remove(bitmap);
                    return bitmap;
                }
            }
        } catch (Exception e) {
            Log.e(TAG, "Error acquiring bitmap from pool", e);
        }
        return null;
    }

    private void releaseBitmapToPool(Bitmap bitmap) {
        try {
            if (bitmap != null && !bitmap.isRecycled() && bitmapPool.size() < BITMAP_POOL_SIZE) {
                // Limpar bitmap antes de devolver ao pool
                bitmap.eraseColor(Color.TRANSPARENT);
                bitmapPool.offer(bitmap);
                Log.d(TAG, "Bitmap returned to pool (size: " + bitmapPool.size() + ")");
            } else if (bitmap != null) {
                bitmap.recycle();
                Log.d(TAG, "Bitmap recycled (pool full)");
            }
        } catch (Exception e) {
            Log.e(TAG, "Error releasing bitmap to pool", e);
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

                    // Copiar pixels para o input buffer do encoder
                    ByteBuffer bitmapBuffer = ByteBuffer.allocate(bitmap.getByteCount());
                    bitmap.copyPixelsToBuffer(bitmapBuffer);
                    bitmapBuffer.rewind();
                    inputBuffer.put(bitmapBuffer);

                    // Timestamp para sincronização
                    long presentationTimeUs = System.nanoTime() / 1000;

                    videoEncoder.queueInputBuffer(
                        inputBufferIndex,
                        0,
                        bitmap.getByteCount(),
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
            // Criar mensagem otimizada (protocolo compacto)
            JSONObject frameMessage = new JSONObject();
            frameMessage.put("type", "video_frame");
            frameMessage.put("deviceId", "android_device"); // Identificar fonte
            frameMessage.put("ts", timestampUs / 1000); // converter para ms
            frameMessage.put("key", isKeyFrame);
            frameMessage.put("w", screenWidth);
            frameMessage.put("h", screenHeight);
            frameMessage.put("seq", frameSequenceNumber++);

            // Base64 otimizado (NO_WRAP + NO_PADDING = ~30% menos overhead)
            String base64Data = android.util.Base64.encodeToString(
                frameData, android.util.Base64.NO_WRAP | android.util.Base64.NO_PADDING);
            frameMessage.put("data", base64Data);

            // Enviar mensagem
            String jsonMessage = frameMessage.toString();
            NetworkManager.getInstance().sendMessage(jsonMessage);

            // Estatísticas otimizadas
            frameStats.bytesSent += jsonMessage.length();
            frameStats.framesSent++;

            // Log inteligente (keyframes ou a cada 50 frames)
            if (isKeyFrame || (frameSequenceNumber % 50) == 0) {
                Log.i(TAG, String.format("📡 Frame: %d bytes (key=%s, seq=%d) - Total: %d frames, %d KB",
                    frameData.length, isKeyFrame ? "YES" : "NO", frameSequenceNumber,
                    frameStats.framesSent, frameStats.bytesSent / 1024));
            }

        } catch (JSONException e) {
            Log.e(TAG, "Error creating frame message JSON", e);
            reportError("FRAME_JSON_ERROR");
        } catch (Exception e) {
            Log.e(TAG, "Error sending frame to server", e);
            reportError("FRAME_TRANSMISSION_ERROR");
        }
    }

    // Contador de sequência para debugging e sincronização
    private long frameSequenceNumber = 0;

    // Estatísticas de transmissão para monitoramento
    private static class FrameStats {
        long framesSent = 0;
        long bytesSent = 0;
        long errors = 0;
        long startTime = System.currentTimeMillis();

        double getAverageBitrate() {
            long elapsedSeconds = (System.currentTimeMillis() - startTime) / 1000;
            if (elapsedSeconds > 0) {
                return (bytesSent * 8.0) / elapsedSeconds / 1000.0; // kbps
            }
            return 0.0;
        }

        double getFps() {
            long elapsedSeconds = (System.currentTimeMillis() - startTime) / 1000;
            if (elapsedSeconds > 0) {
                return (double) framesSent / elapsedSeconds;
            }
            return 0.0;
        }
    }
    private final FrameStats frameStats = new FrameStats();

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

        if (monitoringThread != null) {
            monitoringThread.quitSafely();
            monitoringHandler = null;
            monitoringThread = null;
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