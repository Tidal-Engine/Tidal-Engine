#include "network/NetworkProtocol.h"
#include <cstring>

NetworkPacket::NetworkPacket(MessageType type, const void* data, size_t size) {
    header.type = type;
    header.size = static_cast<uint32_t>(size);
    header.sequence = 0; // Will be set by network layer

    if (data && size > 0) {
        payload.resize(size);
        std::memcpy(payload.data(), data, size);
    }
}

std::vector<uint8_t> NetworkPacket::serialize() const {
    std::vector<uint8_t> data;
    data.reserve(sizeof(MessageHeader) + payload.size());

    // Serialize header
    const uint8_t* headerBytes = reinterpret_cast<const uint8_t*>(&header);
    data.insert(data.end(), headerBytes, headerBytes + sizeof(MessageHeader));

    // Serialize payload
    if (!payload.empty()) {
        data.insert(data.end(), payload.begin(), payload.end());
    }

    return data;
}

bool NetworkPacket::deserialize(const uint8_t* data, size_t size, NetworkPacket& packet) {
    if (size < sizeof(MessageHeader)) {
        return false;
    }

    // Deserialize header
    std::memcpy(&packet.header, data, sizeof(MessageHeader));

    // Check if we have enough data for the payload
    if (size < sizeof(MessageHeader) + packet.header.size) {
        return false;
    }

    // Deserialize payload
    packet.payload.clear();
    if (packet.header.size > 0) {
        packet.payload.resize(packet.header.size);
        std::memcpy(packet.payload.data(), data + sizeof(MessageHeader), packet.header.size);
    }

    return true;
}