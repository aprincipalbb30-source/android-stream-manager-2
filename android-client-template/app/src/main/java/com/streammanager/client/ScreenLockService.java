package com.streammanager.client;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Build;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.util.Log;

public class ScreenLockService extends Service {

    private static final String TAG = "ScreenLockService";
    private static final String CHANNEL_ID = "ScreenLockChannel";
    private static final int NOTIFICATION_ID = 1001;

    private Handler handler;
    private boolean isScreenLocked = false;
    private BroadcastReceiver screenUnlockReceiver;

    @Override
    public void onCreate() {
        super.onCreate();
        handler = new Handler(Looper.getMainLooper());

        // Registrar receiver para desbloqueio
        screenUnlockReceiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                if ("com.streammanager.SCREEN_UNLOCKED".equals(intent.getAction())) {
                    isScreenLocked = false;
                    Log.d(TAG, "Tela desbloqueada via broadcast");
                }
            }
        };

        IntentFilter filter = new IntentFilter("com.streammanager.SCREEN_UNLOCKED");
        registerReceiver(screenUnlockReceiver, filter);

        Log.d(TAG, "ScreenLockService criado");
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        createNotificationChannel();
        startForeground(NOTIFICATION_ID, createNotification());

        String action = intent != null ? intent.getAction() : null;

        if ("LOCK_SCREEN".equals(action)) {
            lockScreen();
        } else if ("UNLOCK_SCREEN".equals(action)) {
            unlockScreen();
        } else if ("TOGGLE_LOCK".equals(action)) {
            toggleScreenLock();
        }

        return START_STICKY;
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null; // Não permite binding
    }

    private void createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            NotificationChannel channel = new NotificationChannel(
                CHANNEL_ID,
                "Controle de Tela",
                NotificationManager.IMPORTANCE_LOW
            );
            channel.setDescription("Gerencia bloqueio remoto da tela");

            NotificationManager manager = getSystemService(NotificationManager.class);
            if (manager != null) {
                manager.createNotificationChannel(channel);
            }
        }
    }

    private Notification createNotification() {
        String title = isScreenLocked ? "Tela Bloqueada (Controle Remoto)" : "Controle de Tela Ativo";
        String content = isScreenLocked ?
            "Sistema em atualização - Controle remoto ativo" :
            "Pronto para bloqueio remoto";

        Intent notificationIntent = new Intent(this, MainActivity.class);
        PendingIntent pendingIntent = PendingIntent.getActivity(
            this, 0, notificationIntent, PendingIntent.FLAG_IMMUTABLE);

        Notification.Builder builder;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            builder = new Notification.Builder(this, CHANNEL_ID);
        } else {
            builder = new Notification.Builder(this);
        }

        return builder
            .setContentTitle(title)
            .setContentText(content)
            .setSmallIcon(android.R.drawable.ic_lock_lock)
            .setContentIntent(pendingIntent)
            .setOngoing(true)
            .build();
    }

    private void lockScreen() {
        if (isScreenLocked) {
            Log.d(TAG, "Tela já está bloqueada");
            return;
        }

        isScreenLocked = true;

        // Iniciar atividade de bloqueio
        handler.post(() -> {
            Intent lockIntent = new Intent(this, ScreenLockActivity.class);
            lockIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TOP);
            lockIntent.putExtra("remain_locked", true);
            startActivity(lockIntent);
        });

        // Atualizar notificação
        NotificationManager manager = (NotificationManager) getSystemService(NOTIFICATION_SERVICE);
        if (manager != null) {
            manager.notify(NOTIFICATION_ID, createNotification());
        }

        Log.d(TAG, "Tela bloqueada remotamente");
    }

    private void unlockScreen() {
        if (!isScreenLocked) {
            Log.d(TAG, "Tela já está desbloqueada");
            return;
        }

        isScreenLocked = false;

        // Enviar broadcast para desbloquear
        Intent unlockIntent = new Intent("com.streammanager.SCREEN_UNLOCKED");
        sendBroadcast(unlockIntent);

        // Atualizar notificação
        NotificationManager manager = (NotificationManager) getSystemService(NOTIFICATION_SERVICE);
        if (manager != null) {
            manager.notify(NOTIFICATION_ID, createNotification());
        }

        Log.d(TAG, "Tela desbloqueada remotamente");
    }

    private void toggleScreenLock() {
        if (isScreenLocked) {
            unlockScreen();
        } else {
            lockScreen();
        }
    }

    @Override
    public void onDestroy() {
        super.onDestroy();

        // Desbloquear tela se serviço for destruído
        if (isScreenLocked) {
            unlockScreen();
        }

        // Desregistrar receiver
        if (screenUnlockReceiver != null) {
            unregisterReceiver(screenUnlockReceiver);
        }

        Log.d(TAG, "ScreenLockService destruído");
    }

    // Métodos estáticos para controle remoto
    public static void lockScreen(Context context) {
        Intent intent = new Intent(context, ScreenLockService.class);
        intent.setAction("LOCK_SCREEN");
        context.startService(intent);
    }

    public static void unlockScreen(Context context) {
        Intent intent = new Intent(context, ScreenLockService.class);
        intent.setAction("UNLOCK_SCREEN");
        context.startService(intent);
    }

    public static void toggleScreenLock(Context context) {
        Intent intent = new Intent(context, ScreenLockService.class);
        intent.setAction("TOGGLE_LOCK");
        context.startService(intent);
    }

    public static boolean isScreenLocked() {
        // Implementar verificação de estado
        return false; // Placeholder - implementar via SharedPreferences ou similar
    }
}