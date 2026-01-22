# Makefile para Android Stream Manager
# Build simplificado para desenvolvimento

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -pthread -fPIC -Icore -Ishared -Isecurity -Ioptimization -Icompliance -I./_deps/jwt_cpp-src/include
LDFLAGS = -pthread -lssl -lcrypto -lz -llz4 -lzip

# Diretórios
SRC_DIR = .
OBJ_DIR = obj
BIN_DIR = bin
LIB_DIR = lib

# Fontes
CORE_SOURCES = \
	core/system_manager.cpp \
	core/apk_builder.cpp \
	core/device_manager.cpp \
	shared/protocol.cpp

SECURITY_SOURCES = \
	security/tls_manager.cpp \
	security/jwt_manager.cpp \
	security/apk_signer.cpp

COMPLIANCE_SOURCES = \
	compliance/compliance_manager.cpp

OPTIMIZATION_SOURCES = \
	optimization/stream_optimizer.cpp \
	optimization/build_cache.cpp \
	optimization/thread_pool.cpp

ALL_SOURCES = $(CORE_SOURCES) $(SECURITY_SOURCES) $(COMPLIANCE_SOURCES) $(OPTIMIZATION_SOURCES)
ALL_OBJECTS = $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(ALL_SOURCES))

# Targets principais
.PHONY: all clean build test install deps help

all: build

# Build da biblioteca core
build: dirs $(LIB_DIR)/libcore.a $(BIN_DIR)/test_system

$(LIB_DIR)/libcore.a: $(ALL_OBJECTS)
	@echo "Criando biblioteca core..."
	@mkdir -p $(LIB_DIR)
	ar rcs $@ $^

$(OBJ_DIR)/%.o: %.cpp
	@echo "Compilando $<..."
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Teste simples
$(BIN_DIR)/test_system: test_main.cpp $(LIB_DIR)/libcore.a
	@echo "Compilando teste..."
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -I. $^ $(LDFLAGS) -o $@

# Dependências
deps:
	@echo "Instalando dependências..."
	@echo "Ubuntu/Debian:"
	@echo "  sudo apt install build-essential cmake libssl-dev liblz4-dev libzip-dev zlib1g-dev"
	@echo "  sudo apt install qt6-base-dev qt6-tools-dev qt6-multimedia-dev  # Para dashboard"
	@echo ""
	@echo "CentOS/RHEL:"
	@echo "  sudo yum install gcc-c++ cmake openssl-devel lz4-devel libzip-devel zlib-devel"
	@echo ""
	@echo "macOS:"
	@echo "  brew install cmake openssl lz4 libzip zlib"

# CMake build completo
cmake-build: deps
	@echo "Build com CMake..."
	@mkdir -p build
	cd build && cmake .. -DCMAKE_BUILD_TYPE=Release
	cd build && make -j$(nproc)

cmake-debug: deps
	@echo "Build com CMake (Debug)..."
	@mkdir -p build
	cd build && cmake .. -DCMAKE_BUILD_TYPE=Debug
	cd build && make -j$(nproc)

# Testes
test: $(BIN_DIR)/test_system
	@echo "Executando testes..."
	./$(BIN_DIR)/test_system

# Limpeza
clean:
	@echo "Limpando arquivos de build..."
	rm -rf $(OBJ_DIR) $(BIN_DIR) $(LIB_DIR) build/

# Instalação
install: build
	@echo "Instalando..."
	sudo cp $(LIB_DIR)/libcore.a /usr/local/lib/
	sudo mkdir -p /usr/local/include/android-stream-manager
	sudo cp -r shared/*.h /usr/local/include/android-stream-manager/
	sudo cp -r core/*.h /usr/local/include/android-stream-manager/
	sudo cp -r optimization/*.h /usr/local/include/android-stream-manager/
	sudo cp -r security/*.h /usr/local/include/android-stream-manager/
	sudo cp -r compliance/*.h /usr/local/include/android-stream-manager/

# Desinstalação
uninstall:
	@echo "Desinstalando..."
	sudo rm -f /usr/local/lib/libcore.a
	sudo rm -rf /usr/local/include/android-stream-manager

# Diretórios
dirs:
	@mkdir -p $(OBJ_DIR) $(BIN_DIR) $(LIB_DIR)

# Android APK build (se Android SDK estiver disponível)
android-build: $(LIB_DIR)/libcore.a
	@echo "Build do APK Android..."
	@echo "Certifique-se de que ANDROID_SDK_ROOT está configurado"
	@if [ -z "$$ANDROID_SDK_ROOT" ]; then \
		echo "ANDROID_SDK_ROOT não configurado"; \
		exit 1; \
	fi
	@echo "SDK encontrado em: $$ANDROID_SDK_ROOT"
	# TODO: Implementar build do APK

# Documentação
docs:
	@echo "Gerando documentação..."
	doxygen Doxyfile 2>/dev/null || echo "Doxygen não encontrado. Instale com: sudo apt install doxygen"

# Análise de código
lint:
	@echo "Executando análise de código..."
	clang-tidy src/*.cpp src/**/*.cpp -- -std=c++17 -I. 2>/dev/null || echo "clang-tidy não encontrado"

# Formatação de código
format:
	@echo "Formatando código..."
	clang-format -i src/*.cpp src/**/*.cpp src/*.h src/**/*.h 2>/dev/null || echo "clang-format não encontrado"

# Contagem de linhas
count:
	@echo "Contagem de linhas:"
	@find . -name "*.cpp" -o -name "*.h" | grep -v build | xargs wc -l | tail -1

# Status do projeto
status:
	@echo "=== Status do Projeto ==="
	@echo "Arquivos C++: $(shell find . -name "*.cpp" | wc -l)"
	@echo "Arquivos Header: $(shell find . -name "*.h" | wc -l)"
	@echo "Arquivos Java: $(shell find . -name "*.java" | wc -l)"
	@echo "Linhas de código: $(shell find . -name "*.cpp" -o -name "*.h" -o -name "*.java" | xargs wc -l | tail -1)"
	@echo ""
	@echo "=== Build Status ==="
	@if [ -f "$(LIB_DIR)/libcore.a" ]; then echo "✓ Biblioteca core: OK"; else echo "✗ Biblioteca core: Faltando"; fi
	@if [ -f "$(BIN_DIR)/test_system" ]; then echo "✓ Teste básico: OK"; else echo "✗ Teste básico: Faltando"; fi

# Help
help:
	@echo "Comandos disponíveis:"
	@echo "  make build       - Compila biblioteca core"
	@echo "  make test        - Executa testes básicos"
	@echo "  make clean       - Limpa arquivos de build"
	@echo "  make install     - Instala biblioteca no sistema"
	@echo "  make uninstall   - Remove biblioteca do sistema"
	@echo "  make deps        - Mostra como instalar dependências"
	@echo "  make cmake-build - Build completo com CMake"
	@echo "  make status      - Mostra status do projeto"
	@echo "  make count       - Conta linhas de código"
	@echo "  make format      - Formata código (clang-format)"
	@echo "  make lint        - Análise de código (clang-tidy)"
	@echo "  make docs        - Gera documentação (doxygen)"
	@echo "  make help        - Mostra esta ajuda"