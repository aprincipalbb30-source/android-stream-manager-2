#ifndef APK_SIGNER_H
#define APK_SIGNER_H

#include <string>
#include <vector>
#include <memory>
#include <openssl/pkcs7.h>
#include <openssl/x509.h>

namespace AndroidStreamManager {

struct SigningConfig {
    std::string keystorePath;
    std::string keystorePassword;
    std::string keyAlias;
    std::string keyPassword;
    std::string certPath;
    std::string privateKeyPath;
    bool v1Signing = true;  // JAR Signature
    bool v2Signing = true;  // APK Signature Scheme v2
    bool v3Signing = true;  // APK Signature Scheme v3
    bool v4Signing = false; // APK Signature Scheme v4
};

class ApkSigner {
public:
    ApkSigner();
    ~ApkSigner();
    
    bool signApk(const std::string& apkPath, 
                const std::string& outputPath,
                const SigningConfig& config);
    
    bool verifySignature(const std::string& apkPath);
    
    // Assinatura programática
    bool signWithKeys(const std::string& apkPath,
                     const std::string& certPem,
                     const std::string& privateKeyPem,
                     const std::string& outputPath);
    
    // Geração de keystore
    static bool generateKeystore(const std::string& path,
                                const std::string& password,
                                const std::string& alias,
                                const std::string& dname,
                                int validityYears = 25);
    
    // Extração de assinatura
    std::string extractSignature(const std::string& apkPath);
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
    
    bool signV1(const std::string& apkPath, 
               const SigningConfig& config);
    bool signV2V3(const std::string& apkPath,
                 const SigningConfig& config);
    bool createSignatureBlock(PKCS7* p7, 
                             std::vector<uint8_t>& output);
};

// Gerenciador de assinaturas corporativas
class CorporateSigningManager {
public:
    static CorporateSigningManager& getInstance();
    
    void initialize(const std::string& masterKeyPath,
                   const std::string& certChainPath);
    
    std::string signBuild(const std::string& buildId,
                         const std::string& apkHash,
                         const std::string& operatorId);
    
    bool verifyCorporateSignature(const std::string& signature,
                                 const std::string& buildId,
                                 const std::string& apkHash);
    
    // Registro de assinaturas
    struct SignatureRecord {
        std::string buildId;
        std::string apkHash;
        std::string operatorId;
        std::string signature;
        std::chrono::system_clock::time_point timestamp;
    };
    
    std::vector<SignatureRecord> getSignatureHistory() const;
    
private:
    CorporateSigningManager();
    
    std::string masterKey;
    std::vector<std::string> certChain;
    std::vector<SignatureRecord> history;
    std::mutex mutex;
};

} // namespace AndroidStreamManager

#endif // APK_SIGNER_H