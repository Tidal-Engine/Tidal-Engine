/**
 * @file NetworkProtocol.h
 * @brief Network protocol definitions and message structures for multiplayer communication
 * @author Tidal Engine Team
 * @version 1.0
 */

#pragma once

#include <glm/glm.hpp>
#include <cstdint>
#include <string>
#include <vector>

#ifndef HEADLESS_SERVER
#include "game/Chunk.h"
#else
/**
 * @brief 3D chunk position for headless server builds
 *
 * Represents the position of a chunk in the 3D world grid.
 * Used when full game headers are not available in headless server mode.
 */
struct ChunkPos {
    int x, y, z;    ///< Chunk coordinates in world space

    /**
     * @brief Default constructor - creates chunk at origin
     * @param x Chunk X coordinate (default: 0)
     * @param y Chunk Y coordinate (default: 0)
     * @param z Chunk Z coordinate (default: 0)
     */
    ChunkPos(int x = 0, int y = 0, int z = 0) : x(x), y(y), z(z) {}

    /**
     * @brief Equality comparison for chunk positions
     * @param other Other chunk position to compare
     * @return True if all coordinates match
     */
    bool operator==(const ChunkPos& other) const {
        return x == other.x && y == other.y && z == other.z;
    }
};

/**
 * @brief Hash function specialization for ChunkPos
 *
 * Enables ChunkPos to be used as key in std::unordered_map and std::unordered_set.
 * Uses coordinate bit-shifting for hash distribution.
 */
namespace std {
    template<>
    struct hash<ChunkPos> {
        /**
         * @brief Generate hash value for chunk position
         * @param pos Chunk position to hash
         * @return Hash value combining all three coordinates
         */
        size_t operator()(const ChunkPos& pos) const {
            return hash<int>()(pos.x) ^ (hash<int>()(pos.y) << 1) ^ (hash<int>()(pos.z) << 2);
        }
    };
}

/**
 * @brief Block types available in the voxel world
 *
 * Enumeration of all block types that can exist in the game world.
 * Used for headless server builds when full game headers are unavailable.
 */
enum class BlockType : uint8_t {
    AIR = 0,        ///< Empty space (transparent)
    STONE = 1,      ///< Solid stone block
    DIRT = 2,       ///< Dirt block
    GRASS = 3,      ///< Grass-covered dirt block
    WOOD = 4,       ///< Wooden block
    SAND = 5,       ///< Sand block
    BRICK = 6,      ///< Brick block
    COBBLESTONE = 7, ///< Cobblestone block
    SNOW = 8,       ///< Snow block
    COUNT           ///< Total number of block types (for iteration)
};
#endif

/// @defgroup NetworkConstants Network Protocol Constants
/// @{
constexpr int CHUNK_SIZE = 32;                      ///< Voxels per chunk edge (32x32x32)
constexpr uint32_t NETWORK_PROTOCOL_VERSION = 1;    ///< Current protocol version for compatibility
/// @}

/**
 * @brief Message types for client-server communication protocol
 *
 * Defines all message types that can be exchanged between client and server.
 * Message IDs are grouped logically and use specific byte ranges for organization.
 */
enum class MessageType : uint8_t {
    /// @name Handshake Messages (0x01-0x0F)
    /// @{
    CLIENT_HELLO = 0x01,    ///< Client requests connection to server
    SERVER_HELLO = 0x02,    ///< Server responds with connection acceptance/rejection
    /// @}

    /// @name Player Management (0x10-0x1F)
    /// @{
    PLAYER_JOIN = 0x10,     ///< Player joins the game world
    PLAYER_LEAVE = 0x11,    ///< Player leaves the game world
    PLAYER_MOVE = 0x12,     ///< Player position/rotation update
    PLAYER_UPDATE = 0x13,   ///< General player state update
    /// @}

    /// @name World Interaction (0x20-0x2F)
    /// @{
    BLOCK_PLACE = 0x20,     ///< Player places a block
    BLOCK_BREAK = 0x21,     ///< Player breaks a block
    BLOCK_UPDATE = 0x22,    ///< Server notifies of block change
    /// @}

    /// @name World Data (0x30-0x3F)
    /// @{
    CHUNK_REQUEST = 0x30,   ///< Client requests chunk data
    CHUNK_DATA = 0x31,      ///< Server sends chunk voxel data
    WORLD_TIME = 0x32,      ///< Server broadcasts world time update
    /// @}

    /// @name Communication (0x40-0x4F)
    /// @{
    CHAT_MESSAGE = 0x40,    ///< Chat message between players
    /// @}

    /// @name System Messages (0xF0-0xFF)
    /// @{
    PING = 0xF0,            ///< Connection keep-alive ping
    PONG = 0xF1,            ///< Response to ping message
    DISCONNECT = 0xF2,      ///< Graceful disconnect notification
    ERROR_MESSAGE = 0xFF    ///< Error condition notification
    /// @}
};

/// @name Player Identification
/// @{
using PlayerId = uint32_t;                          ///< Unique player identifier type
constexpr PlayerId INVALID_PLAYER_ID = 0;           ///< Invalid/unassigned player ID value
/// @}

/**
 * @brief Network message header for all protocol messages
 *
 * Standard header prepended to all network messages for routing and integrity.
 * Provides message identification, payload size, and sequence tracking.
 */
struct MessageHeader {
    MessageType type;       ///< Message type identifier
    uint32_t size;          ///< Size of payload data following this header
    uint32_t sequence;      ///< Packet sequence number for ordering/reliability
};

/// @defgroup MessageStructures Network Message Payload Structures
/// @{

/**
 * @brief Client connection request message
 *
 * Sent by client to initiate connection handshake with server.
 * Contains protocol compatibility information and player identification.
 */
struct ClientHelloMessage {
    uint32_t protocolVersion;   ///< Client protocol version for compatibility check
    char playerName[32];        ///< Desired player name (null-terminated)
};

/**
 * @brief Server response to client connection request
 *
 * Server's response to ClientHelloMessage containing connection acceptance
 * status, assigned player ID, and spawn location if accepted.
 */
struct ServerHelloMessage {
    uint32_t protocolVersion;   ///< Server protocol version
    PlayerId playerId;          ///< Assigned player ID (if accepted)
    glm::vec3 spawnPosition;    ///< Initial spawn position in world
    bool accepted;              ///< True if connection accepted
    char reason[128];           ///< Rejection reason if not accepted (null-terminated)
};

/**
 * @brief Player movement and rotation update
 *
 * Sent by client to update player position and camera orientation.
 * Server broadcasts to other clients for position synchronization.
 */
struct PlayerMoveMessage {
    PlayerId playerId;          ///< ID of moving player
    glm::vec3 position;         ///< New world position
    float yaw;                  ///< Camera yaw rotation in degrees
    float pitch;                ///< Camera pitch rotation in degrees
    uint32_t timestamp;         ///< Client timestamp for lag compensation
};

/**
 * @brief General player state update message
 *
 * Server-sent message to synchronize player state across all clients.
 * Contains position, orientation, and additional state flags.
 */
struct PlayerUpdateMessage {
    PlayerId playerId;          ///< ID of updated player
    glm::vec3 position;         ///< Current world position
    float yaw;                  ///< Camera yaw rotation in degrees
    float pitch;                ///< Camera pitch rotation in degrees
    uint8_t flags;              ///< Bit flags for additional state (running, crouching, etc.)
};

/**
 * @brief Block placement request/notification
 *
 * Sent by client to request block placement, or by server to notify
 * all clients of successful block placement by another player.
 */
struct BlockPlaceMessage {
    glm::ivec3 position;        ///< World position where block is placed
    BlockType blockType;        ///< Type of block being placed
    uint32_t timestamp;         ///< Timestamp for event ordering
};

/**
 * @brief Block destruction request/notification
 *
 * Sent by client to request block destruction, or by server to notify
 * all clients of successful block removal by another player.
 */
struct BlockBreakMessage {
    glm::ivec3 position;        ///< World position of block to break
    uint32_t timestamp;         ///< Timestamp for event ordering
};

/**
 * @brief Server notification of block state change
 *
 * Authoritative server message informing clients of block changes.
 * Used to maintain world consistency across all connected players.
 */
struct BlockUpdateMessage {
    glm::ivec3 position;        ///< World position of changed block
    BlockType blockType;        ///< New block type (AIR for removal)
    PlayerId causedBy;          ///< Player ID who initiated this change
};

/**
 * @brief Client request for chunk data
 *
 * Sent by client to request voxel data for a specific chunk.
 * Server responds with ChunkDataMessage containing the chunk contents.
 */
struct ChunkRequestMessage {
    ChunkPos chunkPos;          ///< Position of requested chunk
};

/**
 * @brief Server response containing chunk voxel data
 *
 * Contains complete voxel data for a requested chunk. If chunk is not empty,
 * the message is followed by CHUNK_SIZE^3 BlockType values representing
 * each voxel in the chunk.
 */
struct ChunkDataMessage {
    ChunkPos chunkPos;          ///< Position of chunk being sent
    bool isEmpty;               ///< True if chunk contains only air blocks
    // Followed by chunk voxel data if not empty:
    // BlockType voxels[CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE];
};

/**
 * @brief Chat message between players
 *
 * Carries text communication between players. Sent by client to server,
 * then broadcast by server to all connected clients.
 */
struct ChatMessage {
    PlayerId playerId;          ///< ID of player who sent message
    char message[256];          ///< Chat message text (null-terminated)
    uint32_t timestamp;         ///< Server timestamp when message was received
};

/**
 * @brief Connection keep-alive ping message
 *
 * Sent periodically to maintain connection and measure round-trip time.
 * Recipient should respond with PongMessage containing same timestamp.
 */
struct PingMessage {
    uint32_t timestamp;         ///< Timestamp when ping was sent
};

/**
 * @brief Graceful disconnection notification
 *
 * Sent by either client or server to notify of intentional disconnection.
 * Allows cleanup and proper connection termination.
 */
struct DisconnectMessage {
    char reason[128];           ///< Human-readable disconnection reason (null-terminated)
};

/**
 * @brief Error condition notification
 *
 * Sent by server to notify client of error conditions that don't
 * require disconnection but need client awareness.
 */
struct ErrorMessage {
    uint32_t errorCode;         ///< Numeric error code from NetworkError enum
    char description[256];      ///< Human-readable error description (null-terminated)
};
/// @}

/**
 * @brief Network error codes for error reporting
 *
 * Standardized error codes sent in ErrorMessage to indicate
 * specific failure conditions that clients should handle.
 */
enum class NetworkError : uint32_t {
    NONE = 0,                   ///< No error (success)
    PROTOCOL_MISMATCH = 1,      ///< Client/server protocol version mismatch
    SERVER_FULL = 2,            ///< Server has reached maximum player capacity
    INVALID_PLAYER_NAME = 3,    ///< Player name contains invalid characters or is taken
    TIMEOUT = 4,                ///< Operation timed out (connection or request)
    INVALID_MESSAGE = 5,        ///< Received message was malformed or unexpected
    PERMISSION_DENIED = 6,      ///< Client lacks permission for requested action
    WORLD_LOAD_FAILED = 7       ///< Server failed to load world data
};

/**
 * @brief Complete network packet for transmission
 *
 * Container for network messages including header and payload data.
 * Provides serialization/deserialization for wire protocol transmission
 * over TCP sockets.
 *
 * @note Thread-safe for read operations, synchronization required for writes
 */
struct NetworkPacket {
    MessageHeader header;           ///< Message header with type and size information
    std::vector<uint8_t> payload;   ///< Message payload data

    /**
     * @brief Default constructor - creates empty packet
     */
    NetworkPacket() = default;

    /**
     * @brief Construct packet with message data
     * @param type Message type for header
     * @param data Pointer to payload data
     * @param size Size of payload data in bytes
     */
    NetworkPacket(MessageType type, const void* data, size_t size);

    /**
     * @brief Serialize packet for network transmission
     * @return Byte vector containing complete packet data
     * @note Result can be sent directly over TCP socket
     */
    std::vector<uint8_t> serialize() const;

    /**
     * @brief Deserialize packet from received network data
     * @param data Pointer to received byte data
     * @param size Size of received data in bytes
     * @param packet Reference to packet object to populate
     * @return True if deserialization succeeded
     * @note Validates header and payload size for safety
     */
    static bool deserialize(const uint8_t* data, size_t size, NetworkPacket& packet);
};

/**
 * @brief Complete player state tracked by server
 *
 * Server-side representation of player state including position,
 * orientation, connection status, and metadata. Used for game logic
 * and synchronization across clients.
 *
 * @note This is the authoritative player state - client states may differ
 */
struct PlayerState {
    PlayerId id;                ///< Unique player identifier
    std::string name;           ///< Player display name
    glm::vec3 position;         ///< Current world position
    float yaw;                  ///< Camera yaw rotation in degrees
    float pitch;                ///< Camera pitch rotation in degrees
    uint32_t lastUpdate;        ///< Timestamp of last state update
    bool connected;             ///< True if player is currently connected

    // Future expansion: inventory, health, game mode, etc.
};

/**
 * @brief Game event for internal server processing
 *
 * Represents discrete game events that need to be processed by server logic.
 * Used for event-driven game state updates and inter-system communication
 * within the server.
 *
 * @note Uses union for memory efficiency - only one event type data is valid
 */
struct GameEvent {
    /**
     * @brief Types of game events that can occur
     */
    enum Type {
        PLAYER_JOINED,      ///< New player connected to server
        PLAYER_LEFT,        ///< Player disconnected from server
        PLAYER_MOVED,       ///< Player changed position/orientation
        BLOCK_PLACED,       ///< Player placed a block in world
        BLOCK_BROKEN,       ///< Player broke a block from world
        CHAT_SENT          ///< Player sent chat message
    } type;                 ///< Type of event that occurred

    PlayerId playerId;      ///< ID of player who triggered event
    uint32_t timestamp;     ///< Server timestamp when event occurred

    /// @name Event-specific Data Structures
    /// @{
    /**
     * @brief Player movement event data
     */
    struct PlayerMoveData {
        glm::vec3 position; ///< New player position
        float yaw;          ///< New camera yaw
        float pitch;        ///< New camera pitch
    };

    /**
     * @brief Block placement event data
     */
    struct BlockPlaceData {
        glm::ivec3 position; ///< World position where block was placed
        BlockType blockType; ///< Type of block that was placed
    };

    /**
     * @brief Block destruction event data
     */
    struct BlockBreakData {
        glm::ivec3 position; ///< World position of broken block
    };

    /**
     * @brief Chat message event data
     */
    struct ChatData {
        char message[256];   ///< Chat message text (null-terminated)
    };
    /// @}

    /**
     * @brief Event-specific data (only one member is valid based on type)
     */
    union {
        PlayerMoveData playerMove;  ///< Valid when type == PLAYER_MOVED
        BlockPlaceData blockPlace;  ///< Valid when type == BLOCK_PLACED
        BlockBreakData blockBreak;  ///< Valid when type == BLOCK_BROKEN
        ChatData chat;              ///< Valid when type == CHAT_SENT
    };
};