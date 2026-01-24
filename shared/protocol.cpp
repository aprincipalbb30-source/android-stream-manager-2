#include "protocol.h"
#include <nlohmann/json.hpp>
#include <iostream>

namespace AndroidStreamManager {

using json = nlohmann::json;

// Helper para obter valor com fallback
template<typename T>
T get_optional(const json& j, const std::string& key, const T& default_value) {
    return j.contains(key) ? j.at(key).get<T>() : default_value;
}

std::string Protocol::serialize(const Message& msg) {
    json j;
    j["type"] = msg.type;
    j["timestamp"] = msg.timestamp;

    if (msg.payload.has_value()) {
        j["payload"] = msg.payload.value();
    }

    return j.dump();
}

std::optional<Message> Protocol::deserialize(const std::string& data) {
    try {
        auto j = json::parse(data);
        Message msg;

        if (!j.contains("type")) {
            return std::nullopt;
        }

        msg.type = j["type"];
        msg.timestamp = get_optional<long long>(j, "timestamp", 0);

        if (j.contains("payload")) {
            msg.payload = j["payload"];
        }

        return msg;
    } catch (const json::parse_error& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
        return std::nullopt;
    }
}

std::string Protocol::createHelloMessage(const std::string& deviceId, const std::string& deviceModel) {
    Message msg;
    msg.type = "hello";
    msg.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    msg.payload = {
        {"deviceId", deviceId},
        {"deviceModel", deviceModel}
    };
    return serialize(msg);
}

std::string Protocol::createFrameMessage(const std::vector<unsigned char>& frameData, int width, int height) {
    Message msg;
    msg.type = "video_frame";
    msg.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    msg.payload = {
        {"width", width},
        {"height", height},
        // Os dados do frame serão enviados como binário, não JSON.
        // Este é um placeholder se quiséssemos enviar metadados.
    };
    // A serialização aqui é apenas para os metadados; o frame real é anexado.
    return serialize(msg);
}

std::string Protocol::createCommandMessage(const std::string& command, const json& args) {
    Message msg;
    msg.type = "command";
    msg.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    msg.payload = {
        {"command", command},
        {"args", args}
    };
    return serialize(msg);
}

std::string Protocol::createResponseMessage(int original_cmd_id, bool success, const std::string& details) {
    Message msg;
    msg.type = "response";
    msg.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    msg.payload = {
        {"original_cmd_id", original_cmd_id},
        {"success", success},
        {"details", details}
    };
    return serialize(msg);
}

std::string Protocol::createHeartbeatMessage() {
    Message msg;
    msg.type = "heartbeat";
    msg.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    return serialize(msg);
}

std::string Protocol::createAuthRequestMessage(const std::string& token) {
    Message msg;
    msg.type = "auth_request";
    msg.payload = {{"token", token}};
    return serialize(msg);
}

} // namespace AndroidStreamManager
