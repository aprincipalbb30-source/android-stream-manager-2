#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <string>
#include <vector>
#include <cstdint>

namespace AndroidStreamManager {

// Protocolo de comunicação entre dashboard e dispositivos
struct ControlMessage {
    enum class Type {
        START_STREAM,
        PAUSE_STREAM,
        STOP_STREAM,
        RESTART,
        TAKE_SCREENSHOT,
        START_AUDIO,
        STOP_AUDIO,
        UPDATE_SETTINGS
    };
    
    Type type;
    std::string deviceId;
    std::string operatorId;
    uint64_t timestamp;
    std::vector<uint8_t> payload; // Configurações adicionais
    std::string signature; // Assinatura digital
};

struct StreamData {
    std::string deviceId;
    uint32_t frameNumber;
    uint64_t timestamp;
    enum class DataType {
        VIDEO_H264,
        VIDEO_H265,
        AUDIO_AAC,
        AUDIO_OPUS,
        SENSOR_DATA,
        DEVICE_INFO
    };
    DataType dataType;
    std::vector<uint8_t> data;
};

// Autenticação e segurança
class SecurityManager {
public:
    static std::string generateToken(const std::string& operatorId,
                                     const std::string& secret);
    static bool validateToken(const std::string& token,
                              const std::string& secret);
    
    static std::vector<uint8_t> encryptData(const std::vector<uint8_t>& data,
                                           const std::string& key);
    static std::vector<uint8_t> decryptData(const std::vector<uint8_t>& encrypted,
                                           const std::string& key);
    
    static std::string calculateHMAC(const std::string& data,
                                     const std::string& key);
};

} // namespace AndroidStreamManager

#endif // PROTOCOL_H