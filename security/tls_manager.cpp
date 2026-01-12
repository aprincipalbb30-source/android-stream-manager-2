#include "tls_manager.h"
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <array>
#include <chrono>

namespace AndroidStreamManager {

class TlsManager::Impl {
public:
    Impl() {
        OPENSSL_init_ssl(0, nullptr);
        OPENSSL_init_crypto(OPENSSL_INIT_LOAD_CONFIG, nullptr);
    }
    
    ~Impl() {
        if (serverCtx) SSL_CTX_free(serverCtx);
        if (clientCtx) SSL_CTX_free(clientCtx);
    }
    
    SSL_CTX* serverCtx = nullptr;
    SSL_CTX* clientCtx = nullptr;
    std::mutex mutex;
    
    bool loadCertificates(const std::string& caCertPath,
                         const std::string& clientCertPath,
                         const std::string& privateKeyPath) {
        std::lock_guard<std::mutex> lock(mutex);
        
        // Contexto servidor
        serverCtx = SSL_CTX_new(TLS_server_method());
        if (!serverCtx) return false;
        
        SSL_CTX_set_min_proto_version(serverCtx, TLS1_3_VERSION);
        SSL_CTX_set_options(serverCtx, SSL_OP_NO_COMPRESSION);
        
        // Contexto cliente
        clientCtx = SSL_CTX_new(TLS_client_method());
        if (!clientCtx) return false;
        
        SSL_CTX_set_min_proto_version(clientCtx, TLS1_3_VERSION);
        SSL_CTX_set_options(clientCtx, SSL_OP_NO_COMPRESSION);
        
        // Carregar certificados
        if (!caCertPath.empty()) {
            if (SSL_CTX_load_verify_locations(serverCtx, 
                caCertPath.c_str(), nullptr) != 1) {
                return false;
            }
            if (SSL_CTX_load_verify_locations(clientCtx, 
                caCertPath.c_str(), nullptr) != 1) {
                return false;
            }
        }
        
        if (!clientCertPath.empty() && !privateKeyPath.empty()) {
            if (SSL_CTX_use_certificate_file(serverCtx, 
                clientCertPath.c_str(), SSL_FILETYPE_PEM) != 1) {
                return false;
            }
            if (SSL_CTX_use_PrivateKey_file(serverCtx, 
                privateKeyPath.c_str(), SSL_FILETYPE_PEM) != 1) {
                return false;
            }
            
            // Verificar chave privada
            if (SSL_CTX_check_private_key(serverCtx) != 1) {
                return false;
            }
        }
        
        // Configurações de segurança
        SSL_CTX_set_cipher_list(serverCtx, 
            "TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256");
        SSL_CTX_set_cipher_list(clientCtx, 
            "TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256");
        
        // Verificação estrita
        SSL_CTX_set_verify(serverCtx, 
            SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, nullptr);
        SSL_CTX_set_verify(clientCtx, SSL_VERIFY_PEER, nullptr);
        
        return true;
    }
};

TlsManager::TlsManager() : pImpl(std::make_unique<Impl>()) {}
TlsManager::~TlsManager() = default;

TlsManager& TlsManager::getInstance() {
    static TlsManager instance;
    return instance;
}

bool TlsManager::initialize(const std::string& caCertPath,
                           const std::string& clientCertPath,
                           const std::string& privateKeyPath) {
    return pImpl->loadCertificates(caCertPath, clientCertPath, privateKeyPath);
}

void TlsManager::cleanup() {
    // OpenSSL limpa automaticamente na saída
}

SSL_CTX* TlsManager::createServerContext() {
    return pImpl->serverCtx;
}

SSL_CTX* TlsManager::createClientContext() {
    return pImpl->clientCtx;
}

bool TlsManager::verifyCertificate(X509* cert) {
    if (!cert) return false;
    
    // Verificar validade
    int result = X509_cmp_current_time(X509_get_notBefore(cert));
    if (result >= 0) return false; // Certificado ainda não válido
    
    result = X509_cmp_current_time(X509_get_notAfter(cert));
    if (result <= 0) return false; // Certificado expirado
    
    return true;
}

std::pair<std::string, std::string> TlsManager::generateRSAKeys(int bits) {
    EVP_PKEY* pkey = EVP_PKEY_new();
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
    
    if (!pkey || !ctx) {
        return {"", ""};
    }
    
    if (EVP_PKEY_keygen_init(ctx) <= 0 ||
        EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, bits) <= 0 ||
        EVP_PKEY_keygen(ctx, &pkey) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return {"", ""};
    }
    
    // Exportar chave pública
    BIO* pub = BIO_new(BIO_s_mem());
    PEM_write_bio_PUBKEY(pub, pkey);
    char* pubData = nullptr;
    long pubLen = BIO_get_mem_data(pub, &pubData);
    std::string publicKey(pubData, pubLen);
    
    // Exportar chave privada
    BIO* priv = BIO_new(BIO_s_mem());
    PEM_write_bio_PrivateKey(priv, pkey, nullptr, nullptr, 0, nullptr, nullptr);
    char* privData = nullptr;
    long privLen = BIO_get_mem_data(priv, &privData);
    std::string privateKey(privData, privLen);
    
    BIO_free(pub);
    BIO_free(priv);
    EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_free(pkey);
    
    return {publicKey, privateKey};
}

std::string TlsManager::calculateSHA256(const std::string& data) {
    return calculateSHA256(
        std::vector<uint8_t>(data.begin(), data.end()));
}

std::string TlsManager::calculateSHA256(const std::vector<uint8_t>& data) {
    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    const EVP_MD* md = EVP_sha256();
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hashLen = 0;
    
    if (!mdctx) return "";
    
    EVP_DigestInit_ex(mdctx, md, nullptr);
    EVP_DigestUpdate(mdctx, data.data(), data.size());
    EVP_DigestFinal_ex(mdctx, hash, &hashLen);
    EVP_MD_CTX_free(mdctx);
    
    std::stringstream ss;
    for (unsigned int i = 0; i < hashLen; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') 
           << static_cast<int>(hash[i]);
    }
    
    return ss.str();
}

// Implementação do cliente TLS
SecureTlsClient::SecureTlsClient() {
    ctx = TlsManager::getInstance().createClientContext();
}

SecureTlsClient::~SecureTlsClient() {
    disconnect();
}

bool SecureTlsClient::connect(const std::string& host, int port, 
                             const std::string& serverName) {
    // Criar socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) return false;
    
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, host.c_str(), &addr.sin_addr);
    
    if (::connect(sockfd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sockfd);
        return false;
    }
    
    // Criar SSL
    ssl = SSL_new(ctx);
    if (!ssl) {
        close(sockfd);
        return false;
    }
    
    SSL_set_fd(ssl, sockfd);
    
    if (!serverName.empty()) {
        SSL_set_tlsext_host_name(ssl, serverName.c_str());
    }
    
    if (SSL_connect(ssl) != 1) {
        SSL_free(ssl);
        close(sockfd);
        return false;
    }
    
    return true;
}

bool SecureTlsClient::send(const std::vector<uint8_t>& data) {
    if (!isConnected()) return false;
    
    int written = SSL_write(ssl, data.data(), static_cast<int>(data.size()));
    return written == static_cast<int>(data.size());
}

std::vector<uint8_t> SecureTlsClient::receive() {
    if (!isConnected()) return {};
    
    std::array<uint8_t, 4096> buffer;
    int bytes = SSL_read(ssl, buffer.data(), buffer.size());
    
    if (bytes <= 0) return {};
    return std::vector<uint8_t>(buffer.begin(), buffer.begin() + bytes);
}

void SecureTlsClient::disconnect() {
    if (ssl) {
        SSL_shutdown(ssl);
        SSL_free(ssl);
        ssl = nullptr;
    }
    if (sockfd >= 0) {
        close(sockfd);
        sockfd = -1;
    }
}

bool SecureTlsClient::isConnected() const {
    return ssl != nullptr && sockfd >= 0;
}

std::string SecureTlsClient::getCipherInfo() const {
    if (!isConnected()) return "";
    return SSL_get_cipher_name(ssl);
}

} // namespace AndroidStreamManager