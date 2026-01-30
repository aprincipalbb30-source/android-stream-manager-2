#!/bin/bash
#
# install.sh - Script de Instalação de Produção para o Android Stream Manager
#
# Este script automatiza a instalação de dependências, compilação do projeto
# e configuração do sistema como um serviço em distribuições baseadas em Debian/Ubuntu.
# Ele é projetado para ser executado em um servidor de produção.
#
set -e

# --- Cores para o output ---
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}===================================================${NC}"
echo -e "${BLUE}🚀 Instalador e Configurador de Produção do Android Stream Manager 🚀${NC}"
echo -e "${BLUE}===================================================${NC}"

# --- 1. Verificação de Root ---
if [ "$EUID" -ne 0 ]; then
    echo -e "${RED}ERRO: Este script precisa ser executado como root (ou com sudo).${NC}"
    echo -e "${YELLOW}Tente: sudo ./install.sh${NC}"
    exit 1
fi

# --- 2. Verificações Iniciais ---
if [ ! -f "CMakeLists.txt" ] || [ ! -d "server" ] || [ ! -d "core" ]; then
    echo -e "${RED}ERRO: O script deve ser executado a partir do diretório raiz do projeto.${NC}"
    exit 1
fi

echo -e "\n${GREEN}✅ Verificações iniciais concluídas.${NC}"

# --- 3. Instalação de Dependências (Debian/Ubuntu) ---
echo -e "\n${YELLOW}🔧 Etapa 1/4: Instalando dependências do sistema...${NC}"

DEPS=(
    build-essential
    cmake
    git
    pkg-config
    libssl-dev
    zlib1g-dev
    libsqlite3-dev
    qt6-base-dev
    qt6-websockets-dev
    qt6-multimedia-dev
    libavcodec-dev
    libavutil-dev
    libswscale-dev
    libxkbcommon-dev
)

apt-get update
apt-get install -y "${DEPS[@]}"

echo -e "\n${GREEN}✅ Dependências do sistema instaladas com sucesso.${NC}"
echo -e "${YELLOW}Nota: jwt-cpp, nlohmann_json, lz4, e libzip serão baixados e compilados pelo CMake (FetchContent).${NC}"

# --- 4. Compilação do Projeto ---
echo -e "\n${YELLOW}🏗️ Etapa 2/4: Compilando o projeto para produção...${NC}"

INSTALL_DIR="/opt/android-stream-manager"

if [ -d "build" ]; then
    echo "Diretório 'build' existente. Removendo para uma compilação limpa..."
    rm -rf build
fi

mkdir build
cd build

echo "Configurando com CMake..."
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF -DCMAKE_INSTALL_PREFIX=${INSTALL_DIR}

echo "Compilando com make (utilizando todos os cores disponíveis)..."
if ! make -j$(nproc); then
    echo -e "${RED}ERRO: A compilação falhou. Verifique os erros acima.${NC}"
    exit 1
fi

echo -e "\n${GREEN}✅ Projeto compilado com sucesso!${NC}"

# --- 5. Instalação do Sistema ---
echo -e "\n${YELLOW}⚙️ Etapa 3/4: Instalando o sistema em ${INSTALL_DIR}...${NC}"

make install

echo -e "\n${GREEN}✅ Sistema instalado com sucesso.${NC}"

# --- 6. Configuração do Serviço de Produção ---
echo -e "\n${YELLOW}🚀 Etapa 4/4: Configurando o ambiente de produção (usuário e serviço systemd)...${NC}"

# Criar usuário e grupo do serviço
SERVICE_USER="asm-user"
if ! getent group ${SERVICE_USER} > /dev/null; then
    echo "Criando grupo de sistema '${SERVICE_USER}'..."
    groupadd --system ${SERVICE_USER}
fi
if ! id -u ${SERVICE_USER} > /dev/null 2>&1; then
    echo "Criando usuário de sistema '${SERVICE_USER}'..."
    useradd --system --no-create-home --gid ${SERVICE_USER} --shell /bin/false \
        --comment "Android Stream Manager Service" ${SERVICE_USER}
fi

# Criar diretórios de dados, logs e configuração
echo "Criando diretórios de dados e logs..."
mkdir -p /var/lib/android-stream-manager
mkdir -p /var/log/android-stream-manager

# Definir permissões
echo "Configurando permissões dos diretórios..."
chown -R ${SERVICE_USER}:${SERVICE_USER} ${INSTALL_DIR}
chown -R ${SERVICE_USER}:${SERVICE_USER} /var/lib/android-stream-manager
chown -R ${SERVICE_USER}:${SERVICE_USER} /var/log/android-stream-manager
# O diretório /etc/android-stream-manager é criado pelo 'make install'
chown -R root:${SERVICE_USER} /etc/android-stream-manager
chmod 775 /etc/android-stream-manager

# Instalar o serviço systemd
if [ -f "scripts/android-stream-manager.service" ]; then
    echo "Instalando arquivo de serviço systemd..."
    cp scripts/android-stream-manager.service /etc/systemd/system/android-stream-manager.service
    
    # Criar arquivo de ambiente padrão
    echo "Criando arquivo de ambiente padrão em /etc/default/android-stream-manager..."
    cat > /etc/default/android-stream-manager << 'EOF'
# Arquivo de ambiente para o serviço Android Stream Manager
# EDITE ESTE ARQUIVO COM SUAS CONFIGURAÇÕES DE PRODUÇÃO

# Segredo para assinar os tokens JWT. DEVE ser alterado para um valor longo e aleatório.
JWT_SECRET="segredo-padrao-inseguro-altere-imediatamente"

# Caminho para o banco de dados SQLite
DB_PATH="/var/lib/android-stream-manager/database.sqlite"
EOF
    chown root:${SERVICE_USER} /etc/default/android-stream-manager
    chmod 640 /etc/default/android-stream-manager

    systemctl daemon-reload
    echo -e "${GREEN}✅ Serviço systemd configurado. Use 'systemctl start android-stream-manager' para iniciá-lo.${NC}"
fi

# --- Conclusão ---
echo -e "\n${BLUE}===================================================${NC}"
echo -e "${GREEN}🎉 Instalação de Produção Finalizada! 🎉${NC}"
echo -e "${BLUE}===================================================${NC}"
echo -e "\n${YELLOW}⚠️ PRÓXIMOS PASSOS:${NC}"
echo -e "1. ${YELLOW}Os binários e bibliotecas foram instalados em '${INSTALL_DIR}'.${NC}"
echo -e "   - Executáveis: ${INSTALL_DIR}/bin/"
echo -e "   - Bibliotecas: ${INSTALL_DIR}/lib/"
echo -e "   - Arquivo de Configuração: ${INSTALL_DIR}/etc/android-stream-manager/system_config.json"
echo ""
echo -e "2. ${YELLOW}Edite o arquivo de ambiente com seu segredo JWT e outras configurações:${NC}"
echo -e "   sudo nano /etc/default/android-stream-manager"
echo ""
echo -e "3. ${YELLOW}Para iniciar o serviço, execute:${NC} sudo systemctl start android-stream-manager"
echo -e "   ${YELLOW}Para habilitá-lo na inicialização:${NC} sudo systemctl enable android-stream-manager"
echo ""

exit 0