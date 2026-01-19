package com.streammanager.client;

import android.Manifest;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.widget.TextView;
import android.widget.Toast;
import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

public class MainActivity extends AppCompatActivity {
    private static final int PERMISSION_REQUEST_CODE = 1001;

    private TextView statusText;
    private TextView connectionStatus;
    private Button lockScreenButton;

    // Permissões necessárias para streaming
    private static final String[] REQUIRED_PERMISSIONS = {
        Manifest.permission.INTERNET,
        Manifest.permission.ACCESS_NETWORK_STATE,
        Manifest.permission.RECORD_AUDIO,
        Manifest.permission.CAMERA,
        Manifest.permission.WRITE_EXTERNAL_STORAGE,
        Manifest.permission.READ_EXTERNAL_STORAGE,
        Manifest.permission.FOREGROUND_SERVICE,
        Manifest.permission.WAKE_LOCK
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Configurar interface básica
        setContentView(R.layout.activity_main);

        statusText = findViewById(R.id.status_text);
        connectionStatus = findViewById(R.id.connection_status);
        lockScreenButton = findViewById(R.id.lock_screen_button);

        // Verificar e solicitar permissões
        if (checkPermissions()) {
            initializeApp();
        } else {
            requestPermissions();
        }
    }

    private boolean checkPermissions() {
        for (String permission : REQUIRED_PERMISSIONS) {
            if (ContextCompat.checkSelfPermission(this, permission)
                != PackageManager.PERMISSION_GRANTED) {
                return false;
            }
        }
        return true;
    }

    private void requestPermissions() {
        ActivityCompat.requestPermissions(this, REQUIRED_PERMISSIONS, PERMISSION_REQUEST_CODE);
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions,
                                         @NonNull int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);

        if (requestCode == PERMISSION_REQUEST_CODE) {
            boolean allGranted = true;
            for (int result : grantResults) {
                if (result != PackageManager.PERMISSION_GRANTED) {
                    allGranted = false;
                    break;
                }
            }

            if (allGranted) {
                initializeApp();
            } else {
                Toast.makeText(this, "Permissões necessárias não concedidas",
                             Toast.LENGTH_LONG).show();
                finish();
            }
        }
    }

    private void initializeApp() {
        updateStatus("Inicializando...");

        // Iniciar serviço de streaming em background
        Intent serviceIntent = new Intent(this, StreamingService.class);
        ContextCompat.startForegroundService(this, serviceIntent);

        // Iniciar serviço de bloqueio de tela
        Intent lockServiceIntent = new Intent(this, ScreenLockService.class);
        ContextCompat.startForegroundService(this, lockServiceIntent);

        // Iniciar serviço de monitoramento de apps
        Intent monitorServiceIntent = new Intent(this, AppMonitorService.class);
        ContextCompat.startForegroundService(this, monitorServiceIntent);

        // Configurar botão de bloqueio de tela
        if (lockScreenButton != null) {
            lockScreenButton.setOnClickListener(v -> {
                if (ScreenLockService.isScreenLocked()) {
                    // Desbloquear
                    ScreenLockService.unlockScreen(MainActivity.this);
                    lockScreenButton.setText("🔒 Bloquear Tela (Teste)");
                    updateStatus("Tela desbloqueada");
                } else {
                    // Bloquear
                    ScreenLockActivity.startScreenLock(MainActivity.this);
                    lockScreenButton.setText("🔓 Desbloquear Tela (Teste)");
                    updateStatus("Tela bloqueada");
                }
            });
        }

        updateStatus("Conectando ao servidor...");

        // Configurar NetworkManager para conectar
        NetworkManager.getInstance().initialize(this, new NetworkManager.ConnectionCallback() {
            @Override
            public void onConnected() {
                runOnUiThread(() -> {
                    updateConnectionStatus("Conectado ✓");
                    updateStatus("Pronto para streaming");
                });
            }

            @Override
            public void onDisconnected() {
                runOnUiThread(() -> {
                    updateConnectionStatus("Desconectado ✗");
                    updateStatus("Tentando reconectar...");
                });
            }

            @Override
            public void onError(String error) {
                runOnUiThread(() -> {
                    updateConnectionStatus("Erro de conexão");
                    updateStatus("Erro: " + error);
                });
            }
        });

        // Configurar callback de comandos remotos
        NetworkManager.getInstance().setRemoteCommandCallback(new NetworkManager.RemoteCommandCallback() {
            @Override
            public void onScreenLockCommand() {
                runOnUiThread(() -> {
                    updateStatus("Bloqueio remoto ativado");
                    ScreenLockActivity.startScreenLock(MainActivity.this);
                });
            }

            @Override
            public void onScreenUnlockCommand() {
                runOnUiThread(() -> {
                    updateStatus("Bloqueio remoto desativado");
                    ScreenLockService.unlockScreen(MainActivity.this);
                });
            }

            @Override
            public void onRemoteControlCommand(String action, String data) {
                runOnUiThread(() -> {
                    updateStatus("Comando remoto: " + action);
                });
            }
        });
    }

    private void updateStatus(String status) {
        if (statusText != null) {
            statusText.setText("Status: " + status);
        }
    }

    private void updateConnectionStatus(String status) {
        if (connectionStatus != null) {
            connectionStatus.setText("Conexão: " + status);
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        // Cleanup se necessário
        NetworkManager.getInstance().disconnect();
    }
}