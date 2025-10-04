#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include "shared/ChunkCoord.hpp"
#include "shared/Item.hpp"

// Cross-platform struct packing macros
// Use #pragma pack on all platforms for consistent behavior
#ifdef _MSC_VER
    #define PACK_BEGIN __pragma(pack(push, 1))
    #define PACK_END __pragma(pack(pop))
    #define PACKED
#else
    // GCC/Clang: use both pragma pack and attribute for maximum compatibility
    #define PACK_BEGIN _Pragma("pack(push, 1)")
    #define PACK_END _Pragma("pack(pop)")
    #define PACKED __attribute__((packed))
#endif

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
    InventoryUpdate = 4,

    // Server -> Client
    ChunkData = 10,
    ChunkUnload = 11,
    BlockUpdate = 12,
    PlayerSpawn = 13,
    PlayerPositionUpdate = 14,
    PlayerRemove = 15,
    InventorySync = 16,

    // Bidirectional
    Disconnect = 20,
    KeepAlive = 21,
};

/**
 * @brief Message header (prepended to all messages)
 */
PACK_BEGIN
struct MessageHeader {
    MessageType type;           ///< Type of message being sent
    uint32_t payloadSize;       ///< Size of data following this header in bytes
} PACKED;
PACK_END

/**
 * @brief Client join request
 */
PACK_BEGIN
struct ClientJoinMessage {
    char playerName[32];        ///< Player's chosen display name (null-terminated)
    uint32_t clientVersion;     ///< Client protocol version number
} PACKED;
PACK_END

/**
 * @brief Player movement update (client -> server)
 */
PACK_BEGIN
struct PlayerMoveMessage {
    glm::vec3 position;         ///< Player position in world coordinates
    glm::vec3 velocity;         ///< Player velocity vector
    float yaw;                  ///< Camera yaw angle in degrees
    float pitch;                ///< Camera pitch angle in degrees
    uint8_t inputFlags;         ///< Bitfield: forward, back, left, right, jump, etc.
} PACKED;
PACK_END

/**
 * @brief Block placement (client -> server)
 */
PACK_BEGIN
struct BlockPlaceMessage {
    int32_t x, y, z;            ///< Block world coordinates
    uint16_t blockType;         ///< Type of block to place
} PACKED;
PACK_END

/**
 * @brief Block break (client -> server)
 */
PACK_BEGIN
struct BlockBreakMessage {
    int32_t x, y, z;            ///< Block world coordinates to break
} PACKED;
PACK_END

/**
 * @brief Chunk data header (server -> client)
 *
 * Followed by compressed chunk data
 */
PACK_BEGIN
struct ChunkDataMessage {
    ChunkCoord coord;           ///< Chunk coordinates
    uint32_t compressedSize;    ///< Size of compressed block data that follows in bytes
    // Compressed block data follows
} PACKED;
PACK_END

/**
 * @brief Chunk unload notification (server -> client)
 */
PACK_BEGIN
struct ChunkUnloadMessage {
    ChunkCoord coord;           ///< Chunk coordinates to unload
} PACKED;
PACK_END

/**
 * @brief Single block update (server -> client)
 */
PACK_BEGIN
struct BlockUpdateMessage {
    int32_t x, y, z;            ///< Block world coordinates
    uint16_t blockType;         ///< New block type at this position
} PACKED;
PACK_END

/**
 * @brief Player spawn data (server -> client)
 */
PACK_BEGIN
struct PlayerSpawnMessage {
    uint32_t playerId;          ///< Unique player identifier
    glm::vec3 spawnPosition;    ///< Initial spawn position in world coordinates
    char playerName[32];        ///< Player's display name (null-terminated)
} PACKED;
PACK_END

/**
 * @brief Player position update (server -> client)
 */
PACK_BEGIN
struct PlayerPositionUpdateMessage {
    uint32_t playerId;          ///< Unique player identifier
    glm::vec3 position;         ///< Current player position in world coordinates
    float yaw;                  ///< Camera yaw angle in degrees
    float pitch;                ///< Camera pitch angle in degrees
} PACKED;
PACK_END

/**
 * @brief Player removal notification (server -> client)
 */
PACK_BEGIN
struct PlayerRemoveMessage {
    uint32_t playerId;          ///< Unique player identifier to remove
} PACKED;
PACK_END

/**
 * @brief Keep-alive ping (bidirectional)
 */
PACK_BEGIN
struct KeepAliveMessage {
    uint64_t timestamp;         ///< Unix timestamp in milliseconds for RTT measurement
} PACKED;
PACK_END

/**
 * @brief Inventory sync (server -> client)
 * Sends hotbar inventory and spawn position to client
 */
PACK_BEGIN
struct InventorySyncMessage {
    ItemStack hotbar[9];         ///< Hotbar inventory (9 slots)
    uint32_t selectedHotbarSlot; ///< Currently selected hotbar slot (0-8)
    glm::vec3 position;          ///< Player spawn position
    float yaw;                   ///< Camera yaw angle in degrees
    float pitch;                 ///< Camera pitch angle in degrees
} PACKED;
PACK_END

/**
 * @brief Inventory update (client -> server)
 * Sends updated hotbar inventory to server for persistence
 */
PACK_BEGIN
struct InventoryUpdateMessage {
    ItemStack hotbar[9];         ///< Hotbar inventory (9 slots)
    uint32_t selectedHotbarSlot; ///< Currently selected hotbar slot (0-8)
} PACKED;
PACK_END

} // namespace engine::protocol
