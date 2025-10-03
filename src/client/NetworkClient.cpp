#include "client/NetworkClient.hpp"
#include "shared/ChunkSerializer.hpp"
#include "core/Logger.hpp"

#include <cstring>
#include <stdexcept>

namespace engine {

NetworkClient::NetworkClient() {
    // Initialize ENet (safe to call multiple times)
    if (enet_initialize() != 0) {
        LOG_ERROR("Failed to initialize ENet for client");
        throw std::runtime_error("Failed to initialize ENet");
    }

    // Create client host (no server address, 1 connection, 2 channels)
    client = enet_host_create(nullptr, 1, 2, 0, 0);
    if (client == nullptr) {
        LOG_ERROR("Failed to create ENet client host");
        throw std::runtime_error("Failed to create ENet client host");
    }

    LOG_INFO("Network client initialized");
}

NetworkClient::~NetworkClient() {
    disconnect();

    if (client != nullptr) {
        enet_host_destroy(client);
        client = nullptr;
    }

    enet_deinitialize();
}

bool NetworkClient::connect(const std::string& host, uint16_t port, uint32_t timeout) {
    if (connected) {
        LOG_WARN("Already connected to server");
        return true;
    }

    LOG_INFO("Connecting to {}:{}...", host, port);

    // Resolve server address
    ENetAddress address;
    if (enet_address_set_host(&address, host.c_str()) != 0) {
        LOG_ERROR("Failed to resolve host: {}", host);
        return false;
    }
    address.port = port;

    // Initiate connection (2 channels, no user data)
    serverPeer = enet_host_connect(client, &address, 2, 0);
    if (serverPeer == nullptr) {
        LOG_ERROR("No available peers for connection");
        return false;
    }

    // Wait for connection to complete
    ENetEvent event;
    if (enet_host_service(client, &event, timeout) > 0 &&
        event.type == ENET_EVENT_TYPE_CONNECT) {
        LOG_INFO("Connected to server successfully");
        connected = true;

        // Send join message
        protocol::ClientJoinMessage joinMsg;
        std::strncpy(joinMsg.playerName, "Player", sizeof(joinMsg.playerName) - 1);
        joinMsg.clientVersion = 1;
        sendMessage(protocol::MessageType::ClientJoin, &joinMsg, sizeof(joinMsg));

        return true;
    } else {
        LOG_ERROR("Connection to {}:{} failed (timeout)", host, port);
        enet_peer_reset(serverPeer);
        serverPeer = nullptr;
        return false;
    }
}

void NetworkClient::disconnect() {
    if (!connected || serverPeer == nullptr) {
        return;
    }

    LOG_INFO("Disconnecting from server...");

    // Send disconnect notification
    enet_peer_disconnect(serverPeer, 0);

    // Wait for disconnect to complete (up to 3 seconds)
    ENetEvent event;
    while (enet_host_service(client, &event, 3000) > 0) {
        switch (event.type) {
            case ENET_EVENT_TYPE_RECEIVE:
                enet_packet_destroy(event.packet);
                break;

            case ENET_EVENT_TYPE_DISCONNECT:
                LOG_INFO("Disconnected from server");
                connected = false;
                serverPeer = nullptr;
                chunks.clear();
                return;

            default:
                break;
        }
    }

    // Force disconnect if graceful disconnect didn't work
    enet_peer_reset(serverPeer);
    serverPeer = nullptr;
    connected = false;
    chunks.clear();
}

void NetworkClient::update() {
    if (!connected) {
        return;
    }

    ENetEvent event;

    // Process all pending events (non-blocking)
    while (enet_host_service(client, &event, 0) > 0) {
        switch (event.type) {
            case ENET_EVENT_TYPE_RECEIVE:
                handlePacket(event.packet);
                enet_packet_destroy(event.packet);
                break;

            case ENET_EVENT_TYPE_DISCONNECT:
                LOG_WARN("Disconnected from server unexpectedly");
                connected = false;
                serverPeer = nullptr;
                chunks.clear();
                break;

            default:
                break;
        }
    }
}

void NetworkClient::sendPlayerMove(const glm::vec3& position, const glm::vec3& velocity, float yaw, float pitch) {
    if (!connected) return;

    protocol::PlayerMoveMessage msg;
    msg.position = position;
    msg.velocity = velocity;
    msg.yaw = yaw;
    msg.pitch = pitch;
    msg.inputFlags = 0; // Input state currently handled client-side, will be needed for server-authoritative movement

    sendMessage(protocol::MessageType::PlayerMove, &msg, sizeof(msg));
}

void NetworkClient::sendBlockPlace(int32_t x, int32_t y, int32_t z, uint16_t blockType) {
    if (!connected) return;

    protocol::BlockPlaceMessage msg;
    msg.x = x;
    msg.y = y;
    msg.z = z;
    msg.blockType = blockType;

    sendMessage(protocol::MessageType::BlockPlace, &msg, sizeof(msg));
}

void NetworkClient::sendBlockBreak(int32_t x, int32_t y, int32_t z) {
    if (!connected) return;

    protocol::BlockBreakMessage msg;
    msg.x = x;
    msg.y = y;
    msg.z = z;

    sendMessage(protocol::MessageType::BlockBreak, &msg, sizeof(msg));
}

Chunk* NetworkClient::getChunk(const ChunkCoord& coord) {
    auto it = chunks.find(coord);
    return (it != chunks.end()) ? it->second.get() : nullptr;
}

const Chunk* NetworkClient::getChunk(const ChunkCoord& coord) const {
    auto it = chunks.find(coord);
    return (it != chunks.end()) ? it->second.get() : nullptr;
}

void NetworkClient::handlePacket(ENetPacket* packet) {
    if (packet->dataLength < sizeof(protocol::MessageHeader)) {
        LOG_WARN("Received malformed packet (too small)");
        return;
    }

    // Read message header
    protocol::MessageHeader header;
    std::memcpy(&header, packet->data, sizeof(protocol::MessageHeader));

    const uint8_t* payload = packet->data + sizeof(protocol::MessageHeader);
    size_t payloadSize = packet->dataLength - sizeof(protocol::MessageHeader);

    // Dispatch based on message type
    switch (header.type) {
        case protocol::MessageType::ChunkData:
            handleChunkData(payload, payloadSize);
            break;

        case protocol::MessageType::ChunkUnload:
            if (payloadSize >= sizeof(protocol::ChunkUnloadMessage)) {
                protocol::ChunkUnloadMessage msg;
                std::memcpy(&msg, payload, sizeof(msg));
                handleChunkUnload(msg);
            }
            break;

        case protocol::MessageType::BlockUpdate:
            if (payloadSize >= sizeof(protocol::BlockUpdateMessage)) {
                protocol::BlockUpdateMessage msg;
                std::memcpy(&msg, payload, sizeof(msg));
                handleBlockUpdate(msg);
            }
            break;

        default:
            LOG_TRACE("Received unhandled message type: {}", static_cast<int>(header.type));
            break;
    }
}

void NetworkClient::handleChunkData(const uint8_t* data, size_t size) {
    if (size < sizeof(protocol::ChunkDataMessage)) {
        LOG_WARN("Malformed chunk data message");
        return;
    }

    // Read chunk header
    protocol::ChunkDataMessage header;
    std::memcpy(&header, data, sizeof(protocol::ChunkDataMessage));

    const uint8_t* compressedData = data + sizeof(protocol::ChunkDataMessage);
    size_t compressedSize = size - sizeof(protocol::ChunkDataMessage);

    // Create chunk and deserialize
    auto chunk = std::make_unique<Chunk>(header.coord);
    if (!ChunkSerializer::deserialize(compressedData, compressedSize, *chunk)) {
        LOG_ERROR("Failed to deserialize chunk at ({}, {}, {})",
                  header.coord.x, header.coord.y, header.coord.z);
        return;
    }

    LOG_INFO("Received chunk ({}, {}, {}) | Compressed: {} bytes",
             header.coord.x, header.coord.y, header.coord.z, compressedSize);

    // Store chunk
    chunks[header.coord] = std::move(chunk);

    // Notify callback
    if (onChunkReceived) {
        onChunkReceived(header.coord);
    }
}

void NetworkClient::handleChunkUnload(const protocol::ChunkUnloadMessage& msg) {
    auto it = chunks.find(msg.coord);
    if (it != chunks.end()) {
        LOG_INFO("Unloading chunk ({}, {}, {})", msg.coord.x, msg.coord.y, msg.coord.z);
        chunks.erase(it);
    }
}

void NetworkClient::handleBlockUpdate(const protocol::BlockUpdateMessage& msg) {
    // Find chunk containing this block
    ChunkCoord chunkCoord = ChunkCoord::fromWorldPos(glm::vec3(msg.x, msg.y, msg.z));
    Chunk* chunk = getChunk(chunkCoord);

    if (chunk == nullptr) {
        LOG_WARN("Received block update for unloaded chunk ({}, {}, {})",
                 chunkCoord.x, chunkCoord.y, chunkCoord.z);
        return;
    }

    // Convert world coords to local chunk coords
    glm::vec3 worldOrigin = chunkCoord.toWorldPos();
    uint32_t localX = msg.x - static_cast<int32_t>(worldOrigin.x);
    uint32_t localY = msg.y - static_cast<int32_t>(worldOrigin.y);
    uint32_t localZ = msg.z - static_cast<int32_t>(worldOrigin.z);

    // Update block
    Block block;
    block.type = static_cast<BlockType>(msg.blockType);
    chunk->setBlock(localX, localY, localZ, block);

    LOG_TRACE("Block updated at ({}, {}, {}) to type {}", msg.x, msg.y, msg.z, msg.blockType);
}

void NetworkClient::sendMessage(protocol::MessageType type, const void* data, size_t size) {
    if (!connected || serverPeer == nullptr) {
        return;
    }

    // Create packet with header + payload
    size_t totalSize = sizeof(protocol::MessageHeader) + size;
    ENetPacket* packet = enet_packet_create(nullptr, totalSize, ENET_PACKET_FLAG_RELIABLE);

    // Write header
    protocol::MessageHeader header;
    header.type = type;
    header.payloadSize = static_cast<uint32_t>(size);
    std::memcpy(packet->data, &header, sizeof(protocol::MessageHeader));

    // Write payload
    if (size > 0 && data != nullptr) {
        std::memcpy(packet->data + sizeof(protocol::MessageHeader), data, size);
    }

    // Send packet
    enet_peer_send(serverPeer, 0, packet);
}

} // namespace engine
