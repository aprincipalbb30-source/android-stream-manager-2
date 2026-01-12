8. Makefile para Build
makefile
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -pthread
LDFLAGS = -pthread -lssl -lcrypto -lz -llz4 -ljwt -lzip

# Diretórios
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

# Fontes principais
SOURCES = $(wildcard $(SRC_DIR)/**/*.cpp $(SRC_DIR)/*.cpp)
OBJECTS = $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SOURCES))
TARGET = $(BIN_DIR)/android-stream-manager

# Dependências OpenSSL
OPENSSL_INC = /usr/include/openssl
OPENSSL_LIB = /usr/lib/x86_64-linux-gnu

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(OBJECTS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(OBJECTS) -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -I$(OPENSSL_INC) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

install: $(TARGET)
	cp $(TARGET) /usr/local/bin/
	cp config/system_config.json /etc/android-stream-manager/
	mkdir -p /var/log/android-stream-manager
	mkdir -p /var/cache/android-stream-manager

# Build específico para segurança
security: CXXFLAGS += -DUSE_OPENSSL_1_1 -DSTRICT_SECURITY
security: clean all

# Build para produção
release: CXXFLAGS += -O3 -DNDEBUG -flto
release: LDFLAGS += -s
release: clean all
9. Script de Inicialização do Sistema
bash
#!/bin/bash
# init_system.sh

CONFIG_PATH="/etc/android-stream-manager"
LOG_PATH="/var/log/android-stream-manager"
CACHE_PATH="/var/cache/android-stream-manager"
BIN_PATH="/usr/local/bin/android-stream-manager"

# Verificar dependências
check_dependencies() {
    echo "Verificando dependências..."
    
    # OpenSSL
    if ! command -v openssl &> /dev/null; then
        echo "OpenSSL não encontrado. Instalando..."
        apt-get update && apt-get install -y openssl libssl-dev
    fi
    
    # LZ4
    if ! ldconfig -p | grep liblz4 &> /dev/null; then
        echo "LZ4 não encontrado. Instalando..."
        apt-get install -y liblz4-dev
    fi
    
    # libzip
    if ! ldconfig -p | grep libzip &> /dev/null; then
        echo "libzip não encontrado. Instalando..."
        apt-get install -y libzip-dev
    fi
}

# Gerar certificados
generate_certificates() {
    echo "Gerando certificados TLS..."
    
    mkdir -p "$CONFIG_PATH/certs"
    cd "$CONFIG_PATH/certs"
    
    # CA Root
    openssl genrsa -out ca.key 4096
    openssl req -new -x509 -days 3650 -key ca.key -out ca.crt \
        -subj "/C=BR/ST=SP/O=Corporation/CN=Android Stream Manager CA"
    
    # Server Certificate
    openssl genrsa -out server.key 2048
    openssl req -new -key server.key -out server.csr \
        -subj "/C=BR/ST=SP/O=Corporation/CN=stream-server.local"
    openssl x509 -req -days 365 -in server.csr -CA ca.crt -CAkey ca.key \
        -CAcreateserial -out server.crt
    
    # Client Certificate
    openssl genrsa -out client.key 2048
    openssl req -new -key client.key -out client.csr \
        -subj "/C=BR/ST=SP/O=Corporation/CN=operator-client"
    openssl x509 -req -days 365 -in client.csr -CA ca.crt -CAkey ca.key \
        -CAcreateserial -out client.crt
    
    chmod 600 *.key
    chmod 644 *.crt
}

# Configurar serviço systemd
setup_systemd_service() {
    echo "Configurando serviço systemd..."
    
    cat > /etc/systemd/system/android-stream-manager.service << EOF
[Unit]
Description=Android Stream Manager Service
After=network.target
Requires=network.target

[Service]
Type=simple
User=stream-manager
Group=stream-manager
WorkingDirectory=/var/lib/android-stream-manager
Environment="CONFIG_PATH=$CONFIG_PATH"
Environment="LOG_PATH=$LOG_PATH"
Environment="CACHE_PATH=$CACHE_PATH"
ExecStart=$BIN_PATH --config $CONFIG_PATH/system_config.json
Restart=on-failure
RestartSec=5
LimitNOFILE=65536
LimitNPROC=4096

# Segurança
PrivateTmp=true
NoNewPrivileges=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=$LOG_PATH $CACHE_PATH

[Install]
WantedBy=multi-user.target
EOF
    
    # Criar usuário dedicado
    useradd -r -s /bin/false -d /var/lib/android-stream-manager stream-manager
    
    # Configurar permissões
    mkdir -p /var/lib/android-stream-manager
    chown -R stream-manager:stream-manager \
        /var/lib/android-stream-manager \
        "$LOG_PATH" \
        "$CACHE_PATH" \
        "$CONFIG_PATH"
    
    chmod 750 "$CONFIG_PATH/certs"
    
    systemctl daemon-reload
    systemctl enable android-stream-manager
}

# Inicializar cache
initialize_cache() {
    echo "Inicializando cache..."
    
    mkdir -p "$CACHE_PATH/builds"
    mkdir -p "$CACHE_PATH/templates"
    mkdir -p "$CACHE_PATH/sessions"
    
    # Template APK padrão
    if [ ! -f "$CACHE_PATH/templates/default.zip" ]; then
        echo "Baixando template padrão..."
        wget -q -O "$CACHE_PATH/templates/default.zip" \
            https://example.com/templates/android-client-base.zip
    fi
}

main() {
    echo "=== Inicialização do Android Stream Manager ==="
    
    check_dependencies
    generate_certificates
    initialize_cache
    setup_systemd_service
    
    echo "Inicialização completa!"
    echo "Para iniciar o serviço: sudo systemctl start android-stream-manager"
    echo "Para ver logs: sudo journalctl -u android-stream-manager -f"
}

main "$@"
Este sistema implementa:

Segurança robusta: TLS 1.3, JWT com rotação, assinatura de APKs

Conformidade ética: Consentimento explícito, logging de auditoria

Otimização avançada: Thread pool, cache LZ4, streaming adaptativo

Arquitetura modular: Componentes desacoplados e reutilizáveis

Pronto para produção: Systemd, configuração segura, monitoramento

Para usar o sistema:

bash
# 1. Instalar dependências
sudo apt-get install openssl libssl-dev liblz4-dev libzip-dev

# 2. Build
make release

# 3. Instalar
sudo make install

# 4. Inicializar
sudo ./init_system.sh

# 5. Iniciar
sudo systemctl start android-stream-manager
O sistema está pronto para ambientes corporativos com todos os requisitos de segurança e conformidade implementados.