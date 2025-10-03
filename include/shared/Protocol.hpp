#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include "shared/ChunkCoord.hpp"

namespace engine::protocol {

/**
 * @brief Network message types
 */
enum class MessageType : uint8_t {
    // Client -> Server
    ClientJoin = 0,
    PlayerMove = 1,
    BlockPlace = 2,
    BlockBreak = 3,

    // Server -> Client
    ChunkData = 10,
    ChunkUnload = 11,
    BlockUpdate = 12,
    PlayerSpawn = 13,
    PlayerPositionUpdate = 14,

    // Bidirectional
    Disconnect = 20,
    KeepAlive = 21,
};

/**
 * @brief Message header (prepended to all messages)
 */
struct MessageHeader {
    MessageType type;
    uint32_t payloadSize;  // Size of data following this header
} __attribute__((packed));

/**
 * @brief Client join request
 */
struct ClientJoinMessage {
    char playerName[32];
    uint32_t clientVersion;
} __attribute__((packed));

/**
 * @brief Player movement update (client -> server)
 */
struct PlayerMoveMessage {
    glm::vec3 position;
    glm::vec3 velocity;
    float yaw;
    float pitch;
    uint8_t inputFlags;  // Bitfield: forward, back, left, right, jump, etc.
} __attribute__((packed));

/**
 * @brief Block placement (client -> server)
 */
struct BlockPlaceMessage {
    int32_t x, y, z;
    uint16_t blockType;
} __attribute__((packed));

/**
 * @brief Block break (client -> server)
 */
struct BlockBreakMessage {
    int32_t x, y, z;
} __attribute__((packed));

/**
 * @brief Chunk data header (server -> client)
 *
 * Followed by compressed chunk data
 */
struct ChunkDataMessage {
    ChunkCoord coord;
    uint32_t compressedSize;
    // Compressed block data follows
} __attribute__((packed));

/**
 * @brief Chunk unload notification (server -> client)
 */
struct ChunkUnloadMessage {
    ChunkCoord coord;
} __attribute__((packed));

/**
 * @brief Single block update (server -> client)
 */
struct BlockUpdateMessage {
    int32_t x, y, z;
    uint16_t blockType;
} __attribute__((packed));

/**
 * @brief Player spawn data (server -> client)
 */
struct PlayerSpawnMessage {
    uint32_t playerId;
    glm::vec3 spawnPosition;
    char playerName[32];
} __attribute__((packed));

/**
 * @brief Player position update (server -> client)
 */
struct PlayerPositionUpdateMessage {
    uint32_t playerId;
    glm::vec3 position;
    float yaw;
    float pitch;
} __attribute__((packed));

/**
 * @brief Keep-alive ping (bidirectional)
 */
struct KeepAliveMessage {
    uint64_t timestamp;
} __attribute__((packed));

} // namespace engine::protocol
