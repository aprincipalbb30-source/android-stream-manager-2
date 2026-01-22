#!/bin/bash

# Script de Instalação Automática para Android Stream Manager no WSL Ubuntu

# --- Funções Auxiliares ---

log_info() {
    echo -e "\\e[34m[INFO]\\e[0m $1"
}

log_success() {
    echo -e "\\e[32m[SUCCESS]\\e[0m $1"
}

log_warning() {
    echo -e "\\e[33m[WARNING]\\e[0m $1"
}

log_error() {
    echo -e "\\e[31m[ERROR]\\e[0m $1"
    exit 1
}

check_command() {
    if ! command -v "$1" &> /dev/null; then
        log_error "Comando '$1' não encontrado. Certifique-se de que está no PATH."
    fi
}

# --- Configurações Iniciais ---

export DEBIAN_FRONTEND=noninteractive
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
PROJECT_ROOT_DIR="android-stream-manager" # Nome esperado do diretório do projeto
REPO_URL="https://github.com/aprincipalbb30-source/android-stream-manager.git"

# Se o script está sendo executado de dentro do diretório do projeto clonado,
# então SCRIPT_DIR já é o PROJECT_DIR.
if [[ "$(basename "$SCRIPT_DIR")" == "$PROJECT_ROOT_DIR" ]]; then
    PROJECT_DIR="$SCRIPT_DIR"
    log_info "Executando de dentro do diretório do projeto: $PROJECT_DIR"
elif [ -d "$SCRIPT_DIR/$PROJECT_ROOT_DIR" ]; then
    # O diretório do projeto existe, mas não é onde o script foi iniciado
    PROJECT_DIR="$SCRIPT_DIR/$PROJECT_ROOT_DIR"
    log_info "Diretório do projeto encontrado em: $PROJECT_DIR. Navegando para ele."
    cd "$PROJECT_DIR" || log_error "Não foi possível mudar para o diretório do projeto."
else
    # O diretório do projeto não existe, então vamos clonar
    log_info "Repositório não encontrado. Clonando $REPO_URL para $SCRIPT_DIR/$PROJECT_ROOT_DIR..."
    git clone "$REPO_URL" "$SCRIPT_DIR/$PROJECT_ROOT_DIR" || log_error "Falha ao clonar o repositório."
    PROJECT_DIR="$SCRIPT_DIR/$PROJECT_ROOT_DIR"
    cd "$PROJECT_DIR" || log_error "Não foi possível mudar para o diretório do projeto clonado."
    log_success "Repositório clonado e diretório do projeto definido para: $PROJECT_DIR"
fi
ANDROID_SDK_INSTALL_DIR="$HOME/android-sdk"

log_info "Iniciando a instalação automática do Android Stream Manager..."

# --- PASSO 1: PREPARAÇÃO DO AMBIENTE WSL UBUNTU ---
log_info "PASSO 1: Preparando o ambiente WSL Ubuntu..."

log_info "Atualizando o sistema..."
sudo apt update || log_error "Falha ao atualizar o apt."
sudo apt upgrade -y || log_error "Falha ao fazer upgrade do sistema."

log_info "Instalando ferramentas básicas..."
sudo apt install -y \
    git \
    curl \
    wget \
    build-essential \
    cmake \
    ninja-build \
    pkg-config \
    software-properties-common \
    apt-transport-https \
    ca-certificates \
    gnupg \
    lsb-release \
    libsqlite3-dev || log_error "Falha ao instalar ferramentas básicas."

# --- PASSO 2: INSTALAÇÃO DAS DEPENDÊNCIAS DO SISTEMA ---
log_info "PASSO 2: Instalando dependências do sistema..."

log_info "Instalando bibliotecas essenciais..."
sudo apt install -y \
    libssl-dev \
    zlib1g-dev \
    liblz4-dev \
    libzip-dev \
    uuid-dev || log_error "Falha ao instalar bibliotecas essenciais."

log_info "Instalando Qt6 para o Dashboard..."
sudo apt install -y \
    qt6-base-dev \
    qt6-tools-dev \
    qt6-multimedia-dev \
    qt6-declarative-dev \
    qt6-websockets-dev \
    libopengl-dev || log_error "Falha ao instalar Qt6."

log_info "Instalando ferramentas de desenvolvimento..."
sudo apt install -y \
    clang \
    clang-format \
    clang-tidy \
    valgrind \
    gdb \
    doxygen \
    graphviz || log_error "Falha ao instalar ferramentas de desenvolvimento."

# --- PASSO 3: PREPARAR O PROJETO ---
log_info "PASSO 3: Preparando o projeto..."

log_info "Dando permissões de execução aos scripts (se existirem)..."
find "$PROJECT_DIR" -type f -name "*.sh" -exec chmod +x {} \;
chmod +x "$PROJECT_DIR/tutorial-install-linux.sh" 2>/dev/null || true # Ignora erro se não existir

# --- PASSO 4: BUILD BÁSICO (TESTE RÁPIDO) ---
log_info "PASSO 4: Realizando build básico (make)..."

cd "$PROJECT_DIR" || log_error "Não foi possível mudar para o diretório do projeto."
check_command "make"

log_info "Executando 'make build'..."
make build || log_error "Falha no 'make build'. Verifique o erro acima."
log_success "Build básico concluído."

log_info "Executando 'make test'..."
make test || log_error "Falha no 'make test'. Verifique o erro acima."
log_success "Testes básicos concluídos."

# --- PASSO 5: BUILD COMPLETO COM CMAKE ---
log_info "PASSO 5: Realizando build completo com CMake..."

mkdir -p "$PROJECT_DIR/build" || log_error "Falha ao criar diretório 'build'."
cd "$PROJECT_DIR/build" || log_error "Não foi possível mudar para o diretório 'build'."
check_command "cmake"

log_info "Configurando com CMake..."
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_DASHBOARD=ON \
    -DBUILD_APK_BUILDER=ON \
    -DBUILD_SERVER=ON \
    -DBUILD_TESTS=OFF || log_error "Falha na configuração do CMake."
log_success "Configuração do CMake concluída."

log_info "Compilando o projeto com CMake..."
make -j$(nproc) || log_error "Falha na compilação com CMake. Verifique o erro acima."
log_success "Compilação completa com CMake concluída."

log_info "Verificando executáveis gerados:"
ls -la bin/

# --- PASSO 6: CONFIGURAÇÃO DO ANDROID SDK ---
log_info "PASSO 6: Configurando o Android SDK (necessário para build de APKs)..."

mkdir -p "$ANDROID_SDK_INSTALL_DIR" || log_error "Falha ao criar diretório para Android SDK."
cd "$ANDROID_SDK_INSTALL_DIR" || log_error "Não foi possível mudar para o diretório do Android SDK."

log_info "Baixando Command Line Tools do Android..."
wget -q --show-progress https://dl.google.com/android/repository/commandlinetools-linux-9477386_latest.zip -O commandlinetools.zip || log_error "Falha ao baixar Command Line Tools."
unzip -q commandlinetools.zip -d cmdline-tools || log_error "Falha ao extrair Command Line Tools."
rm commandlinetools.zip

# Estrutura esperada: cmdline-tools/cmdline-tools/latest
mv cmdline-tools/cmdline-tools/* cmdline-tools/
rmdir cmdline-tools/cmdline-tools

# Configurar variáveis de ambiente temporariamente para o script e permanentemente
export ANDROID_SDK_ROOT="$ANDROID_SDK_INSTALL_DIR"
export ANDROID_HOME="$ANDROID_SDK_ROOT"
export PATH="$PATH:$ANDROID_SDK_ROOT/cmdline-tools/bin:$ANDROID_SDK_ROOT/platform-tools"

if ! grep -q "ANDROID_SDK_ROOT" "$HOME/.bashrc"; then
    log_info "Configurando variáveis de ambiente do Android SDK no ~/.bashrc..."
    echo 'export ANDROID_SDK_ROOT="'"$ANDROID_SDK_INSTALL_DIR"'"' >> "$HOME/.bashrc"
    echo 'export ANDROID_HOME="$ANDROID_SDK_ROOT"' >> "$HOME/.bashrc"
    echo 'export PATH="$PATH:$ANDROID_SDK_ROOT/cmdline-tools/bin:$ANDROID_SDK_ROOT/platform-tools"' >> "$HOME/.bashrc"
    log_info "Por favor, execute 'source ~/.bashrc' em novos terminais para carregar as variáveis."
else
    log_info "Variáveis de ambiente do Android SDK já configuradas no ~/.bashrc."
fi

log_info "Instalando componentes do Android SDK (pode demorar)..."
yes | sdkmanager --licenses > /dev/null 2>&1 # Aceita licenças automaticamente
sdkmanager --install "platform-tools" "platforms;android-33" "build-tools;33.0.0" "ndk;25.2.9519653" "cmake;3.22.1" || log_error "Falha ao instalar componentes do Android SDK."
log_success "Componentes do Android SDK instalados."

# --- PASSO 7: CONFIGURAÇÃO DO SISTEMA ---
log_info "PASSO 7: Configurando arquivos e diretórios do sistema..."

sudo mkdir -p /etc/android-stream-manager || log_error "Falha ao criar /etc/android-stream-manager."
sudo cp "$PROJECT_DIR/config/system_config.json" /etc/android-stream-manager/ || log_error "Falha ao copiar system_config.json."

sudo mkdir -p \
    /var/lib/android-stream-manager/{apks,cache,sessions} \
    /var/log/android-stream-manager \
    /usr/local/share/android-stream-manager/templates || log_error "Falha ao criar diretórios de sistema."

if [ -d "$PROJECT_DIR/templates" ]; then
    sudo cp -r "$PROJECT_DIR/templates"/* /usr/local/share/android-stream-manager/templates/ 2>/dev/null || true
    log_info "Templates copiados (se existirem)."
else
    log_warning "Diretório 'templates' não encontrado. Ignorando cópia de templates."
fi

# --- PASSO 8: BUILD DO APK ANDROID ---
log_info "PASSO 8: Realizando build do APK Android..."

cd "$PROJECT_DIR/android-client-template" || log_error "Não foi possível mudar para o diretório do cliente Android."
check_command "gradlew"

log_info "Executando './gradlew assembleDebug' (pode demorar)..."
./gradlew assembleDebug || log_error "Falha no build do APK. Verifique as mensagens do Gradle."
log_success "APK Android buildado com sucesso."

log_info "Verificando APK gerado:"
ls -la app/build/outputs/apk/debug/

# --- PASSO 9: INSTALAÇÃO DA BIBLIOTECA CORE (OPCIONAL) ---
log_info "PASSO 9: Instalando a biblioteca core no sistema (opcional)..."
cd "$PROJECT_DIR" || log_error "Não foi possível mudar para o diretório do projeto."
make install || log_warning "Falha em 'make install'. Isso pode ser ignorado para execução local."
log_success "Instalação da biblioteca core concluída (se bem-sucedida)."

log_info "Verificação rápida dos executáveis instalados:"
ls -la "$PROJECT_DIR/build/bin/"

log_success "Instalação completa do Android Stream Manager concluída!"
log_info "Para executar o Dashboard: cd $PROJECT_DIR/build/bin && ./android_stream_dashboard"
log_info "Para executar o Servidor: cd $PROJECT_DIR/build/bin && ./stream_server --port 8443"
log_info "O APK está em: $PROJECT_DIR/android-client-template/app/build/outputs/apk/debug/app-debug.apk"
log_info "Lembre-se de rodar 'source ~/.bashrc' em novos terminais para carregar as variáveis do Android SDK."
