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

        screenCaptureCallback = new ScreenCaptureCallback();
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

        Log.i(TAG, "Captura de tela iniciada");
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
    private static class ScreenCaptureCallback implements android.view.SurfaceHolder.Callback {
        // Implementação básica - em produção implementar captura real
        @Override
        public void surfaceCreated(android.view.SurfaceHolder holder) {}

        @Override
        public void surfaceChanged(android.view.SurfaceHolder holder, int format, int width, int height) {}

        @Override
        public void surfaceDestroyed(android.view.SurfaceHolder holder) {}

        public android.view.Surface getSurface() {
            // Retornar surface para captura
            return null; // Implementar adequadamente
        }

        public void release() {
            // Limpar recursos
        }
    }
}