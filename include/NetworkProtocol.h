#pragma once

#include <glm/glm.hpp>
#include <cstdint>
#include <string>
#include <vector>

#ifndef HEADLESS_SERVER
#include "Chunk.h"
#else
// Forward declarations for headless server
struct ChunkPos {
    int x, y, z;
    ChunkPos(int x = 0, int y = 0, int z = 0) : x(x), y(y), z(z) {}
    bool operator==(const ChunkPos& other) const {
        return x == other.x && y == other.y && z == other.z;
    }
};

// Hash function for ChunkPos to use in unordered_map
namespace std {
    template<>
    struct hash<ChunkPos> {
        size_t operator()(const ChunkPos& pos) const {
            return hash<int>()(pos.x) ^ (hash<int>()(pos.y) << 1) ^ (hash<int>()(pos.z) << 2);
        }
    };
}

enum class BlockType : uint8_t {
    AIR = 0, STONE = 1, DIRT = 2, GRASS = 3, WOOD = 4,
    SAND = 5, BRICK = 6, COBBLESTONE = 7, SNOW = 8, COUNT
};
#endif

// Chunk size constant - available for both headless and regular builds
constexpr int CHUNK_SIZE = 32;

// Protocol version for compatibility
constexpr uint32_t NETWORK_PROTOCOL_VERSION = 1;

// Message types for client-server communication
enum class MessageType : uint8_t {
    // Handshake
    CLIENT_HELLO = 0x01,
    SERVER_HELLO = 0x02,

    // Player management
    PLAYER_JOIN = 0x10,
    PLAYER_LEAVE = 0x11,
    PLAYER_MOVE = 0x12,
    PLAYER_UPDATE = 0x13,

    // World interaction
    BLOCK_PLACE = 0x20,
    BLOCK_BREAK = 0x21,
    BLOCK_UPDATE = 0x22,

    // World data
    CHUNK_REQUEST = 0x30,
    CHUNK_DATA = 0x31,
    WORLD_TIME = 0x32,

    // Chat/communication
    CHAT_MESSAGE = 0x40,

    // System
    PING = 0xF0,
    PONG = 0xF1,
    DISCONNECT = 0xF2,
    ERROR_MESSAGE = 0xFF
};

// Player ID type
using PlayerId = uint32_t;
constexpr PlayerId INVALID_PLAYER_ID = 0;

// Network message header
struct MessageHeader {
    MessageType type;
    uint32_t size;      // Size of payload following header
    uint32_t sequence;  // For packet ordering/reliability
};

// Message structures
struct ClientHelloMessage {
    uint32_t protocolVersion;
    char playerName[32];
};

struct ServerHelloMessage {
    uint32_t protocolVersion;
    PlayerId playerId;
    glm::vec3 spawnPosition;
    bool accepted;
    char reason[128];  // If not accepted, reason why
};

struct PlayerMoveMessage {
    PlayerId playerId;
    glm::vec3 position;
    float yaw;
    float pitch;
    uint32_t timestamp;
};

struct PlayerUpdateMessage {
    PlayerId playerId;
    glm::vec3 position;
    float yaw;
    float pitch;
    uint8_t flags;  // Bit flags for additional state
};

struct BlockPlaceMessage {
    glm::ivec3 position;
    BlockType blockType;
    uint32_t timestamp;
};

struct BlockBreakMessage {
    glm::ivec3 position;
    uint32_t timestamp;
};

struct BlockUpdateMessage {
    glm::ivec3 position;
    BlockType blockType;
    PlayerId causedBy;  // Which player caused this change
};

struct ChunkRequestMessage {
    ChunkPos chunkPos;
};

struct ChunkDataMessage {
    ChunkPos chunkPos;
    bool isEmpty;
    // Followed by chunk voxel data if not empty
    // BlockType voxels[CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE];
};

struct ChatMessage {
    PlayerId playerId;
    char message[256];
    uint32_t timestamp;
};

struct PingMessage {
    uint32_t timestamp;
};

struct DisconnectMessage {
    char reason[128];
};

struct ErrorMessage {
    uint32_t errorCode;
    char description[256];
};

// Error codes
enum class NetworkError : uint32_t {
    NONE = 0,
    PROTOCOL_MISMATCH = 1,
    SERVER_FULL = 2,
    INVALID_PLAYER_NAME = 3,
    TIMEOUT = 4,
    INVALID_MESSAGE = 5,
    PERMISSION_DENIED = 6,
    WORLD_LOAD_FAILED = 7
};

// Network packet for transmission
struct NetworkPacket {
    MessageHeader header;
    std::vector<uint8_t> payload;

    NetworkPacket() = default;
    NetworkPacket(MessageType type, const void* data, size_t size);

    // Serialize packet for transmission
    std::vector<uint8_t> serialize() const;

    // Deserialize packet from received data
    static bool deserialize(const uint8_t* data, size_t size, NetworkPacket& packet);
};

// Player state on server
struct PlayerState {
    PlayerId id;
    std::string name;
    glm::vec3 position;
    float yaw;
    float pitch;
    uint32_t lastUpdate;
    bool connected;

    // Future: inventory, health, etc.
};

// Game event for internal server processing
struct GameEvent {
    enum Type {
        PLAYER_JOINED,
        PLAYER_LEFT,
        PLAYER_MOVED,
        BLOCK_PLACED,
        BLOCK_BROKEN,
        CHAT_SENT
    } type;

    PlayerId playerId;
    uint32_t timestamp;

    // Event-specific data
    struct PlayerMoveData { glm::vec3 position; float yaw; float pitch; };
    struct BlockPlaceData { glm::ivec3 position; BlockType blockType; };
    struct BlockBreakData { glm::ivec3 position; };
    struct ChatData { char message[256]; };

    union {
        PlayerMoveData playerMove;
        BlockPlaceData blockPlace;
        BlockBreakData blockBreak;
        ChatData chat;
    };
};