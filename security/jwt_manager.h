#ifndef JWT_MANAGER_H
#define JWT_MANAGER_H

#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <mutex>
#include <jwt-cpp/jwt.h>

namespace AndroidStreamManager {

struct JwtClaims {
    std::string operatorId;
    std::string deviceId;
    std::string role; // "admin", "operator", "viewer"
    std::vector<std::string> permissions;
    std::chrono::system_clock::time_point issuedAt;
    std::chrono::system_clock::time_point expiresAt;
    std::string sessionId;
};

class JwtManager {
public:
    static JwtManager& getInstance();
    
    // Gerenciamento de chaves
    void initialize(const std::string& secret, 
                   const std::chrono::seconds& keyRotationInterval = std::chrono::hours(24));
    void rotateKeys();
    
    // Geração e validação
    std::string generateToken(const JwtClaims& claims);
    bool validateToken(const std::string& token, JwtClaims& claims);
    bool verifyToken(const std::string& token);
    
    // Token de dispositivo
    std::string generateDeviceToken(const std::string& deviceId,
                                   const std::string& apkConfigId);
    
    // Revogação
    void revokeToken(const std::string& tokenId);
    bool isTokenRevoked(const std::string& tokenId);
    
    // Chaves públicas para verificação
    std::string getCurrentPublicKey();
    
private:
    JwtManager();
    
    struct KeyPair {
        std::string id;
        std::string publicKey;
        std::string privateKey;
        std::chrono::system_clock::time_point createdAt;
        bool active;
    };
    
    std::vector<KeyPair> keyPairs;
    std::unordered_map<std::string, std::chrono::system_clock::time_point> revokedTokens;
    
    std::mutex mutex;
    std::chrono::seconds keyRotationInterval;
    std::string currentKeyId;
    
    KeyPair generateKeyPair();
    std::string signWithKey(const std::string& data, const KeyPair& key);
    bool verifyWithKey(const std::string& data, const std::string& signature, 
                      const KeyPair& key);
};

} // namespace AndroidStreamManager

#endif // JWT_MANAGER_H