#pragma once

#include "shared/Block.hpp"
#include "shared/ChunkCoord.hpp"
#include <array>
#include <vector>
#include <cstdint>

namespace engine {

/**
 * @brief Chunk size in each dimension
 */
constexpr uint32_t CHUNK_SIZE = 32;
constexpr uint32_t CHUNK_VOLUME = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE; // 32,768 blocks

/**
 * @brief A 32x32x32 section of the world
 *
 * Chunks are the fundamental unit of world storage and streaming.
 * They contain a 3D array of blocks and their associated data.
 */
class Chunk {
public:
    /**
     * @brief Construct a new chunk at the given coordinate
     * @param coord Chunk coordinate in world space
     */
    explicit Chunk(const ChunkCoord& coord);

    /**
     * @brief Get block at local chunk coordinates
     * @param x Local X coordinate (0-31)
     * @param y Local Y coordinate (0-31)
     * @param z Local Z coordinate (0-31)
     * @return Reference to block at that position
     */
    Block& getBlock(uint32_t x, uint32_t y, uint32_t z);  // NOLINT(readability-identifier-length)
    const Block& getBlock(uint32_t x, uint32_t y, uint32_t z) const;  // NOLINT(readability-identifier-length)

    /**
     * @brief Set block at local chunk coordinates
     * @param x Local X coordinate (0-31)
     * @param y Local Y coordinate (0-31)
     * @param z Local Z coordinate (0-31)
     * @param block Block to set
     */
    void setBlock(uint32_t x, uint32_t y, uint32_t z, const Block& block);  // NOLINT(readability-identifier-length)

    /**
     * @brief Get chunk coordinate in world space
     */
    const ChunkCoord& getCoord() const { return coord; }

    /**
     * @brief Check if chunk has been modified since creation
     */
    bool isDirty() const { return dirty; }

    /**
     * @brief Mark chunk as clean (e.g., after saving to disk)
     */
    void clearDirty() { dirty = false; }

    /**
     * @brief Get raw block data for serialization
     */
    const std::array<Block, CHUNK_VOLUME>& getBlockData() const { return blocks; }

    /**
     * @brief Set raw block data (for deserialization)
     */
    void setBlockData(const std::array<Block, CHUNK_VOLUME>& data);

    /**
     * @brief Serialize chunk to binary data
     * @param outData Output buffer for serialized data
     */
    void serialize(std::vector<uint8_t>& outData) const;

    /**
     * @brief Deserialize chunk from binary data
     * @param data Input buffer containing serialized data
     * @return true if deserialization successful
     */
    bool deserialize(const std::vector<uint8_t>& data);

private:
    ChunkCoord coord;
    std::array<Block, CHUNK_VOLUME> blocks;
    bool dirty = false; // True if chunk has been modified

    /**
     * @brief Convert 3D coordinates to 1D array index
     * @param x Local X coordinate (0-31)
     * @param y Local Y coordinate (0-31)
     * @param z Local Z coordinate (0-31)
     * @return Array index
     */
    static constexpr uint32_t getIndex(uint32_t x, uint32_t y, uint32_t z) {  // NOLINT(readability-identifier-length)
        // Layout: X varies fastest, then Z, then Y
        // This gives better cache locality for horizontal iteration
        return (y * (CHUNK_SIZE * CHUNK_SIZE)) + (z * CHUNK_SIZE) + x;
    }
};

} // namespace engine
