#include "jwt_manager.h"
#include <sstream>
#include <iomanip>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <base64/base64.h>

namespace AndroidStreamManager {

JwtManager& JwtManager::getInstance() {
    static JwtManager instance;
    return instance;
}

JwtManager::JwtManager() 
    : keyRotationInterval(std::chrono::hours(24)) {}

void JwtManager::initialize(const std::string& secret, 
                           const std::chrono::seconds& rotationInterval) {
    std::lock_guard<std::mutex> lock(mutex);
    
    keyRotationInterval = rotationInterval;
    
    // Gerar par de chaves inicial
    auto keyPair = generateKeyPair();
    keyPairs.push_back(keyPair);
    currentKeyId = keyPair.id;
}

JwtManager::KeyPair JwtManager::generateKeyPair() {
    KeyPair pair;
    pair.id = std::to_string(
        std::chrono::system_clock::now().time_since_epoch().count());
    pair.createdAt = std::chrono::system_clock::now();
    pair.active = true;
    
    // Gerar chaves RSA
    auto keys = TlsManager::generateRSAKeys(2048);
    pair.publicKey = keys.first;
    pair.privateKey = keys.second;
    
    return pair;
}

void JwtManager::rotateKeys() {
    std::lock_guard<std::mutex> lock(mutex);
    
    auto now = std::chrono::system_clock::now();
    
    // Desativar chaves antigas
    for (auto& keyPair : keyPairs) {
        if (now - keyPair.createdAt > keyRotationInterval) {
            keyPair.active = false;
        }
    }
    
    // Gerar nova chave
    auto newKeyPair = generateKeyPair();
    keyPairs.push_back(newKeyPair);
    currentKeyId = newKeyPair.id;
    
    // Limitar histórico de chaves
    if (keyPairs.size() > 5) {
        keyPairs.erase(keyPairs.begin());
    }
}

std::string JwtManager::generateToken(const JwtClaims& claims) {
    std::lock_guard<std::mutex> lock(mutex);
    
    auto it = std::find_if(keyPairs.begin(), keyPairs.end(),
        [this](const KeyPair& kp) { return kp.id == currentKeyId; });
    
    if (it == keyPairs.end()) return "";
    
    // Criar payload JWT
    auto now = std::chrono::system_clock::now();
    auto token = jwt::create()
        .set_issuer("android-stream-manager")
        .set_subject(claims.operatorId)
        .set_audience(claims.deviceId)
        .set_issued_at(now)
        .set_expires_at(claims.expiresAt)
        .set_payload_claim("role", jwt::claim(claims.role))
        .set_payload_claim("session_id", jwt::claim(claims.sessionId))
        .set_payload_claim("permissions", 
            jwt::claim(std::vector<std::string>{claims.permissions}))
        .sign(jwt::algorithm::rs256(it->publicKey, it->privateKey));
    
    return token;
}

bool JwtManager::validateToken(const std::string& token, JwtClaims& claims) {
    if (!verifyToken(token)) return false;
    
    try {
        auto decoded = jwt::decode(token);
        
        claims.operatorId = decoded.get_subject();
        claims.deviceId = decoded.get_audience();
        claims.role = decoded.get_payload_claim("role").as_string();
        claims.sessionId = decoded.get_payload_claim("session_id").as_string();
        
        auto perms = decoded.get_payload_claim("permissions").as_array();
        for (const auto& perm : perms) {
            claims.permissions.push_back(perm.as_string());
        }
        
        return true;
    } catch (...) {
        return false;
    }
}

bool JwtManager::verifyToken(const std::string& token) {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (isTokenRevoked(token)) return false;
    
    // Tentar verificar com todas as chaves ativas
    for (const auto& keyPair : keyPairs) {
        if (!keyPair.active) continue;
        
        try {
            auto verifier = jwt::verify()
                .allow_algorithm(jwt::algorithm::rs256(keyPair.publicKey))
                .with_issuer("android-stream-manager");
            
            verifier.verify(jwt::decode(token));
            return true;
        } catch (...) {
            continue;
        }
    }
    
    return false;
}

std::string JwtManager::generateDeviceToken(const std::string& deviceId,
                                           const std::string& apkConfigId) {
    JwtClaims claims;
    claims.operatorId = "system";
    claims.deviceId = deviceId;
    claims.role = "device";
    claims.sessionId = apkConfigId;
    claims.issuedAt = std::chrono::system_clock::now();
    claims.expiresAt = claims.issuedAt + std::chrono::hours(24);
    
    return generateToken(claims);
}

void JwtManager::revokeToken(const std::string& tokenId) {
    std::lock_guard<std::mutex> lock(mutex);
    revokedTokens[tokenId] = std::chrono::system_clock::now();
}

bool JwtManager::isTokenRevoked(const std::string& tokenId) {
    std::lock_guard<std::mutex> lock(mutex);
    return revokedTokens.find(tokenId) != revokedTokens.end();
}

std::string JwtManager::getCurrentPublicKey() {
    std::lock_guard<std::mutex> lock(mutex);
    
    auto it = std::find_if(keyPairs.begin(), keyPairs.end(),
        [this](const KeyPair& kp) { return kp.id == currentKeyId; });
    
    return it != keyPairs.end() ? it->publicKey : "";
}

} // namespace AndroidStreamManager