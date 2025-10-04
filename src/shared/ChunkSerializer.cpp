#include "shared/ChunkSerializer.hpp"
#include "core/Logger.hpp"
#include <cstring>

namespace engine {

size_t ChunkSerializer::serialize(const Chunk& chunk, std::vector<uint8_t>& outBuffer) {
    outBuffer.clear();

    // Get raw block data
    const auto& blockData = chunk.getBlockData();

    // Compress using RLE
    size_t compressedSize = compressRLE(blockData.data(), CHUNK_VOLUME, outBuffer);

    LOG_TRACE("Serialized chunk ({}, {}, {}) | Original: {} bytes | Compressed: {} bytes | Ratio: {:.1f}%",
              chunk.getCoord().x, chunk.getCoord().y, chunk.getCoord().z,
              CHUNK_VOLUME * sizeof(Block), compressedSize,
              (compressedSize * 100.0f) / (CHUNK_VOLUME * sizeof(Block)));

    return compressedSize;
}

bool ChunkSerializer::deserialize(const uint8_t* buffer, size_t size, Chunk& outChunk) {
    std::array<Block, CHUNK_VOLUME> blocks;

    // Decompress RLE data
    if (!decompressRLE(buffer, size, blocks.data(), CHUNK_VOLUME)) {
        LOG_ERROR("Failed to decompress chunk data");
        return false;
    }

    // Set block data
    outChunk.setBlockData(blocks);

    return true;
}

size_t ChunkSerializer::compressRLE(const Block* blocks, size_t count, std::vector<uint8_t>& outBuffer) {
    outBuffer.reserve(count * sizeof(Block) / 4); // Estimate 4:1 compression

    size_t idx = 0;
    while (idx < count) {
        BlockType currentType = blocks[idx].type;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        uint16_t runLength = 1;

        // Count consecutive blocks of same type
        while (idx + runLength < count &&
               blocks[idx + runLength].type == currentType &&  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
               runLength < UINT16_MAX) {
            runLength++;
        }

        // Write [runLength:uint16_t][blockType:uint16_t]
        uint16_t length = runLength;
        uint16_t type = static_cast<uint16_t>(currentType);

        outBuffer.insert(outBuffer.end(),
                        reinterpret_cast<uint8_t*>(&length),  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                        reinterpret_cast<uint8_t*>(&length) + sizeof(uint16_t));  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast, cppcoreguidelines-pro-bounds-pointer-arithmetic)
        outBuffer.insert(outBuffer.end(),
                        reinterpret_cast<uint8_t*>(&type),  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                        reinterpret_cast<uint8_t*>(&type) + sizeof(uint16_t));  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast, cppcoreguidelines-pro-bounds-pointer-arithmetic)

        idx += runLength;
    }

    return outBuffer.size();
}

bool ChunkSerializer::decompressRLE(const uint8_t* buffer, size_t size, Block* outBlocks, size_t maxBlocks) {
    size_t bufferPos = 0;
    size_t blockPos = 0;

    while (bufferPos < size) {
        // Read runLength
        if (bufferPos + sizeof(uint16_t) > size) {
            LOG_ERROR("Corrupted RLE data: unexpected end while reading run length");
            return false;
        }
        uint16_t runLength = 0;
        std::memcpy(&runLength, buffer + bufferPos, sizeof(uint16_t));  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        bufferPos += sizeof(uint16_t);

        // Read blockType
        if (bufferPos + sizeof(uint16_t) > size) {
            LOG_ERROR("Corrupted RLE data: unexpected end while reading block type");
            return false;
        }
        uint16_t blockType = 0;
        std::memcpy(&blockType, buffer + bufferPos, sizeof(uint16_t));  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        bufferPos += sizeof(uint16_t);

        // Validate we won't overflow output
        if (blockPos + runLength > maxBlocks) {
            LOG_ERROR("Corrupted RLE data: run would overflow output buffer");
            return false;
        }

        // Write blocks
        Block block;
        block.type = static_cast<BlockType>(blockType);
        for (uint16_t idx = 0; idx < runLength; idx++) {
            outBlocks[blockPos++] = block;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        }
    }

    // Verify we filled exactly the expected number of blocks
    if (blockPos != maxBlocks) {
        LOG_ERROR("Corrupted RLE data: decompressed {} blocks, expected {}", blockPos, maxBlocks);
        return false;
    }

    return true;
}

} // namespace engine
