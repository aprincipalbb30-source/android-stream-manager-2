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
echo -e "${BLUE}🚀 Instalador de Produção do Android Stream Manager 🚀${NC}"
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

# --- 3. Coleta de Informações para Produção ---
echo -e "\n${YELLOW}📝 Etapa 1/5: Configuração do ambiente de produção...${NC}"

read -p "Por favor, insira o nome de domínio (hostname) para este servidor (ex: stream.suaempresa.com): " SERVER_HOSTNAME
if [ -z "$SERVER_HOSTNAME" ]; then
    echo -e "${RED}ERRO: O nome de domínio é obrigatório para a configuração de produção.${NC}"
    exit 1
fi

echo -e "${GREEN}Hostname configurado para: $SERVER_HOSTNAME${NC}"

# --- 3. Instalação de Dependências (Debian/Ubuntu) ---
echo -e "\n${YELLOW}🔧 Etapa 2/5: Instalando dependências do sistema...${NC}"

DEPS=(
    build-essential
    cmake
    git
    pkg-config
    # Dependências diretas do projeto (encontradas via find_package)
    libssl-dev         # Para OpenSSL (TLS, JWT)
    zlib1g-dev
    libsqlite3-dev
    # libprocps-dev é opcional e foi removido para evitar erros de instalação.
    # Dependências para o Dashboard Qt e processamento de vídeo (FFmpeg)
    qt6-base-dev
    qt6-websockets-dev
    qt6-multimedia-dev
    libavcodec-dev
    libavformat-dev    # FFmpeg: formatos de contêiner
    libavutil-dev
    libswscale-dev
    libxkbcommon-dev   # Dependência de runtime para Qt em servidores
    # Ferramentas para o APK Builder
    openjdk-17-jdk
    unzip
)

apt-get update
apt-get install -y "${DEPS[@]}"

echo -e "\n${GREEN}✅ Dependências instaladas com sucesso.${NC}"

# --- 4. Compilação do Projeto ---
echo -e "\n${YELLOW}🏗️ Etapa 3/5: Compilando o projeto para produção...${NC}"

if [ -d "build" ]; then
    echo "Diretório 'build' existente. Removendo para uma compilação limpa..."
    rm -rf build
fi

mkdir build
cd build

echo "Configurando com CMake..."
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF

echo "Compilando com make (utilizando todos os cores disponíveis)..."
if ! make -j$(nproc); then
    echo -e "${RED}ERRO: A compilação falhou. Verifique os erros acima.${NC}"
    exit 1
fi

cd ..
echo -e "\n${GREEN}✅ Projeto compilado com sucesso! Binários estão em 'build/bin'.${NC}"

# --- 5. Verificação de Certificados SSL ---
echo -e "\n${YELLOW}🔐 Etapa 4/5: Verificando certificados SSL...${NC}"
CERT_DIR="/etc/android-stream-manager/certs"
mkdir -p "$CERT_DIR" # Garante que o diretório exista

if [ -f "${CERT_DIR}/server.crt" ] && [ -f "${CERT_DIR}/server.key" ]; then
    echo -e "${GREEN}✅ Certificados SSL encontrados em ${CERT_DIR}.${NC}"
else
    echo -e "${YELLOW}AVISO: Certificados SSL não encontrados em ${CERT_DIR}.${NC}"
    echo -e "${YELLOW}Para produção, você DEVE fornecer certificados válidos ('server.crt' e 'server.key').${NC}"
    echo -e "${YELLOW}Você pode usar Let's Encrypt (certbot) ou outro provedor de sua escolha.${NC}"
fi

# --- 6. Instalação do Sistema ---
echo -e "\n${YELLOW}⚙️ Etapa 5/5: Configurando o sistema (usuário, serviço, diretórios...)${NC}"

if [ ! -f "scripts/init_system.sh" ]; then
    echo -e "${RED}ERRO: O script 'scripts/init_system.sh' não foi encontrado!${NC}"
    exit 1
fi

chmod +x scripts/init_system.sh
# Passa o hostname para o script de inicialização
scripts/init_system.sh "$SERVER_HOSTNAME"

echo -e "\n${GREEN}✅ Configuração do sistema concluída.${NC}"

# --- Conclusão ---
echo -e "\n${BLUE}===================================================${NC}"
echo -e "${GREEN}🎉 Instalação de Produção Finalizada! 🎉${NC}"
echo -e "${BLUE}===================================================${NC}"
echo -e "\n${YELLOW}⚠️ PRÓXIMOS PASSOS OBRIGATÓRIOS:${NC}"
echo -e "1. ${YELLOW}Configure um registro DNS para o seu hostname '${SERVER_HOSTNAME}' apontando para o IP deste servidor.${NC}"
echo ""
echo -e "2. ${YELLOW}Instale certificados SSL válidos em '${CERT_DIR}'.${NC}"
echo -e "   Exemplo usando Let's Encrypt: sudo certbot certonly --standalone -d ${SERVER_HOSTNAME}"
echo -e "   Depois, copie os arquivos para o diretório correto."
echo ""
echo -e "3. ${YELLOW}Edite o arquivo de ambiente com suas chaves e senhas:${NC}"
echo -e "   sudo nano /etc/default/android-stream-manager"
echo -e "   (Especialmente JWT_SECRET, KEYSTORE_PASSWORD, e ANDROID_SDK_ROOT se for construir APKs)"
echo ""
echo -e "4. ${YELLOW}Após configurar tudo, inicie e habilite o serviço:${NC}"
echo -e "   sudo systemctl start android-stream-manager"
echo -e "   sudo systemctl enable android-stream-manager"
echo ""

exit 0