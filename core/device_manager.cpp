#include "device_manager.h"
#include <iostream>
#include <algorithm>
#include <chrono>

namespace AndroidStreamManager {

DeviceManager::DeviceManager()
    : running_(false) {
    std::cout << "DeviceManager inicializado" << std::endl;
}

DeviceManager::~DeviceManager() {
    shutdown();
    std::cout << "DeviceManager destruído" << std::endl;
}

bool DeviceManager::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (running_) {
        std::cout << "DeviceManager já está inicializado" << std::endl;
        return true;
    }

    try {
        running_ = true;

        // Iniciar thread de manutenção
        maintenanceThread_ = std::thread(&DeviceManager::maintenanceLoop, this);

        std::cout << "DeviceManager inicializado com sucesso" << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "Erro ao inicializar DeviceManager: " << e.what() << std::endl;
        running_ = false;
        return false;
    }
}

void DeviceManager::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!running_) {
        return;
    }

    running_ = false;

    // Notificar thread de manutenção
    maintenanceCV_.notify_all();

    // Aguardar thread terminar
    if (maintenanceThread_.joinable()) {
        maintenanceThread_.join();
    }

    // Desconectar todos os dispositivos
    for (auto& pair : connectedDevices_) {
        disconnectDevice(pair.first);
    }

    connectedDevices_.clear();
    std::cout << "DeviceManager finalizado" << std::endl;
}

bool DeviceManager::registerDevice(const DeviceInfo& deviceInfo) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto deviceId = deviceInfo.deviceId;

    // Verificar se dispositivo já está registrado
    if (connectedDevices_.find(deviceId) != connectedDevices_.end()) {
        std::cout << "Dispositivo já registrado: " << deviceId << std::endl;
        return false;
    }

    DeviceSession session;
    session.deviceInfo = deviceInfo;
    session.connectedAt = std::chrono::system_clock::now();
    session.lastHeartbeat = session.connectedAt;
    session.status = DeviceStatus::CONNECTED;
    session.streamingActive = false;

    connectedDevices_[deviceId] = session;

    std::cout << "Dispositivo registrado: " << deviceId << " (" << deviceInfo.deviceModel << ")" << std::endl;

    // Notificar listeners
    notifyDeviceConnected(deviceId);

    return true;
}

bool DeviceManager::unregisterDevice(const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = connectedDevices_.find(deviceId);
    if (it == connectedDevices_.end()) {
        std::cout << "Dispositivo não encontrado para remoção: " << deviceId << std::endl;
        return false;
    }

    disconnectDevice(deviceId);
    connectedDevices_.erase(it);

    std::cout << "Dispositivo removido: " << deviceId << std::endl;

    // Notificar listeners
    notifyDeviceDisconnected(deviceId);

    return true;
}

bool DeviceManager::updateDeviceHeartbeat(const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = connectedDevices_.find(deviceId);
    if (it == connectedDevices_.end()) {
        return false;
    }

    it->second.lastHeartbeat = std::chrono::system_clock::now();
    return true;
}

bool DeviceManager::startStreaming(const std::string& deviceId, const StreamConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = connectedDevices_.find(deviceId);
    if (it == connectedDevices_.end()) {
        std::cerr << "Dispositivo não encontrado para streaming: " << deviceId << std::endl;
        return false;
    }

    if (it->second.streamingActive) {
        std::cout << "Streaming já ativo para dispositivo: " << deviceId << std::endl;
        return false;
    }

    it->second.streamConfig = config;
    it->second.streamingActive = true;
    it->second.streamStartedAt = std::chrono::system_clock::now();

    std::cout << "Streaming iniciado para dispositivo: " << deviceId << std::endl;

    // Notificar listeners
    notifyStreamingStarted(deviceId);

    return true;
}

bool DeviceManager::stopStreaming(const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = connectedDevices_.find(deviceId);
    if (it == connectedDevices_.end()) {
        std::cerr << "Dispositivo não encontrado para parar streaming: " << deviceId << std::endl;
        return false;
    }

    if (!it->second.streamingActive) {
        std::cout << "Streaming já parado para dispositivo: " << deviceId << std::endl;
        return false;
    }

    it->second.streamingActive = false;
    it->second.streamEndedAt = std::chrono::system_clock::now();

    std::cout << "Streaming parado para dispositivo: " << deviceId << std::endl;

    // Notificar listeners
    notifyStreamingStopped(deviceId);

    return true;
}

bool DeviceManager::sendCommand(const std::string& deviceId, const ControlMessage& command) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = connectedDevices_.find(deviceId);
    if (it == connectedDevices_.end()) {
        std::cerr << "Dispositivo não encontrado para comando: " << deviceId << std::endl;
        return false;
    }

    // Aqui seria enviado o comando via WebSocket ou outra conexão
    // Por enquanto apenas registramos
    std::cout << "Comando enviado para " << deviceId << ": " << static_cast<int>(command.type) << std::endl;

    return true;
}

std::vector<DeviceInfo> DeviceManager::getConnectedDevices() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<DeviceInfo> devices;
    devices.reserve(connectedDevices_.size());

    for (const auto& pair : connectedDevices_) {
        devices.push_back(pair.second.deviceInfo);
    }

    return devices;
}

DeviceSession DeviceManager::getDeviceSession(const std::string& deviceId) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = connectedDevices_.find(deviceId);
    if (it != connectedDevices_.end()) {
        return it->second;
    }

    return DeviceSession(); // Sessão vazia indica não encontrado
}

bool DeviceManager::isDeviceConnected(const std::string& deviceId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return connectedDevices_.find(deviceId) != connectedDevices_.end();
}

bool DeviceManager::isDeviceStreaming(const std::string& deviceId) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = connectedDevices_.find(deviceId);
    if (it != connectedDevices_.end()) {
        return it->second.streamingActive;
    }

    return false;
}

DeviceStats DeviceManager::getDeviceStats() const {
    std::lock_guard<std::mutex> lock(mutex_);

    DeviceStats stats;
    stats.totalDevices = connectedDevices_.size();

    stats.streamingDevices = 0;
    stats.totalUptime = std::chrono::seconds(0);

    for (const auto& pair : connectedDevices_) {
        const auto& session = pair.second;

        if (session.streamingActive) {
            stats.streamingDevices++;
        }

        auto uptime = std::chrono::system_clock::now() - session.connectedAt;
        stats.totalUptime += std::chrono::duration_cast<std::chrono::seconds>(uptime);
    }

    if (stats.totalDevices > 0) {
        stats.averageUptime = stats.totalUptime / stats.totalDevices;
    }

    return stats;
}

void DeviceManager::addDeviceListener(DeviceListener* listener) {
    std::lock_guard<std::mutex> lock(mutex_);
    listeners_.push_back(listener);
}

void DeviceManager::removeDeviceListener(DeviceListener* listener) {
    std::lock_guard<std::mutex> lock(mutex_);

    listeners_.erase(
        std::remove(listeners_.begin(), listeners_.end(), listener),
        listeners_.end()
    );
}

void DeviceManager::notifyDeviceConnected(const std::string& deviceId) {
    for (auto* listener : listeners_) {
        if (listener) {
            listener->onDeviceConnected(deviceId);
        }
    }
}

void DeviceManager::notifyDeviceDisconnected(const std::string& deviceId) {
    for (auto* listener : listeners_) {
        if (listener) {
            listener->onDeviceDisconnected(deviceId);
        }
    }
}

void DeviceManager::notifyStreamingStarted(const std::string& deviceId) {
    for (auto* listener : listeners_) {
        if (listener) {
            listener->onStreamingStarted(deviceId);
        }
    }
}

void DeviceManager::notifyStreamingStopped(const std::string& deviceId) {
    for (auto* listener : listeners_) {
        if (listener) {
            listener->onStreamingStopped(deviceId);
        }
    }
}

void DeviceManager::disconnectDevice(const std::string& deviceId) {
    auto it = connectedDevices_.find(deviceId);
    if (it != connectedDevices_.end()) {
        // Parar streaming se ativo
        if (it->second.streamingActive) {
            stopStreaming(deviceId);
        }

        // Aqui seria fechada a conexão WebSocket/TCP
        std::cout << "Dispositivo desconectado: " << deviceId << std::endl;
    }
}

void DeviceManager::maintenanceLoop() {
    std::cout << "Loop de manutenção do DeviceManager iniciado" << std::endl;

    while (running_) {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            maintenanceCV_.wait_for(lock, std::chrono::seconds(30));

            if (!running_) {
                break;
            }

            // Verificar dispositivos inativos
            checkInactiveDevices();
        }

        std::this_thread::sleep_for(std::chrono::seconds(30));
    }

    std::cout << "Loop de manutenção do DeviceManager finalizado" << std::endl;
}

void DeviceManager::checkInactiveDevices() {
    auto now = std::chrono::system_clock::now();
    auto timeout = std::chrono::minutes(5); // 5 minutos sem heartbeat

    std::vector<std::string> inactiveDevices;

    for (const auto& pair : connectedDevices_) {
        auto timeSinceHeartbeat = now - pair.second.lastHeartbeat;
        if (timeSinceHeartbeat > timeout) {
            inactiveDevices.push_back(pair.first);
        }
    }

    // Remover dispositivos inativos
    for (const auto& deviceId : inactiveDevices) {
        std::cout << "Removendo dispositivo inativo: " << deviceId << std::endl;
        unregisterDevice(deviceId);
    }
}

} // namespace AndroidStreamManager