/**
 * @file SaveSystem.h
 * @brief World data persistence and serialization system
 * @author Tidal Engine Team
 * @version 1.0
 */

#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <vector>
#include <cstdint>

#ifndef HEADLESS_SERVER
#include "game/Chunk.h"
#include "core/Camera.h"
#else
// Forward declarations for headless server
class Camera;
class Chunk;
#include <glm/glm.hpp>
#endif

// Include NetworkProtocol for shared types like ChunkPos and BlockType
#include "network/NetworkProtocol.h"

// Forward declarations
class ChunkManager;

/**
 * @brief Save file version for backward compatibility
 */
constexpr uint32_t SAVE_VERSION = 1;

/**
 * @brief Player state data for persistence
 */
struct PlayerData {
    glm::vec3 position;         ///< Player position in world space
    float yaw;                  ///< Camera yaw angle in degrees
    float pitch;                ///< Camera pitch angle in degrees
    float movementSpeed;        ///< Player movement speed
    float mouseSensitivity;     ///< Mouse sensitivity multiplier
    float zoom;                 ///< Camera field of view
    int selectedHotbarSlot;     ///< Currently selected hotbar slot
    // TODO: Add inventory data in future versions
};

/**
 * @brief World metadata and configuration
 */
struct WorldData {
    std::string name;               ///< World display name
    uint64_t creationTime;          ///< World creation timestamp
    uint64_t lastPlayTime;          ///< Last time world was played
    uint32_t seed;                  ///< World generation seed
    std::string activeTexturepack;  ///< Active texture pack name
};

/**
 * @brief Raw chunk data for serialization
 */
struct ChunkData {
    ChunkPos position;      ///< Chunk position in world coordinates
#ifndef HEADLESS_SERVER
    BlockType voxels[Chunk::SIZE][Chunk::SIZE][Chunk::SIZE];    ///< Voxel data array
#else
    BlockType voxels[CHUNK_SIZE][CHUNK_SIZE][CHUNK_SIZE];       ///< Voxel data array (headless)
#endif
    /**
     * @brief Check if chunk contains only air blocks
     * @return True if chunk is empty
     */
    bool isEmpty() const;
};

/**
 * @brief Comprehensive save system for world data persistence
 *
 * The SaveSystem manages all aspects of world data storage including:
 * - Player data (position, camera, settings)
 * - World metadata (name, creation time, seed)
 * - Chunk data (voxel arrays, compressed storage)
 * - Multi-world support with separate save directories
 * - Cross-platform file I/O with error handling
 *
 * Features:
 * - Binary serialization for efficiency
 * - Version compatibility for save format evolution
 * - Atomic writes to prevent data corruption
 * - Directory structure management
 *
 * @see PlayerData for player state structure
 * @see WorldData for world metadata structure
 * @see ChunkData for chunk serialization
 */
class SaveSystem {
public:
    /**
     * @brief Construct save system with saves directory
     * @param savesDirectory Root directory for all world saves
     */
    SaveSystem(const std::string& savesDirectory = "saves");
    /**
     * @brief Default destructor
     */
    ~SaveSystem() = default;

    /// @name World Management
    /// @{
    /**
     * @brief Create new world with directory structure
     * @param worldName Name of the world to create
     * @return True if world created successfully
     */
    bool createWorld(const std::string& worldName);

    /**
     * @brief Load existing world and set as current
     * @param worldName Name of world to load
     * @return True if world loaded successfully
     */
    bool loadWorld(const std::string& worldName);

    /**
     * @brief Save current world data to disk
     * @return True if save completed successfully
     */
    bool saveWorld();

    /**
     * @brief Delete world and all associated data
     * @param worldName Name of world to delete
     * @return True if deletion completed successfully
     */
    bool deleteWorld(const std::string& worldName);

    /**
     * @brief Get list of all available world names
     * @return Vector of world names found in saves directory
     */
    std::vector<std::string> getAvailableWorlds() const;
    /// @}

    /// @name Player Data Persistence
    /// @{
    /**
     * @brief Save player state to current world
     * @param playerData Player data to serialize and save
     * @return True if save completed successfully
     */
    bool savePlayerData(const PlayerData& playerData);

    /**
     * @brief Load player state from current world
     * @param playerData Reference to populate with loaded data
     * @return True if load completed successfully
     */
    bool loadPlayerData(PlayerData& playerData);
    /// @}

    /// @name Chunk Data Persistence
    /// @{
    /**
     * @brief Save chunk data to disk with compression
     * @param chunkData Chunk data to serialize and save
     * @return True if save completed successfully
     */
    bool saveChunk(const ChunkData& chunkData);

    /**
     * @brief Load chunk data from disk
     * @param position Chunk position coordinates
     * @param chunkData Reference to populate with loaded data
     * @return True if load completed successfully
     */
    bool loadChunk(const ChunkPos& position, ChunkData& chunkData);

    /**
     * @brief Check if chunk save file exists
     * @param position Chunk position to check
     * @return True if chunk has saved data
     */
    bool chunkExists(const ChunkPos& position) const;
    /// @}

    /// @name World Metadata Persistence
    /// @{
    /**
     * @brief Save world metadata to disk
     * @param worldData World metadata to save
     * @return True if save completed successfully
     */
    bool saveWorldData(const WorldData& worldData);

    /**
     * @brief Load world metadata from disk
     * @param worldData Reference to populate with loaded metadata
     * @return True if load completed successfully
     */
    bool loadWorldData(WorldData& worldData);
    /// @}

    /// @name Utility Methods
    /// @{
    /**
     * @brief Set the currently active world
     * @param worldName Name of world to set as current
     */
    void setCurrentWorld(const std::string& worldName) { m_currentWorld = worldName; }

    /**
     * @brief Get the currently active world name
     * @return Current world name
     */
    const std::string& getCurrentWorld() const { return m_currentWorld; }

    /**
     * @brief Get full path to current world directory
     * @return Absolute path to world directory
     */
    std::string getWorldPath() const;

    /**
     * @brief Get full path to specific chunk save file
     * @param position Chunk position coordinates
     * @return Absolute path to chunk save file
     */
    std::string getChunkPath(const ChunkPos& position) const;
    /// @}

    /// @name High-Level Integration Helpers
    /// @{
    /**
     * @brief Save player data directly from camera state
     * @param camera Camera object containing position and orientation
     * @param selectedHotbarSlot Currently selected hotbar slot
     */
    void savePlayerFromCamera(const Camera& camera, int selectedHotbarSlot);

    /**
     * @brief Load player data directly to camera state
     * @param camera Camera object to update with loaded data
     * @param selectedHotbarSlot Reference to store loaded hotbar slot
     */
    void loadPlayerToCamera(Camera& camera, int& selectedHotbarSlot);

    /**
     * @brief Save chunk data directly from Chunk object
     * @param chunk Chunk object containing voxel data
     */
    void saveChunkFromChunk(const Chunk& chunk);

    /**
     * @brief Load chunk data directly to Chunk object
     * @param chunk Chunk object to populate with loaded data
     * @return True if load completed successfully
     */
    bool loadChunkToChunk(Chunk& chunk);
    /// @}

private:
    std::string m_savesDirectory;   ///< Root directory for all saves
    std::string m_currentWorld;     ///< Currently active world name

    /// @name File I/O Helpers
    /// @{
    /**
     * @brief Create directory with proper permissions
     * @param path Directory path to create
     * @return True if directory created or already exists
     */
    bool createDirectory(const std::string& path) const;

    /**
     * @brief Check if file exists on filesystem
     * @param path File path to check
     * @return True if file exists and is readable
     */
    bool fileExists(const std::string& path) const;

    /**
     * @brief Write binary data to file atomically
     * @param path File path to write to
     * @param data Pointer to data buffer
     * @param size Size of data in bytes
     * @return True if write completed successfully
     */
    bool writeDataToFile(const std::string& path, const void* data, size_t size);

    /**
     * @brief Read binary data from file
     * @param path File path to read from
     * @param data Pointer to buffer for data
     * @param size Expected size of data in bytes
     * @return True if read completed successfully
     */
    bool readDataFromFile(const std::string& path, void* data, size_t size);
    /// @}

    /// @name Serialization Helpers
    /// @{
    /**
     * @brief Serialize player data to binary buffer
     * @param data Player data to serialize
     * @param buffer Buffer to write serialized data
     */
    void serializePlayerData(const PlayerData& data, std::vector<uint8_t>& buffer);

    /**
     * @brief Deserialize player data from binary buffer
     * @param buffer Buffer containing serialized data
     * @param data Reference to populate with deserialized data
     * @return True if deserialization successful
     */
    bool deserializePlayerData(const std::vector<uint8_t>& buffer, PlayerData& data);

    /**
     * @brief Serialize chunk data to binary buffer with compression
     * @param data Chunk data to serialize
     * @param buffer Buffer to write serialized data
     */
    void serializeChunkData(const ChunkData& data, std::vector<uint8_t>& buffer);

    /**
     * @brief Deserialize chunk data from binary buffer
     * @param buffer Buffer containing serialized data
     * @param data Reference to populate with deserialized data
     * @return True if deserialization successful
     */
    bool deserializeChunkData(const std::vector<uint8_t>& buffer, ChunkData& data);

    /**
     * @brief Serialize world metadata to binary buffer
     * @param data World data to serialize
     * @param buffer Buffer to write serialized data
     */
    void serializeWorldData(const WorldData& data, std::vector<uint8_t>& buffer);

    /**
     * @brief Deserialize world metadata from binary buffer
     * @param buffer Buffer containing serialized data
     * @param data Reference to populate with deserialized data
     * @return True if deserialization successful
     */
    bool deserializeWorldData(const std::vector<uint8_t>& buffer, WorldData& data);
    /// @}
};