#ifndef TLS_MANAGER_H
#define TLS_MANAGER_H

#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509.h>
#include <openssl/evp.h>

namespace AndroidStreamManager {

class TlsManager {
public:
    static TlsManager& getInstance();
    
    // Inicialização
    bool initialize(const std::string& caCertPath = "",
                   const std::string& clientCertPath = "",
                   const std::string& privateKeyPath = "");
    void cleanup();
    
    // Criação de contextos SSL
    SSL_CTX* createServerContext();
    SSL_CTX* createClientContext();
    
    // Verificação de certificado
    bool verifyCertificate(X509* cert);
    
    // Geração de chaves
    static std::pair<std::string, std::string> generateRSAKeys(int bits = 2048);
    static std::string generateECKey();
    
    // Utilidades
    static std::string calculateSHA256(const std::string& data);
    static std::string calculateSHA256(const std::vector<uint8_t>& data);
    
private:
    TlsManager();
    ~TlsManager();
    TlsManager(const TlsManager&) = delete;
    TlsManager& operator=(const TlsManager&) = delete;
    
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

// Cliente TLS seguro
class SecureTlsClient {
public:
    SecureTlsClient();
    ~SecureTlsClient();
    
    bool connect(const std::string& host, int port, 
                 const std::string& serverName = "");
    bool send(const std::vector<uint8_t>& data);
    std::vector<uint8_t> receive();
    void disconnect();
    
    bool isConnected() const;
    std::string getCipherInfo() const;
    
private:
    SSL* ssl = nullptr;
    int sockfd = -1;
    SSL_CTX* ctx = nullptr;
};

} // namespace AndroidStreamManager

#endif // TLS_MANAGER_H