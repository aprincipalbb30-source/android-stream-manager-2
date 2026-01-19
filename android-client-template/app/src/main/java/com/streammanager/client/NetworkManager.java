package com.streammanager.client;

import android.content.Context;
import android.util.Log;
import org.java_websocket.client.WebSocketClient;
import org.java_websocket.handshake.ServerHandshake;
import org.json.JSONObject;
import org.json.JSONException;
import java.net.URI;
import java.net.URISyntaxException;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.TimeUnit;

public class NetworkManager {
    private static final String TAG = "NetworkManager";
    private static final String SERVER_URL = "wss://stream-server.local:8443/ws"; // Configurável
    private static final long RECONNECT_DELAY_MS = 5000; // 5 segundos
    private static final long HEARTBEAT_INTERVAL_MS = 30000; // 30 segundos

    private static NetworkManager instance;
    private WebSocketClient webSocketClient;
    private ConnectionCallback callback;
    private RemoteCommandCallback remoteCommandCallback;
    private Context context;
    private ScheduledExecutorService heartbeatExecutor;
    private AtomicBoolean isConnected = new AtomicBoolean(false);
    private AtomicBoolean isConnecting = new AtomicBoolean(false);
    private String deviceId;
    private String authToken;

    public interface ConnectionCallback {
        void onConnected();
        void onDisconnected();
        void onError(String error);
    }

    public interface RemoteCommandCallback {
        void onScreenLockCommand();
        void onScreenUnlockCommand();
        void onRemoteControlCommand(String action, String data);
    }

    private NetworkManager() {
        this.heartbeatExecutor = Executors.newSingleThreadScheduledExecutor();
    }

    public static synchronized NetworkManager getInstance() {
        if (instance == null) {
            instance = new NetworkManager();
        }
        return instance;
    }

    public void initialize(Context context, ConnectionCallback callback) {
        this.context = context;
        this.callback = callback;
        this.deviceId = generateDeviceId();

        Log.i(TAG, "Inicializando NetworkManager com deviceId: " + deviceId);
        connect();
    }

    public void setRemoteCommandCallback(RemoteCommandCallback callback) {
        this.remoteCommandCallback = callback;
    }

    private String generateDeviceId() {
        // Gerar ID único baseado no dispositivo
        return android.provider.Settings.Secure.getString(
            context.getContentResolver(),
            android.provider.Settings.Secure.ANDROID_ID
        );
    }

    private void connect() {
        if (isConnecting.get() || isConnected.get()) {
            return;
        }

        isConnecting.set(true);

        try {
            URI serverUri = new URI(SERVER_URL);
            webSocketClient = new WebSocketClient(serverUri) {
                @Override
                public void onOpen(ServerHandshake handshakedata) {
                    Log.i(TAG, "WebSocket conectado");
                    isConnected.set(true);
                    isConnecting.set(false);

                    // Autenticar após conectar
                    authenticate();
                }

                @Override
                public void onMessage(String message) {
                    handleMessage(message);
                }

                @Override
                public void onClose(int code, String reason, boolean remote) {
                    Log.w(TAG, "WebSocket fechado: " + code + " - " + reason);
                    isConnected.set(false);
                    isConnecting.set(false);

                    if (callback != null) {
                        callback.onDisconnected();
                    }

                    // Tentar reconectar
                    scheduleReconnect();
                }

                @Override
                public void onError(Exception ex) {
                    Log.e(TAG, "Erro WebSocket", ex);
                    isConnecting.set(false);

                    if (callback != null) {
                        callback.onError(ex.getMessage());
                    }

                    scheduleReconnect();
                }
            };

            webSocketClient.connect();

        } catch (URISyntaxException e) {
            Log.e(TAG, "URI inválida", e);
            isConnecting.set(false);
            if (callback != null) {
                callback.onError("URI do servidor inválida");
            }
        }
    }

    private void authenticate() {
        try {
            JSONObject authMessage = new JSONObject();
            authMessage.put("type", "authenticate");
            authMessage.put("deviceId", deviceId);
            authMessage.put("timestamp", System.currentTimeMillis());

            // Assinar a mensagem (simplificado - em produção usar HMAC)
            String signature = generateSignature(authMessage.toString());
            authMessage.put("signature", signature);

            sendMessage(authMessage.toString());

            // Iniciar heartbeat
            startHeartbeat();

        } catch (JSONException e) {
            Log.e(TAG, "Erro ao criar mensagem de autenticação", e);
        }
    }

    private void startHeartbeat() {
        heartbeatExecutor.scheduleAtFixedRate(() -> {
            if (isConnected.get()) {
                try {
                    JSONObject heartbeat = new JSONObject();
                    heartbeat.put("type", "heartbeat");
                    heartbeat.put("deviceId", deviceId);
                    heartbeat.put("timestamp", System.currentTimeMillis());

                    sendMessage(heartbeat.toString());
                } catch (JSONException e) {
                    Log.e(TAG, "Erro ao enviar heartbeat", e);
                }
            }
        }, HEARTBEAT_INTERVAL_MS, HEARTBEAT_INTERVAL_MS, TimeUnit.MILLISECONDS);
    }

    private void handleMessage(String message) {
        try {
            JSONObject jsonMessage = new JSONObject(message);
            String type = jsonMessage.getString("type");

            switch (type) {
                case "authenticated":
                    Log.i(TAG, "Dispositivo autenticado com sucesso");
                    if (callback != null) {
                        callback.onConnected();
                    }
                    break;

                case "control":
                    handleControlMessage(jsonMessage);
                    break;

                case "screen_lock":
                    handleScreenLockCommand(jsonMessage);
                    break;

                case "app_monitoring":
                    handleAppMonitoringCommand(jsonMessage);
                    break;

                case "stream_request":
                    handleStreamRequest(jsonMessage);
                    break;

                case "error":
                    String errorMsg = jsonMessage.optString("message", "Erro desconhecido");
                    Log.e(TAG, "Erro do servidor: " + errorMsg);
                    if (callback != null) {
                        callback.onError(errorMsg);
                    }
                    break;

                default:
                    Log.w(TAG, "Tipo de mensagem desconhecido: " + type);
            }

        } catch (JSONException e) {
            Log.e(TAG, "Erro ao processar mensagem JSON", e);
        }
    }

    private void handleControlMessage(JSONObject message) {
        try {
            String action = message.getString("action");
            Log.i(TAG, "Comando de controle recebido: " + action);

            // Delegar para StreamingService
            StreamingService.getInstance().handleControlCommand(action, message);

        } catch (JSONException e) {
            Log.e(TAG, "Erro ao processar comando de controle", e);
        }
    }

    private void handleStreamRequest(JSONObject message) {
        try {
            String streamType = message.getString("streamType");
            Log.i(TAG, "Requisição de stream: " + streamType);

            // Iniciar streaming apropriado
            StreamingService.getInstance().startStreaming(streamType);

        } catch (JSONException e) {
            Log.e(TAG, "Erro ao processar requisição de stream", e);
        }
    }

    private void handleScreenLockCommand(JSONObject message) {
        try {
            String action = message.getString("action");
            Log.i(TAG, "Comando de bloqueio de tela: " + action);

            // Executar comando de bloqueio em thread principal
            new android.os.Handler(android.os.Looper.getMainLooper()).post(() -> {
                if (remoteCommandCallback != null) {
                    if ("lock".equals(action)) {
                        remoteCommandCallback.onScreenLockCommand();
                    } else if ("unlock".equals(action)) {
                        remoteCommandCallback.onScreenUnlockCommand();
                    }
                }

                // Fallback: controlar via serviço
                if ("lock".equals(action)) {
                    ScreenLockService.lockScreen(context);
                } else if ("unlock".equals(action)) {
                    ScreenLockService.unlockScreen(context);
                }
            });

        } catch (JSONException e) {
            Log.e(TAG, "Erro ao processar comando de bloqueio de tela", e);
        }
    }

    private void handleAppMonitoringCommand(JSONObject message) {
        try {
            String action = message.getString("action");
            Log.i(TAG, "Comando de monitoramento de apps: " + action);

            // Executar comando em thread principal
            new android.os.Handler(android.os.Looper.getMainLooper()).post(() -> {
                if ("start_monitoring".equals(action)) {
                    AppMonitorService.startMonitoring(context);
                    Log.i(TAG, "Monitoramento de apps iniciado remotamente");
                } else if ("stop_monitoring".equals(action)) {
                    AppMonitorService.stopMonitoring(context);
                    Log.i(TAG, "Monitoramento de apps parado remotamente");
                } else if ("get_stats".equals(action)) {
                    AppMonitorService.requestAppStats(context);
                    Log.i(TAG, "Estatísticas de apps solicitadas remotamente");
                }
            });

        } catch (JSONException e) {
            Log.e(TAG, "Erro ao processar comando de monitoramento de apps", e);
        }
    }

    private void scheduleReconnect() {
        if (!isConnected.get()) {
            Log.i(TAG, "Agendando reconexão em " + RECONNECT_DELAY_MS + "ms");
            new android.os.Handler(android.os.Looper.getMainLooper()).postDelayed(
                this::connect, RECONNECT_DELAY_MS);
        }
    }

    public void sendMessage(String message) {
        if (webSocketClient != null && isConnected.get()) {
            webSocketClient.send(message);
        } else {
            Log.w(TAG, "Tentativa de enviar mensagem sem conexão");
        }
    }

    public void disconnect() {
        if (heartbeatExecutor != null) {
            heartbeatExecutor.shutdown();
        }

        if (webSocketClient != null) {
            webSocketClient.close();
        }

        isConnected.set(false);
        isConnecting.set(false);
    }

    public boolean isConnected() {
        return isConnected.get();
    }

    public String getDeviceId() {
        return deviceId;
    }

    // Método simplificado para assinatura (em produção usar HMAC adequado)
    private String generateSignature(String data) {
        // TODO: Implementar assinatura HMAC adequada
        return "signature_placeholder";
    }
}