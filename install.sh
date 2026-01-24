#!/bin/bash
#
# install.sh - Script de Instalação Completa para o Android Stream Manager
#
# Este script automatiza a instalação de dependências, compilação do projeto
# e configuração do sistema como um serviço em distribuições baseadas em Debian/Ubuntu.
#

set -e

# --- Cores para o output ---
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}===================================================${NC}"
echo -e "${BLUE}🚀 Instalador do Android Stream Manager 🚀${NC}"
echo -e "${BLUE}===================================================${NC}"

# --- 1. Verificação de Root ---
if [ "$EUID" -ne 0 ]; then
    echo -e "${RED}ERRO: Este script precisa ser executado como root (ou com sudo).${NC}"
    echo -e "${YELLOW}Tente: sudo ./install.sh${NC}"
    exit 1
fi

# --- 2. Verificação do Diretório ---
if [ ! -f "CMakeLists.txt" ] || [ ! -d "core" ]; then
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
    libssl-dev
    zlib1g-dev
    liblz4-dev
    libzip-dev
    qt6-base-dev
    libsqlite3-dev
)

apt-get update
apt-get install -y "${DEPS[@]}"

echo -e "\n${GREEN}✅ Dependências instaladas com sucesso.${NC}"

# --- 4. Compilação do Projeto ---
echo -e "\n${YELLOW}🏗️ Etapa 2/4: Compilando o projeto...${NC}"

if [ -d "build" ]; then
    echo "Diretório 'build' existente. Removendo para uma compilação limpa..."
    rm -rf build
fi

mkdir build
cd build

echo "Configurando com CMake..."
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF

echo "Compilando com make (utilizando todos os cores disponíveis)..."
make -j$(nproc)

cd ..
echo -e "\n${GREEN}✅ Projeto compilado com sucesso! Binários estão em 'build/bin'.${NC}"

# --- 5. Geração de Certificados SSL (Self-signed) ---
echo -e "\n${YELLOW}🔐 Etapa 3/4: Gerando certificados SSL autoassinados...${NC}"

if [ -f "server.key" ] && [ -f "server.crt" ]; then
    echo "Certificados SSL já existem. Pulando esta etapa."
else
    openssl req -x509 -newkey rsa:4096 -keyout server.key -out server.crt -days 365 -nodes \
    -subj "/C=BR/ST=SaoPaulo/L=SaoPaulo/O=AndroidStreamManager/OU=Dev/CN=localhost"
    echo -e "${GREEN}✅ Certificados 'server.key' e 'server.crt' gerados.${NC}"
fi

# --- 6. Instalação do Sistema ---
echo -e "\n${YELLOW}⚙️ Etapa 4/4: Configurando o sistema (usuário, serviço, diretórios...)${NC}"

if [ ! -f "scripts/init_system.sh" ]; then
    echo -e "${RED}ERRO: O script 'scripts/init_system.sh' não foi encontrado!${NC}"
    exit 1
fi

chmod +x scripts/init_system.sh
./scripts/init_system.sh

echo -e "\n${GREEN}✅ Configuração do sistema concluída.${NC}"

# --- Conclusão ---
echo -e "\n${BLUE}===================================================${NC}"
echo -e "${GREEN}🎉 Instalação do Android Stream Manager finalizada! 🎉${NC}"
echo -e "${BLUE}===================================================${NC}"
echo -e "\n${YELLOW}⚠️ PRÓXIMOS PASSOS OBRIGATÓRIOS:${NC}"
echo -e "1. ${YELLOW}Edite o arquivo de configuração de ambiente com suas chaves e senhas:${NC}"
echo -e "   sudo nano /etc/default/android-stream-manager"
echo ""
echo -e "2. ${YELLOW}Inicie o serviço:${NC}"
echo -e "   sudo systemctl start android-stream-manager"
echo ""
echo -e "3. ${YELLOW}Verifique o status e os logs do serviço:${NC}"
echo -e "   sudo systemctl status android-stream-manager"
echo -e "   sudo journalctl -u android-stream-manager -f"
echo ""
echo -e "4. ${YELLOW}Compile e instale o APK no dispositivo Android (veja README_STREAMING.md).${NC}"
echo ""

exit 0