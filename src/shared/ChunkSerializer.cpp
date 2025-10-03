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

    size_t i = 0;
    while (i < count) {
        BlockType currentType = blocks[i].type;
        uint16_t runLength = 1;

        // Count consecutive blocks of same type
        while (i + runLength < count &&
               blocks[i + runLength].type == currentType &&
               runLength < UINT16_MAX) {
            runLength++;
        }

        // Write [runLength:uint16_t][blockType:uint16_t]
        uint16_t length = runLength;
        uint16_t type = static_cast<uint16_t>(currentType);

        outBuffer.insert(outBuffer.end(),
                        reinterpret_cast<uint8_t*>(&length),
                        reinterpret_cast<uint8_t*>(&length) + sizeof(uint16_t));
        outBuffer.insert(outBuffer.end(),
                        reinterpret_cast<uint8_t*>(&type),
                        reinterpret_cast<uint8_t*>(&type) + sizeof(uint16_t));

        i += runLength;
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
        uint16_t runLength;
        std::memcpy(&runLength, buffer + bufferPos, sizeof(uint16_t));
        bufferPos += sizeof(uint16_t);

        // Read blockType
        if (bufferPos + sizeof(uint16_t) > size) {
            LOG_ERROR("Corrupted RLE data: unexpected end while reading block type");
            return false;
        }
        uint16_t blockType;
        std::memcpy(&blockType, buffer + bufferPos, sizeof(uint16_t));
        bufferPos += sizeof(uint16_t);

        // Validate we won't overflow output
        if (blockPos + runLength > maxBlocks) {
            LOG_ERROR("Corrupted RLE data: run would overflow output buffer");
            return false;
        }

        // Write blocks
        Block block;
        block.type = static_cast<BlockType>(blockType);
        for (uint16_t i = 0; i < runLength; i++) {
            outBlocks[blockPos++] = block;
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
