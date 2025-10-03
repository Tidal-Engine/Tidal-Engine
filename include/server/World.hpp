#pragma once

#include "shared/Chunk.hpp"
#include "shared/ChunkCoord.hpp"

#include <unordered_map>
#include <memory>
#include <vector>
#include <mutex>

namespace engine {

/**
 * @brief World manager for the game server
 *
 * Handles chunk storage, generation, and modification.
 * Thread-safe for concurrent access from network and game threads.
 */
class World {
public:
    World();
    ~World() = default;

    // Delete copy and move operations (mutex is not movable)
    World(const World&) = delete;
    World& operator=(const World&) = delete;
    World(World&&) = delete;
    World& operator=(World&&) = delete;

    /**
     * @brief Update the world (called every server tick)
     *
     * Processes chunk updates, block ticks, etc.
     */
    void update();

    /**
     * @brief Get a chunk at the given coordinate
     * @param coord Chunk coordinate
     * @return Pointer to chunk, or nullptr if not loaded
     */
    Chunk* getChunk(const ChunkCoord& coord);
    const Chunk* getChunk(const ChunkCoord& coord) const;

    /**
     * @brief Load or generate a chunk at the given coordinate
     * @param coord Chunk coordinate
     * @return Reference to the loaded/generated chunk
     */
    Chunk& loadChunk(const ChunkCoord& coord);

    /**
     * @brief Unload a chunk (save to disk and remove from memory)
     * @param coord Chunk coordinate
     */
    void unloadChunk(const ChunkCoord& coord);

    /**
     * @brief Get block at world coordinates
     * @param worldX World X coordinate
     * @param worldY World Y coordinate
     * @param worldZ World Z coordinate
     * @return Pointer to block, or nullptr if chunk not loaded
     */
    Block* getBlockAt(int32_t worldX, int32_t worldY, int32_t worldZ);
    const Block* getBlockAt(int32_t worldX, int32_t worldY, int32_t worldZ) const;

    /**
     * @brief Set block at world coordinates
     * @param worldX World X coordinate
     * @param worldY World Y coordinate
     * @param worldZ World Z coordinate
     * @param block Block to set
     * @return true if successful, false if chunk not loaded
     */
    bool setBlockAt(int32_t worldX, int32_t worldY, int32_t worldZ, const Block& block);

    /**
     * @brief Get all loaded chunks (for sending to clients)
     */
    std::vector<const Chunk*> getAllChunks() const;

    /**
     * @brief Get number of loaded chunks
     */
    size_t getLoadedChunkCount() const;

    /**
     * @brief Save all dirty chunks to disk
     * @param worldDir Directory to save world data
     * @return Number of chunks saved
     */
    size_t saveWorld(const std::string& worldDir);

    /**
     * @brief Load world from disk
     * @param worldDir Directory containing world data
     * @return Number of chunks loaded
     */
    size_t loadWorld(const std::string& worldDir);

    /**
     * @brief Generate initial chunks for a new world
     */
    void generateInitialChunks();

private:
    std::unordered_map<ChunkCoord, std::unique_ptr<Chunk>> chunks;
    mutable std::mutex chunksMutex;

    /**
     * @brief Generate a new chunk
     * @param coord Chunk coordinate
     * @return Generated chunk
     */
    std::unique_ptr<Chunk> generateChunk(const ChunkCoord& coord);

    /**
     * @brief Convert world coordinates to chunk coordinate and local position
     */
    static void worldToChunkLocal(int32_t worldX, int32_t worldY, int32_t worldZ,
                                   ChunkCoord& outChunkCoord,
                                   uint32_t& outLocalX, uint32_t& outLocalY, uint32_t& outLocalZ);
};

} // namespace engine
