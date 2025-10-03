#pragma once

#include "shared/Chunk.hpp"
#include <vector>
#include <cstdint>

namespace engine {

/**
 * @brief Serializes and compresses chunk data for network transmission
 *
 * Uses run-length encoding (RLE) to compress chunk data efficiently.
 * For a flat stone world, this can compress 32KB down to ~100 bytes.
 */
class ChunkSerializer {
public:
    /**
     * @brief Serialize chunk to compressed byte array
     * @param chunk Chunk to serialize
     * @param outBuffer Output buffer for compressed data
     * @return Size of compressed data in bytes
     */
    static size_t serialize(const Chunk& chunk, std::vector<uint8_t>& outBuffer);

    /**
     * @brief Deserialize chunk from compressed byte array
     * @param buffer Input buffer with compressed data
     * @param size Size of compressed data
     * @param outChunk Output chunk to populate
     * @return true if successful, false if data corrupted
     */
    static bool deserialize(const uint8_t* buffer, size_t size, Chunk& outChunk);

private:
    /**
     * @brief Compress block data using run-length encoding
     *
     * Format: [count:uint16_t][blockType:uint16_t][count][blockType]...
     * Example: 1000 air blocks, 32 stone blocks = [1000][0][32][1]
     */
    static size_t compressRLE(const Block* blocks, size_t count, std::vector<uint8_t>& outBuffer);

    /**
     * @brief Decompress run-length encoded data
     */
    static bool decompressRLE(const uint8_t* buffer, size_t size, Block* outBlocks, size_t maxBlocks);
};

} // namespace engine
