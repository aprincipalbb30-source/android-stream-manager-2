package com.streammanager.client;

import android.app.Application;
import android.content.Context;
import android.util.Log;
import java.io.File;

public class StreamingApplication extends Application {
    private static final String TAG = "StreamingApplication";

    private static StreamingApplication instance;
    private File cacheDir;
    private File logDir;

    @Override
    public void onCreate() {
        super.onCreate();
        instance = this;

        Log.i(TAG, "StreamingApplication inicializada");

        // Inicializar diretórios
        initializeDirectories();

        // Configurar logging global
        setupLogging();

        // Configurar crash handler
        setupCrashHandler();

        Log.i(TAG, "StreamingApplication pronta");
    }

    private void initializeDirectories() {
        try {
            // Diretório de cache do app
            cacheDir = new File(getCacheDir(), "stream_cache");
            if (!cacheDir.exists()) {
                cacheDir.mkdirs();
            }

            // Diretório de logs
            logDir = new File(getFilesDir(), "logs");
            if (!logDir.exists()) {
                logDir.mkdirs();
            }

            Log.i(TAG, "Diretórios inicializados: " + cacheDir.getAbsolutePath());

        } catch (Exception e) {
            Log.e(TAG, "Erro ao inicializar diretórios", e);
        }
    }

    private void setupLogging() {
        // Configurar logging para arquivo (opcional)
        // Em produção, pode integrar com bibliotecas de logging como Timber
        Log.i(TAG, "Logging configurado");
    }

    private void setupCrashHandler() {
        // Configurar handler de crashes não tratados
        Thread.setDefaultUncaughtExceptionHandler((thread, throwable) -> {
            Log.e(TAG, "Crash não tratado na thread " + thread.getName(), throwable);

            // Salvar informações do crash para análise
            saveCrashInfo(thread, throwable);

            // Finalizar aplicação
            System.exit(1);
        });
    }

    private void saveCrashInfo(Thread thread, Throwable throwable) {
        try {
            // Salvar informações do crash em arquivo de log
            String crashInfo = String.format(
                "CRASH: %s\nThread: %s\nTime: %d\nStackTrace:\n%s",
                throwable.getMessage(),
                thread.getName(),
                System.currentTimeMillis(),
                Log.getStackTraceString(throwable)
            );

            // Em produção, salvar em arquivo ou enviar para servidor
            Log.e(TAG, crashInfo);

        } catch (Exception e) {
            Log.e(TAG, "Erro ao salvar informações do crash", e);
        }
    }

    public static StreamingApplication getInstance() {
        return instance;
    }

    public File getCacheDirectory() {
        return cacheDir;
    }

    public File getLogDirectory() {
        return logDir;
    }

    @Override
    public void onLowMemory() {
        super.onLowMemory();
        Log.w(TAG, "Memória baixa detectada");

        // Limpar cache se necessário
        cleanupCache();
    }

    @Override
    public void onTrimMemory(int level) {
        super.onTrimMemory(level);
        Log.i(TAG, "Trim memory level: " + level);

        if (level >= TRIM_MEMORY_BACKGROUND) {
            cleanupCache();
        }
    }

    private void cleanupCache() {
        try {
            if (cacheDir != null && cacheDir.exists()) {
                File[] files = cacheDir.listFiles();
                if (files != null) {
                    for (File file : files) {
                        if (file.isFile() && file.lastModified() < System.currentTimeMillis() - (24 * 60 * 60 * 1000)) {
                            // Apagar arquivos com mais de 24 horas
                            file.delete();
                        }
                    }
                }
            }
            Log.i(TAG, "Cache limpo");
        } catch (Exception e) {
            Log.e(TAG, "Erro ao limpar cache", e);
        }
    }

    // Método utilitário para obter contexto de aplicação
    public static Context getAppContext() {
        return instance.getApplicationContext();
    }
}