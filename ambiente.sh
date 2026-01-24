#!/bin/bash
#
# ambiente.sh - Script de Preparação de Ambiente
#
# Este script prepara o ambiente em sistemas Debian/Ubuntu para a instalação
# do Android Stream Manager, instalando todas as dependências necessárias.
#

set -e

# --- Cores para o output ---
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}======================================================${NC}"
echo -e "${BLUE}🔧 Preparador de Ambiente para Android Stream Manager 🔧${NC}"
echo -e "${BLUE}======================================================${NC}"

# --- 1. Verificação de Root ---
if [ "$EUID" -ne 0 ]; then
    echo -e "${RED}ERRO: Este script precisa ser executado como root (ou com sudo).${NC}"
    echo -e "${YELLOW}Tente: sudo ./ambiente.sh${NC}"
    exit 1
fi

echo -e "\n${GREEN}✅ Verificação de permissões concluída.${NC}"

# --- 2. Instalação de Dependências (Debian/Ubuntu) ---
echo -e "\n${YELLOW}Etapa 1/2: Instalando dependências do sistema...${NC}"
echo "Isso pode levar alguns minutos."

# Lista de dependências extraída do README e scripts de instalação
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
    openjdk-17-jdk
    unzip
)

apt-get update
apt-get install -y "${DEPS[@]}"

echo -e "\n${GREEN}✅ Dependências instaladas com sucesso.${NC}"

# --- 3. Verificação do Android SDK ---
echo -e "\n${YELLOW}Etapa 2/2: Verificando o Android SDK...${NC}"

echo -e "O Android Stream Manager precisa do Android SDK para construir os APKs."
echo -e "O script de instalação irá configurar um caminho padrão, mas é recomendado"
echo -e "que você instale o SDK manualmente para ter mais controle."
echo ""
echo -e "${YELLOW}AVISO: Após a instalação, lembre-se de definir a variável de ambiente ANDROID_SDK_ROOT${NC}"
echo -e "${YELLOW}no arquivo '/etc/default/android-stream-manager' que será criado pelo instalador.${NC}"
echo -e "Exemplo: ANDROID_SDK_ROOT=/home/seu_usuario/Android/sdk"
echo ""

# --- Conclusão ---
echo -e "\n${BLUE}======================================================${NC}"
echo -e "${GREEN}🎉 Ambiente preparado com sucesso! 🎉${NC}"
echo -e "${BLUE}======================================================${NC}"
echo -e "\n${YELLOW}Próximo passo:${NC}"
echo -e "Execute o script de instalação principal para compilar e configurar o serviço:"
echo -e "   ${GREEN}sudo ./database/install.sh${NC}"
echo ""

exit 0
