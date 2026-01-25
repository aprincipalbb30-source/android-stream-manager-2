#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <string>
#include <vector>
#include <cstdint>
#include <optional>
#include <nlohmann/json_fwd.hpp> // Forward declaration para json

namespace AndroidStreamManager {

// Estrutura de mensagem genérica usada pela implementação em protocol.cpp
struct Message {
    std::string type;
    long long timestamp;
    std::optional<nlohmann::json> payload;
};

// Classe de protocolo para serialização/desserialização
class Protocol {
public:
    static std::string serialize(const Message& msg);
    static std::optional<Message> deserialize(const std::string& data);
    static std::string createHelloMessage(const std::string& deviceId, const std::string& deviceModel);
    static std::string createFrameMessage(const std::vector<unsigned char>& frameData, int width, int height);
    static std::string createCommandMessage(const std::string& command, const nlohmann::json& args);
    static std::string createResponseMessage(int original_cmd_id, bool success, const std::string& details);
    static std::string createHeartbeatMessage();
    static std::string createAuthRequestMessage(const std::string& token);
};

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