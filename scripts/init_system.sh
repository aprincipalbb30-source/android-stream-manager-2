#!/bin/bash
# init_system.sh - Script completo de inicialização

set -e

# Cores para output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== Inicialização do Android Stream Manager ===${NC}"

# 1. Verificar root
if [ "$EUID" -ne 0 ]; then 
    echo -e "${RED}Por favor, execute como root (sudo)${NC}"
    exit 1
fi

# Obter hostname do argumento
SERVER_HOSTNAME=$1
if [ -z "$SERVER_HOSTNAME" ]; then
    echo -e "${RED}ERRO: O nome do host do servidor não foi fornecido ao script init_system.sh.${NC}"
    exit 1
fi

# 2. Criar diretórios necessários
echo "Criando estrutura de diretórios..."
mkdir -p /opt/android-stream-manager
mkdir -p /etc/android-stream-manager/{certs,config}
mkdir -p /var/lib/android-stream-manager/{apks,database,sessions,cache}
mkdir -p /var/log/android-stream-manager
mkdir -p /var/backup/android-stream-manager
mkdir -p /usr/local/share/android-stream-manager/templates

# 3. Copiar arquivos do build
echo "Copiando arquivos do sistema..."
if [ -d "build/bin" ]; then
    cp -r build/bin/* /opt/android-stream-manager/
else # Esta verificação deve ser feita no install.sh, mas mantemos por segurança
    echo -e "${YELLOW}Aviso: Diretório build/bin não encontrado. Execute 'make build' primeiro.${NC}"
fi

# 4. Copiar configurações
cp config/* /etc/android-stream-manager/config/
cp templates/* /usr/local/share/android-stream-manager/templates/

# 5. Criar usuário e grupo
echo "Configurando usuário do sistema..."
if ! getent group stream-manager > /dev/null; then
    groupadd --system stream-manager
fi

if ! id -u stream-manager > /dev/null 2>&1; then
    useradd --system --no-create-home --gid stream-manager --shell /bin/false \
        --comment "Android Stream Manager Service" stream-manager
fi

# 6. Configurar permissões
echo "Configurando permissões..."
chown -R stream-manager:stream-manager \
    /opt/android-stream-manager \
    /var/lib/android-stream-manager \
    /var/log/android-stream-manager \
    /etc/android-stream-manager

chmod 750 /opt/android-stream-manager
chmod 750 /var/lib/android-stream-manager
chmod 640 /etc/android-stream-manager/certs/*.key 2>/dev/null || true
chmod 644 /etc/android-stream-manager/certs/*.crt 2>/dev/null || true

# 7. Configurar firewall (se disponível)
echo "Configurando firewall..."
if command -v ufw &> /dev/null; then
    ufw allow 8443/tcp comment "Android Stream Manager HTTPS"
    ufw allow 9090/tcp comment "Prometheus Metrics"
    echo -e "${GREEN}Firewall configurado${NC}"
fi

# 8. Instalar serviço systemd
echo "Instalando serviço systemd..."
cat > /etc/systemd/system/android-stream-manager.service << 'EOF'
[Unit] 
Description=Android Stream Manager Service
After=network.target
Requires=network.target
StartLimitIntervalSec=0

[Service]
Type=simple
Restart=always
RestartSec=5
User=stream-manager
Group=stream-manager
WorkingDirectory=/opt/android-stream-manager
EnvironmentFile=/etc/default/android-stream-manager

# Configurações de segurança
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=/var/lib/android-stream-manager /var/log/android-stream-manager

# Limites de recursos
LimitNOFILE=65536
LimitNPROC=4096
LimitCORE=0

# Comando de execução
ExecStart=/opt/android-stream-manager/stream_server --port ${SERVER_PORT} --cert ${CERT_PATH} --key ${KEY_PATH}

# Logs
StandardOutput=journal
StandardError=journal
SyslogIdentifier=android-stream-manager

[Install]
WantedBy=multi-user.target
EOF

# 9. Criar arquivo de ambiente
echo "Criando arquivo de ambiente..."
cat > /etc/default/android-stream-manager << 'EOF'
# Configurações de ambiente para o Android Stream Manager

# Configurações de Rede e SSL
SERVER_HOSTNAME=your-server-hostname.com
SERVER_PORT=8443
CERT_PATH=/etc/android-stream-manager/certs/server.crt
KEY_PATH=/etc/android-stream-manager/certs/server.key

# Configurações de Segurança (ALTERE ESTES VALORES!)
JWT_SECRET="your-super-secret-jwt-key-that-is-long-and-random"
ANDROID_HOME=$ANDROID_SDK_ROOT

# Logging
LOG_LEVEL=info
LOG_DIR=/var/log/android-stream-manager

# Database
DB_PATH=/var/lib/android-stream-manager/database.db
EOF
# Substituir o hostname no arquivo de ambiente
sed -i "s/SERVER_HOSTNAME=.*/SERVER_HOSTNAME=${SERVER_HOSTNAME}/" /etc/default/android-stream-manager

echo -e "${YELLOW}AVISO: O arquivo /etc/default/android-stream-manager foi criado com valores padrão.${NC}"
echo -e "${YELLOW}É CRUCIAL que você edite este arquivo e defina um JWT_SECRET seguro!${NC}"

# 10. Recarregar systemd
systemctl daemon-reload

# 11. Criar script de backup
echo "Criando script de backup..."
cat > /usr/local/bin/backup-stream-manager << 'EOF'
#!/bin/bash
BACKUP_DIR="/var/backup/android-stream-manager"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
BACKUP_FILE="$BACKUP_DIR/backup_$TIMESTAMP.tar.gz"

echo "Criando backup em: $BACKUP_FILE"
tar -czf "$BACKUP_FILE" \
    /etc/android-stream-manager \
    /var/lib/android-stream-manager/database.db \
    /var/lib/android-stream-manager/apks \
    2>/dev/null

# Limpar backups antigos (mais de 30 dias)
find "$BACKUP_DIR" -name "backup_*.tar.gz" -mtime +30 -delete

echo "Backup completo: $BACKUP_FILE"
EOF

chmod +x /usr/local/bin/backup-stream-manager

# 12. Criar crontab para backups
echo "Configurando backup automático..."
(crontab -l 2>/dev/null; echo "0 2 * * * /usr/local/bin/backup-stream-manager") | crontab -

# 13. Instalar página de status web (opcional)
echo "Instalando página de status web..."
if [ -f "scripts/status.html" ]; then
    cp scripts/status.html /var/www/html/stream-manager-status.html
    chown www-data:www-data /var/www/html/stream-manager-status.html
fi

echo -e "${GREEN}=== Inicialização completa! ===${NC}"
echo ""
echo -e "${YELLOW}Próximos passos:${NC}"
echo "1. Edite /etc/default/android-stream-manager para definir suas chaves secretas."
echo "2. Coloque seus certificados SSL em /etc/android-stream-manager/certs/"
echo "3. Inicie o serviço com: sudo systemctl start android-stream-manager"
echo "4. Verifique os logs com: sudo journalctl -u android-stream-manager -f"
echo ""
echo -e "${GREEN}O serviço foi configurado, mas não iniciado ou habilitado automaticamente.${NC}"
echo -e "${GREEN}Inicie-o manualmente após concluir a configuração.${NC}"