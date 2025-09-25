#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <vector>
#include <cstdint>

#ifndef HEADLESS_SERVER
#include "Chunk.h"
#include "Camera.h"
#else
// Forward declarations for headless server
class Camera;
class Chunk;
#include <glm/glm.hpp>
#endif

// Include NetworkProtocol for shared types like ChunkPos and BlockType
#include "NetworkProtocol.h"

// Forward declarations
class ChunkManager;

// Save file version for compatibility
constexpr uint32_t SAVE_VERSION = 1;

// Player save data
struct PlayerData {
    glm::vec3 position;
    float yaw;
    float pitch;
    float movementSpeed;
    float mouseSensitivity;
    float zoom;
    int selectedHotbarSlot;
    // Future: inventory data
};

// World metadata
struct WorldData {
    std::string name;
    uint64_t creationTime;
    uint64_t lastPlayTime;
    uint32_t seed;  // For future procedural generation
    std::string activeTexturepack;
};

// Chunk save data (raw voxel data only)
struct ChunkData {
    ChunkPos position;
#ifndef HEADLESS_SERVER
    BlockType voxels[Chunk::SIZE][Chunk::SIZE][Chunk::SIZE];
#else
    BlockType voxels[CHUNK_SIZE][CHUNK_SIZE][CHUNK_SIZE];
#endif
    bool isEmpty() const;
};

class SaveSystem {
public:
    SaveSystem(const std::string& savesDirectory = "saves");
    ~SaveSystem() = default;

    // World management
    bool createWorld(const std::string& worldName);
    bool loadWorld(const std::string& worldName);
    bool saveWorld();
    bool deleteWorld(const std::string& worldName);
    std::vector<std::string> getAvailableWorlds() const;

    // Data saving/loading
    bool savePlayerData(const PlayerData& playerData);
    bool loadPlayerData(PlayerData& playerData);

    bool saveChunk(const ChunkData& chunkData);
    bool loadChunk(const ChunkPos& position, ChunkData& chunkData);
    bool chunkExists(const ChunkPos& position) const;

    bool saveWorldData(const WorldData& worldData);
    bool loadWorldData(WorldData& worldData);

    // Utility
    void setCurrentWorld(const std::string& worldName) { m_currentWorld = worldName; }
    const std::string& getCurrentWorld() const { return m_currentWorld; }
    std::string getWorldPath() const;
    std::string getChunkPath(const ChunkPos& position) const;

    // High-level integration helpers
    void savePlayerFromCamera(const Camera& camera, int selectedHotbarSlot);
    void loadPlayerToCamera(Camera& camera, int& selectedHotbarSlot);
    void saveChunkFromChunk(const Chunk& chunk);
    bool loadChunkToChunk(Chunk& chunk);

private:
    std::string m_savesDirectory;
    std::string m_currentWorld;

    // File I/O helpers
    bool createDirectory(const std::string& path) const;
    bool fileExists(const std::string& path) const;
    bool writeDataToFile(const std::string& path, const void* data, size_t size);
    bool readDataFromFile(const std::string& path, void* data, size_t size);

    // Serialization helpers
    void serializePlayerData(const PlayerData& data, std::vector<uint8_t>& buffer);
    bool deserializePlayerData(const std::vector<uint8_t>& buffer, PlayerData& data);
    void serializeChunkData(const ChunkData& data, std::vector<uint8_t>& buffer);
    bool deserializeChunkData(const std::vector<uint8_t>& buffer, ChunkData& data);
    void serializeWorldData(const WorldData& data, std::vector<uint8_t>& buffer);
    bool deserializeWorldData(const std::vector<uint8_t>& buffer, WorldData& data);
};