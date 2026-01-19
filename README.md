# 🚀 Android Stream Manager

Sistema corporativo completo para **gerenciamento remoto de dispositivos Android** com foco em segurança enterprise, streaming em tempo real e construção automatizada de aplicações customizadas.

[![C++](https://img.shields.io/badge/C%2B%2B-17-blue)](https://isocpp.org/)
[![Java](https://img.shields.io/badge/Java-8-orange)](https://java.com/)
[![Qt](https://img.shields.io/badge/Qt-6-green)](https://qt.io/)
[![License](https://img.shields.io/badge/License-MIT-yellow)](LICENSE)

## 📱 Funcionalidades Principais

### 🎥 **Streaming em Tempo Real**
- Captura de tela HD
- Streaming de áudio
- Transmissão de dados de sensores
- Controle remoto completo

### 📦 **Construtor de APKs**
- Geração automatizada de aplicações
- Templates customizáveis
- Assinatura corporativa
- Configurações dinâmicas

### 🖥️ **Dashboard Corporativo**
- Interface Qt moderna
- Gerenciamento de dispositivos
- Monitoramento em tempo real
- Relatórios e analytics

### 🔐 **Segurança Enterprise**
- TLS 1.3 com certificados mútuos
- Autenticação JWT com rotação
- Controle de acesso baseado em roles
- Auditoria completa de operações

## 🏗️ Arquitetura

```
android-stream-manager/
├── core/                    # Núcleo do sistema
│   ├── system_manager       # Gerenciador central
│   ├── apk_builder          # Construtor de APKs
│   └── device_manager       # Gerenciamento de dispositivos
├── security/               # Camada de segurança
│   ├── tls_manager         # Conexões seguras
│   ├── jwt_manager         # Autenticação
│   └── apk_signer          # Assinatura digital
├── optimization/           # Otimizações de performance
│   ├── thread_pool         # Pool de threads
│   ├── build_cache         # Cache inteligente
│   └── stream_optimizer    # Otimizador de streaming
├── dashboard/              # Interface gráfica Qt
├── android-client-template/# Template Android
├── compliance/             # Conformidade e auditoria
└── config/                 # Configurações do sistema
```

## 🚀 Instalação Rápida

### Pré-requisitos
- Ubuntu 20.04+ / WSL
- CMake 3.16+
- Qt6 (opcional, para dashboard)
- Android SDK (opcional, para build de APKs)

### Instalação Automática (WSL Ubuntu)

```bash
# 1. Instalar dependências
sudo apt update && sudo apt install -y \
    build-essential cmake libssl-dev zlib1g-dev \
    liblz4-dev libzip-dev qt6-base-dev

# 2. Clonar e compilar
git clone https://github.com/aprincipalbb30-source/android-stream-manager.git
cd android-stream-manager

# 3. Build básico
make build

# 4. Build completo
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# 5. Testar
make test
```

Para instruções detalhadas, consulte [`tutorial.txt`](tutorial.txt).

## 📖 Uso Básico

### 1. **Iniciar Dashboard**
```bash
./bin/android_stream_dashboard
```

### 2. **Construir APK**
```bash
./bin/apk_builder --config config/apk_config.json
```

### 3. **Instalar no Dispositivo**
```bash
adb install android-client-template/app/build/outputs/apk/debug/app-debug.apk
```

## 🔧 Desenvolvimento

### Build para Desenvolvimento
```bash
# Build rápido
make build && make test

# Build completo com CMake
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON
make -j$(nproc)

# Limpar
make clean
```

### Estrutura de Testes
```bash
# Executar todos os testes
make test

# Testes específicos
cd build && ctest --output-on-failure
```

## 📋 API e Protocolos

### Protocolo de Comunicação
- **WebSocket** para comunicação em tempo real
- **TLS 1.3** para criptografia
- **JWT** para autenticação
- **Protobuf** para serialização eficiente

### Endpoints Principais
- `wss://server:8443/ws` - WebSocket principal
- `https://server:8443/api/v1/devices` - API REST
- `https://server:9090/metrics` - Métricas Prometheus

## 🔒 Segurança

### Recursos de Segurança Implementados
- ✅ Criptografia TLS 1.3
- ✅ Autenticação JWT com rotação de chaves
- ✅ Controle de acesso RBAC
- ✅ Auditoria completa
- ✅ Assinatura digital de APKs
- ✅ Comunicação segura device-server

### Configuração de Segurança
```json
{
  "security": {
    "tls": {
      "enabled": true,
      "min_version": "TLS1.3",
      "certificate_authority": "./certs/ca.crt"
    },
    "jwt": {
      "key_rotation_hours": 12,
      "token_lifetime_hours": 8
    }
  }
}
```

## 📊 Monitoramento

### Métricas Disponíveis
- Dispositivos conectados
- Taxa de transferência
- Latência de streaming
- Utilização de cache
- Taxa de hit do cache

### Dashboard de Monitoramento
- Interface web em tempo real
- Métricas Prometheus
- Alertas configuráveis
- Logs centralizados

## 🧪 Testes

### Testes Unitários
```bash
# Build com testes
cmake .. -DBUILD_TESTS=ON
make && make test
```

### Testes de Integração
```bash
# Testar comunicação
nc -zv localhost 8443

# Testar API
curl -k https://localhost:8443/api/health
```

## 📚 Documentação

- [**Tutorial de Instalação**](tutorial.txt) - Guia completo para WSL Ubuntu
- [**Arquitetura do Sistema**](docs/architecture.md) - Documentação técnica
- [**API Reference**](docs/api-reference.md) - Referência da API
- [**Guia de Segurança**](docs/security-guide.md) - Configuração de segurança

## 🤝 Contribuição

1. Fork o projeto
2. Crie uma branch para sua feature (`git checkout -b feature/AmazingFeature`)
3. Commit suas mudanças (`git commit -m 'Add some AmazingFeature'`)
4. Push para a branch (`git push origin feature/AmazingFeature`)
5. Abra um Pull Request

### Padrões de Código
- C++17 com RAII
- Java 8+ com padrões Android
- Documentação Doxygen
- Testes unitários obrigatórios

## 📄 Licença

Este projeto está licenciado sob a Licença MIT - veja o arquivo [LICENSE](LICENSE) para detalhes.

## 🙏 Agradecimentos

- Qt Project - Framework de interface
- OpenSSL - Biblioteca de criptografia
- LZ4 - Compressão de dados
- Protocol Buffers - Serialização

## 📞 Suporte

- **Issues**: [GitHub Issues](https://github.com/aprincipalbb30-source/android-stream-manager/issues)
- **Wiki**: [Documentação Completa](https://github.com/aprincipalbb30-source/android-stream-manager/wiki)
- **Discussões**: [GitHub Discussions](https://github.com/aprincipalbb30-source/android-stream-manager/discussions)

---

**⚠️ AVISO**: Este sistema é projetado para uso corporativo. Certifique-se de cumprir todas as leis de privacidade e regulamentações aplicáveis ao usar este software.