#include "shared/Chunk.hpp"
#include "core/Logger.hpp"
#include <cmath>
#include <stdexcept>
#include <cstring>

namespace engine {

Chunk::Chunk(const ChunkCoord& coord)
    : coord(coord) {
    // Initialize all blocks to air
    for (auto& block : blocks) {
        block.type = BlockType::Air;
    }
}

Block& Chunk::getBlock(uint32_t x, uint32_t y, uint32_t z) {  // NOLINT(readability-identifier-length)
    if (x >= CHUNK_SIZE || y >= CHUNK_SIZE || z >= CHUNK_SIZE) {
        LOG_ERROR("Block access out of bounds: ({}, {}, {}) in chunk ({}, {}, {})",
                  x, y, z, coord.x, coord.y, coord.z);
        throw std::out_of_range("Block coordinates out of chunk bounds");
    }
    return blocks[getIndex(x, y, z)];
}

const Block& Chunk::getBlock(uint32_t x, uint32_t y, uint32_t z) const {  // NOLINT(readability-identifier-length)
    if (x >= CHUNK_SIZE || y >= CHUNK_SIZE || z >= CHUNK_SIZE) {
        LOG_ERROR("Block access out of bounds: ({}, {}, {}) in chunk ({}, {}, {})",
                  x, y, z, coord.x, coord.y, coord.z);
        throw std::out_of_range("Block coordinates out of chunk bounds");
    }
    return blocks[getIndex(x, y, z)];
}

void Chunk::setBlock(uint32_t x, uint32_t y, uint32_t z, const Block& block) {  // NOLINT(readability-identifier-length)
    if (x >= CHUNK_SIZE || y >= CHUNK_SIZE || z >= CHUNK_SIZE) {
        LOG_ERROR("Block set out of bounds: ({}, {}, {}) in chunk ({}, {}, {})",
                  x, y, z, coord.x, coord.y, coord.z);
        throw std::out_of_range("Block coordinates out of chunk bounds");
    }
    blocks[getIndex(x, y, z)] = block;
    dirty = true;
}

void Chunk::setBlockData(const std::array<Block, CHUNK_VOLUME>& data) {
    blocks = data;
    dirty = true;
}

void Chunk::serialize(std::vector<uint8_t>& outData) const {
    outData.clear();

    // Format: [chunk_x][chunk_y][chunk_z][block_data]
    // Chunk coordinates (3 x int32_t = 12 bytes)
    outData.resize(12 + (CHUNK_VOLUME * sizeof(Block)));

    size_t offset = 0;

    // Write chunk coordinates
    std::memcpy(outData.data() + offset, &coord.x, sizeof(int32_t));  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    offset += sizeof(int32_t);
    std::memcpy(outData.data() + offset, &coord.y, sizeof(int32_t));  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    offset += sizeof(int32_t);
    std::memcpy(outData.data() + offset, &coord.z, sizeof(int32_t));  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    offset += sizeof(int32_t);

    // Write block data
    std::memcpy(outData.data() + offset, blocks.data(), CHUNK_VOLUME * sizeof(Block));  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
}

bool Chunk::deserialize(const std::vector<uint8_t>& data) {
    // Verify size
    const size_t EXPECTED_SIZE = 12 + (CHUNK_VOLUME * sizeof(Block));
    if (data.size() != EXPECTED_SIZE) {
        LOG_ERROR("Chunk deserialization failed: invalid data size {} (expected {})",
                  data.size(), EXPECTED_SIZE);
        return false;
    }

    size_t offset = 0;

    // Read chunk coordinates
    ChunkCoord loadedCoord;
    std::memcpy(&loadedCoord.x, data.data() + offset, sizeof(int32_t));  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    offset += sizeof(int32_t);
    std::memcpy(&loadedCoord.y, data.data() + offset, sizeof(int32_t));  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    offset += sizeof(int32_t);
    std::memcpy(&loadedCoord.z, data.data() + offset, sizeof(int32_t));  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    offset += sizeof(int32_t);

    // Verify coordinates match
    if (loadedCoord.x != coord.x || loadedCoord.y != coord.y || loadedCoord.z != coord.z) {
        LOG_ERROR("Chunk deserialization failed: coordinate mismatch - expected ({}, {}, {}), got ({}, {}, {})",
                  coord.x, coord.y, coord.z, loadedCoord.x, loadedCoord.y, loadedCoord.z);
        return false;
    }

    // Read block data
    std::memcpy(blocks.data(), data.data() + offset, CHUNK_VOLUME * sizeof(Block));  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)

    dirty = false; // Freshly loaded chunks are clean
    return true;
}

} // namespace engine
