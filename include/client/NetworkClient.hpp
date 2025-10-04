#pragma once

#include "shared/Protocol.hpp"
#include "shared/Chunk.hpp"
#include "shared/ChunkCoord.hpp"

#include <enet/enet.h>
#include <string>
#include <unordered_map>
#include <memory>
#include <functional>

namespace engine {

/**
 * @brief Client-side networking manager
 *
 * Handles connection to server, sends player input, receives world updates.
 */
class NetworkClient {
public:
    NetworkClient();
    ~NetworkClient();

    // Delete copy/move operations
    NetworkClient(const NetworkClient&) = delete;
    NetworkClient& operator=(const NetworkClient&) = delete;
    NetworkClient(NetworkClient&&) = delete;
    NetworkClient& operator=(NetworkClient&&) = delete;

    /**
     * @brief Connect to a server
     * @param host Server hostname or IP (e.g., "127.0.0.1" or "localhost")
     * @param port Server port (default: 25565)
     * @param timeout Connection timeout in milliseconds (default: 5000)
     * @return true if connected successfully
     */
    bool connect(const std::string& host, uint16_t port = 25565, uint32_t timeout = 5000);

    /**
     * @brief Disconnect from server
     */
    void disconnect();

    /**
     * @brief Check if connected to server
     */
    bool isConnected() const { return connected; }

    /**
     * @brief Process incoming network messages
     *
     * Call this every frame to handle server updates
     */
    void update();

    /**
     * @brief Send player movement to server
     */
    void sendPlayerMove(const glm::vec3& position, const glm::vec3& velocity, float yaw, float pitch);

    /**
     * @brief Send block placement request
     */
    void sendBlockPlace(int32_t x, int32_t y, int32_t z, uint16_t blockType);

    /**
     * @brief Send block break request
     */
    void sendBlockBreak(int32_t x, int32_t y, int32_t z);

    /**
     * @brief Get a received chunk (nullptr if not loaded)
     */
    Chunk* getChunk(const ChunkCoord& coord);
    const Chunk* getChunk(const ChunkCoord& coord) const;

    /**
     * @brief Get all loaded chunks
     */
    const std::unordered_map<ChunkCoord, std::unique_ptr<Chunk>>& getChunks() const { return chunks; }

    /**
     * @brief Set callback for when new chunk is received
     */
    void setOnChunkReceived(std::function<void(const ChunkCoord&)> callback) {
        onChunkReceived = callback;
    }

    /**
     * @brief Set callback for when chunk is unloaded
     */
    void setOnChunkUnloaded(std::function<void(const ChunkCoord&)> callback) {
        onChunkUnloaded = callback;
    }

    /**
     * @brief Get all other players' positions
     */
    struct PlayerData {
        glm::vec3 position;
        float yaw = 0.0f;
        float pitch = 0.0f;
    };

    const std::unordered_map<uint32_t, PlayerData>& getOtherPlayers() const { return otherPlayers; }

private:
    ENetHost* client = nullptr;
    ENetPeer* serverPeer = nullptr;
    bool connected = false;

    // Received chunks from server
    std::unordered_map<ChunkCoord, std::unique_ptr<Chunk>> chunks;

    // Other players
    std::unordered_map<uint32_t, PlayerData> otherPlayers;  ///< Player ID -> Player data (position, yaw, pitch)

    // Callbacks
    std::function<void(const ChunkCoord&)> onChunkReceived;
    std::function<void(const ChunkCoord&)> onChunkUnloaded;

    /**
     * @brief Handle received packet from server
     */
    void handlePacket(ENetPacket* packet);

    /**
     * @brief Handle chunk data message
     */
    void handleChunkData(const uint8_t* data, size_t size);

    /**
     * @brief Handle chunk unload message
     */
    void handleChunkUnload(const protocol::ChunkUnloadMessage& msg);

    /**
     * @brief Handle block update message
     */
    void handleBlockUpdate(const protocol::BlockUpdateMessage& msg);

    /**
     * @brief Send a message to server
     */
    void sendMessage(protocol::MessageType type, const void* data, size_t size);
};

} // namespace engine
