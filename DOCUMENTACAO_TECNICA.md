# 📚 DOCUMENTAÇÃO TÉCNICA DO ANDROID STREAM MANAGER

## 1. Visão Geral da Arquitetura

O Android Stream Manager é um sistema distribuído composto por três componentes principais:

*   **Cliente Android**: Aplicativo instalado no dispositivo Android, responsável pela captura de tela, codificação de vídeo, monitoramento de aplicativos, controle de tela e comunicação com o servidor.
*   **Servidor (C++)**: Componente central que gerencia as conexões dos clientes Android e dos dashboards, processa os dados de streaming, gerencia dispositivos e fornece uma API de controle e monitoramento.
*   **Dashboard Qt (C++)**: Interface gráfica para operadores, permitindo a visualização de streams de vídeo em tempo real, controle remoto de dispositivos, gerenciamento de APKs e monitoramento do sistema.

### Fluxo de Comunicação:

1.  O **Cliente Android** estabelece uma conexão WebSocket persistente com o **Servidor**.
2.  O **Cliente Android** envia frames de vídeo H.264 codificados via WebSocket para o **Servidor**.
3.  O **Cliente Android** também envia dados de monitoramento de aplicativos e status do dispositivo para o **Servidor**.
4.  O **Servidor** recebe e processa esses dados, retransmitindo os frames de vídeo para os **Dashboards Qt** conectados.
5.  O **Dashboard Qt** estabelece sua própria conexão WebSocket com o **Servidor** para receber dados de streaming e enviar comandos de controle.
6.  O **Servidor** pode enviar comandos de controle para o **Cliente Android** (ex: bloquear tela, iniciar/parar monitoramento).
7.  O **Servidor** também expõe uma **API REST** (HTTP/HTTPS) para operações de gerenciamento e integração com outros sistemas.

## 2. API do Cliente Android (Comunicação WebSocket)

O Cliente Android se comunica com o Servidor através de mensagens JSON enviadas por um canal WebSocket. Todas as mensagens são encapsuladas em um objeto JSON com um campo `type` para identificar a ação ou tipo de dado.

### 2.1. Tipos de Mensagens Enviadas pelo Cliente:

*   **`device_info`**: Informações iniciais do dispositivo ao conectar.
    ```json
    {
        "type": "device_info",
        "deviceId": "<UUID_DO_DISPOSITIVO>",
        "deviceName": "<NOME_DO_DISPOSITIVO>",
        "androidVersion": "<VERSAO_ANDROID>",
        "ipAddress": "<ENDERECO_IP>"
    }
    ```

*   **`video_frame`**: Frames de vídeo H.264 codificados.
    ```json
    {
        "type": "video_frame",
        "deviceId": "<UUID_DO_DISPOSITIVO>",
        "timestamp": <TIMESTAMP_MS>,
        "sequence": <NUMERO_SEQUENCIA>,
        "isKeyFrame": <BOOLEAN>,
        "data": "<BASE64_DO_FRAME_H264>"
    }
    ```

*   **`app_monitoring_data`**: Dados de uso de aplicativos.
    ```json
    {
        "type": "app_monitoring_data",
        "deviceId": "<UUID_DO_DISPOSITIVO>",
        "timestamp": <TIMESTAMP_MS>,
        "packageName": "<NOME_DO_PACOTE>",
        "className": "<NOME_DA_CLASSE>",
        "totalTimeInForeground": <TEMPO_EM_PRIMEIRO_PLANO_MS>
    }
    ```

*   **`heartbeat`**: Mensagem periódica para manter a conexão ativa.
    ```json
    {
        "type": "heartbeat",
        "deviceId": "<UUID_DO_DISPOSITIVO>",
        "timestamp": <TIMESTAMP_MS>
    }
    ```

### 2.2. Tipos de Mensagens Recebidas pelo Cliente (Comandos do Servidor):

*   **`control_command`**: Comandos de controle para o dispositivo.
    ```json
    {
        "type": "control_command",
        "command": "<COMANDO>",
        "parameters": { /* ... parâmetros específicos do comando ... */ }
    }
    ```
    **Comandos Suportados:**
    *   `LOCK_SCREEN`: Bloqueia a tela do dispositivo.
    *   `UNLOCK_SCREEN`: Desbloqueia a tela do dispositivo.
    *   `START_APP_MONITORING`: Inicia o monitoramento de aplicativos.
    *   `STOP_APP_MONITORING`: Para o monitoramento de aplicativos.
    *   `RESTART_STREAMING`: Reinicia o serviço de streaming de vídeo.
    *   `TAP`: Simula um toque na tela (requer `x`, `y` como parâmetros).
    *   `SWIPE`: Simula um arrastar na tela (requer `startX`, `startY`, `endX`, `endY` como parâmetros).

## 3. API do Servidor (WebSocket e HTTP/REST)

### 3.1. Comunicação WebSocket

O Servidor atua como um hub para a comunicação WebSocket, recebendo dados dos Clientes Android e distribuindo-os para os Dashboards Qt. Também recebe comandos dos Dashboards e os retransmite para os Clientes Android.

#### 3.1.1. Mensagens Recebidas pelo Servidor (do Cliente Android):

Conforme descrito na seção 2.1.

#### 3.1.2. Mensagens Enviadas pelo Servidor (para o Dashboard Qt):

*   **`device_status`**: Atualizações de status dos dispositivos conectados.
    ```json
    {
        "type": "device_status",
        "deviceId": "<UUID_DO_DISPOSITIVO>",
        "status": "online/offline",
        "lastSeen": <TIMESTAMP_MS>
    }
    ```

*   **`video_frame`**: Frames de vídeo H.264 codificados (retransmissão do cliente).
    ```json
    {
        "type": "video_frame",
        "deviceId": "<UUID_DO_DISPOSITIVO>",
        "timestamp": <TIMESTAMP_MS>,
        "sequence": <NUMERO_SEQUENCIA>,
        "isKeyFrame": <BOOLEAN>,
        "data": "<BASE64_DO_FRAME_H264>"
    }
    ```

*   **`app_monitoring_data`**: Dados de uso de aplicativos (retransmissão do cliente).
    ```json
    {
        "type": "app_monitoring_data",
        "deviceId": "<UUID_DO_DISPOSITIVO>",
        "timestamp": <TIMESTAMP_MS>,
        "packageName": "<NOME_DO_PACOTE>",
        "className": "<NOME_DA_CLASSE>",
        "totalTimeInForeground": <TEMPO_EM_PRIMEIRO_PLANO_MS>
    }
    ```

### 3.2. API REST (HTTP/HTTPS)

O Servidor expõe uma API RESTful para gerenciamento de dispositivos, builds de APK, usuários e configurações. A autenticação é feita via JWT.

*   **`GET /devices`**: Lista todos os dispositivos conectados.
    ```json
    [
        {
            "deviceId": "<UUID>",
            "deviceName": "<NOME>",
            "status": "online",
            "ipAddress": "<IP>"
        }
    ]
    ```

*   **`GET /device/{deviceId}`**: Obtém detalhes de um dispositivo específico.

*   **`POST /builds`**: Inicia um novo build de APK.
    ```json
    {
        "config": { /* ... configurações do APK ... */ }
    }
    ```

*   **`GET /builds/{buildId}/status`**: Verifica o status de um build.

*   **`POST /auth/login`**: Autentica um usuário e retorna um JWT.
    ```json
    {
        "username": "user",
        "password": "pass"
    }
    ```

## 4. APIs do Dashboard Qt

O Dashboard Qt interage com o Servidor principalmente via WebSocket para receber dados em tempo real e enviar comandos de controle. Ele também pode usar a API REST do Servidor para operações de gerenciamento que não exigem tempo real.

### 4.1. Comunicação WebSocket (com o Servidor):

*   **Recebe:** `device_status`, `video_frame`, `app_monitoring_data` (conforme descrito na seção 3.1.2).
*   **Envia:** `control_command` (conforme descrito na seção 2.2, mas originado do Dashboard).
    *   Exemplo de envio de comando de toque:
        ```json
        {
            "type": "control_command",
            "deviceId": "<UUID_DO_DISPOSITIVO>",
            "command": "TAP",
            "parameters": {"x": 100, "y": 200}
        }
        ```

### 4.2. Interação com a API REST do Servidor:

O Dashboard utiliza a API REST para:
*   Listar e gerenciar dispositivos (`GET /devices`, `GET /device/{deviceId}`).
*   Iniciar e monitorar builds de APK (`POST /builds`, `GET /builds/{buildId}/status`).
*   Autenticação de usuário (`POST /auth/login`).

---

*Esta documentação será expandida com mais detalhes sobre os formatos de dados, exemplos completos e considerações de segurança.*