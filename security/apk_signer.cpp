#include "apk_signer.h"
#include <fstream>
#include <sstream>
#include <zip/zip.h>
#include <iostream>

extern "C" {
#include <openssl/conf.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/pkcs12.h>
}

namespace AndroidStreamManager {

class ApkSigner::Impl {
public:
    bool signWithKeysImpl(const std::string& apkPath,
                         const std::string& certPem,
                         const std::string& privateKeyPem,
                         const std::string& outputPath) {
        
        // Ler certificado
        BIO* certBio = BIO_new_mem_buf(certPem.c_str(), -1);
        X509* cert = PEM_read_bio_X509(certBio, nullptr, nullptr, nullptr);
        BIO_free(certBio);
        
        if (!cert) return false;
        
        // Ler chave privada
        BIO* keyBio = BIO_new_mem_buf(privateKeyPem.c_str(), -1);
        EVP_PKEY* privateKey = PEM_read_bio_PrivateKey(keyBio, nullptr, nullptr, nullptr);
        BIO_free(keyBio);
        
        if (!privateKey) {
            X509_free(cert);
            return false;
        }
        
        // Criar contexto de assinatura
        PKCS7* p7 = PKCS7_new();
        PKCS7_set_type(p7, NID_pkcs7_signed);
        
        // Adicionar certificado
        STACK_OF(X509)* certs = sk_X509_new_null();
        sk_X509_push(certs, cert);
        PKCS7_set0_type_other(p7, 1);
        
        // Assinar
        PKCS7_SIGNER_INFO* si = PKCS7_add_signature(p7, cert, privateKey, EVP_sha256());
        if (!si) {
            PKCS7_free(p7);
            EVP_PKEY_free(privateKey);
            X509_free(cert);
            return false;
        }
        
        // Adicionar atributos
        PKCS7_add_signed_attribute(si, NID_pkcs9_contentType, 
            V_ASN1_OBJECT, OBJ_nid2obj(NID_pkcs7_data));
        
        // Criar bloco de assinatura
        std::vector<uint8_t> signatureBlock;
        if (!createSignatureBlock(p7, signatureBlock)) {
            PKCS7_free(p7);
            EVP_PKEY_free(privateKey);
            X509_free(cert);
            return false;
        }
        
        // Copiar APK e adicionar assinatura
        std::ifstream src(apkPath, std::ios::binary);
        std::ofstream dst(outputPath, std::ios::binary);
        
        if (!src || !dst) {
            return false;
        }
        
        dst << src.rdbuf();
        
        // Adicionar assinatura ao final do arquivo
        dst.write(reinterpret_cast<const char*>(signatureBlock.data()), 
                 signatureBlock.size());
        
        // Adicionar tamanho da assinatura
        uint32_t sigSize = signatureBlock.size();
        dst.write(reinterpret_cast<const char*>(&sigSize), sizeof(sigSize));
        
        PKCS7_free(p7);
        EVP_PKEY_free(privateKey);
        X509_free(cert);
        
        return true;
    }
    
private:
    bool createSignatureBlock(PKCS7* p7, std::vector<uint8_t>& output) {
        BIO* bio = BIO_new(BIO_s_mem());
        i2d_PKCS7_bio(bio, p7);
        
        BUF_MEM* mem = nullptr;
        BIO_get_mem_ptr(bio, &mem);
        
        if (mem) {
            output.assign(mem->data, mem->data + mem->length);
        }
        
        BIO_free(bio);
        return !output.empty();
    }
};

ApkSigner::ApkSigner() : pImpl(std::make_unique<Impl>()) {}
ApkSigner::~ApkSigner() = default;

bool ApkSigner::signWithKeys(const std::string& apkPath,
                            const std::string& certPem,
                            const std::string& privateKeyPem,
                            const std::string& outputPath) {
    return pImpl->signWithKeysImpl(apkPath, certPem, privateKeyPem, outputPath);
}

bool ApkSigner::generateKeystore(const std::string& path,
                                const std::string& password,
                                const std::string& alias,
                                const std::string& dname,
                                int validityYears) {
    
    std::string cmd = "keytool -genkeypair "
                      "-keystore \"" + path + "\" "
                      "-storepass \"" + password + "\" "
                      "-alias \"" + alias + "\" "
                      "-keyalg RSA -keysize 2048 "
                      "-validity " + std::to_string(validityYears * 365) + " "
                      "-dname \"" + dname + "\"";
    
    return system(cmd.c_str()) == 0;
}

// Implementação do gerenciador de assinaturas corporativas
CorporateSigningManager& CorporateSigningManager::getInstance() {
    static CorporateSigningManager instance;
    return instance;
}

void CorporateSigningManager::initialize(const std::string& masterKeyPath,
                                        const std::string& certChainPath) {
    std::lock_guard<std::mutex> lock(mutex);
    
    // Carregar chave mestra
    std::ifstream keyFile(masterKeyPath);
    if (keyFile) {
        std::stringstream buffer;
        buffer << keyFile.rdbuf();
        masterKey = buffer.str();
    }
}

std::string CorporateSigningManager::signBuild(const std::string& buildId,
                                              const std::string& apkHash,
                                              const std::string& operatorId) {
    std::lock_guard<std::mutex> lock(mutex);
    
    // Criar dados para assinatura
    std::string data = buildId + "|" + apkHash + "|" + operatorId + "|" +
                      std::to_string(std::chrono::system_clock::now()
                      .time_since_epoch().count());
    
    // Assinar com HMAC-SHA256
    std::string signature = TlsManager::calculateSHA256(data + masterKey);
    
    // Registrar
    SignatureRecord record;
    record.buildId = buildId;
    record.apkHash = apkHash;
    record.operatorId = operatorId;
    record.signature = signature;
    record.timestamp = std::chrono::system_clock::now();
    
    history.push_back(record);
    
    return signature;
}

} // namespace AndroidStreamManager