#!/bin/bash
# test_system.sh

echo "=== Testando Android Stream Manager ==="

# 1. Testar se o serviço está rodando
if ! systemctl is-active --quiet android-stream-manager; then
    echo "❌ Serviço não está rodando"
    exit 1
fi
echo "✅ Serviço está ativo"

# 2. Testar porta HTTPS
if ! nc -z localhost 8443; then
    echo "❌ Porta 8443 não está ouvindo"
    exit 1
fi
echo "✅ Porta 8443 está aberta"

# 3. Testar API de saúde
HEALTH_RESPONSE=$(curl -s -k https://localhost:8443/api/health)
if echo "$HEALTH_RESPONSE" | grep -q '"status":"ok"'; then
    echo "✅ API de saúde respondendo"
else
    echo "❌ API de saúde falhou: $HEALTH_RESPONSE"
fi

# 4. Testar banco de dados
if [ -f "/var/lib/android-stream-manager/database.db" ]; then
    DB_SIZE=$(stat -c%s "/var/lib/android-stream-manager/database.db")
    if [ "$DB_SIZE" -gt 0 ]; then
        echo "✅ Banco de dados existe e tem $DB_SIZE bytes"
    else
        echo "⚠️ Banco de dados vazio"
    fi
fi

# 5. Testar diretórios
DIRS=(
    "/opt/android-stream-manager"
    "/var/lib/android-stream-manager"
    "/var/log/android-stream-manager"
    "/etc/android-stream-manager"
)

for dir in "${DIRS[@]}"; do
    if [ -d "$dir" ]; then
        echo "✅ Diretório $dir existe"
    else
        echo "❌ Diretório $dir não existe"
    fi
done

# 6. Testar permissões
USER_CHECK=$(stat -c '%U:%G' /opt/android-stream-manager 2>/dev/null)
if [ "$USER_CHECK" = "stream-manager:stream-manager" ]; then
    echo "✅ Permissões do usuário corretas"
else
    echo "❌ Permissões incorretas: $USER_CHECK"
fi

# 7. Testar certificados
CERT_FILES=(
    "/etc/android-stream-manager/certs/server.crt"
    "/etc/android-stream-manager/certs/server.key"
    "/etc/android-stream-manager/certs/ca.crt"
)

for cert in "${CERT_FILES[@]}"; do
    if [ -f "$cert" ]; then
        echo "✅ Certificado $cert existe"
    else
        echo "⚠️ Certificado $cert não encontrado"
    fi
done

echo "=== Teste completo ==="