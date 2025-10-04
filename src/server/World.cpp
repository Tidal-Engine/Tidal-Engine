#include "server/World.hpp"
#include "core/Logger.hpp"

#include <cmath>
#include <fstream>
#include <filesystem>
#include <unordered_set>

namespace engine {

World::World() {
    LOG_INFO("Initializing world...");
    // World will be populated by either loadWorld() or generateInitialChunks()
}

void World::generateInitialChunks() {
    // Only generate a small spawn area (3x3x3) to reduce initial load time
    // Additional chunks will be generated dynamically as players explore
    for (int32_t x = -1; x <= 1; x++) {
        for (int32_t y = -1; y <= 1; y++) {
            for (int32_t z = -1; z <= 1; z++) {
                loadChunk(ChunkCoord{x, y, z});
            }
        }
    }

    LOG_INFO("Generated initial spawn area with {} chunks (3x3x3)", chunks.size());
}

void World::update() {
    // TODO: Process block updates, chunk updates, etc.
    // For now, this is a no-op
}

Chunk* World::getChunk(const ChunkCoord& coord) {
    std::lock_guard<std::mutex> lock(chunksMutex);

    auto it = chunks.find(coord);
    if (it != chunks.end()) {
        return it->second.get();
    }
    return nullptr;
}

const Chunk* World::getChunk(const ChunkCoord& coord) const {
    std::lock_guard<std::mutex> lock(chunksMutex);

    auto it = chunks.find(coord);
    if (it != chunks.end()) {
        return it->second.get();
    }
    return nullptr;
}

Chunk& World::loadChunk(const ChunkCoord& coord) {
    std::lock_guard<std::mutex> lock(chunksMutex);

    // Check if already loaded in memory
    auto it = chunks.find(coord);
    if (it != chunks.end()) {
        return *it->second;
    }

    // Try to load from disk first
    std::string filename = "world/chunk_" +
                          std::to_string(coord.x) + "_" +
                          std::to_string(coord.y) + "_" +
                          std::to_string(coord.z) + ".dat";

    if (std::filesystem::exists(filename)) {
        // Load from file
        std::ifstream file(filename, std::ios::binary);
        if (file.is_open()) {
            file.seekg(0, std::ios::end);
            size_t fileSize = file.tellg();
            file.seekg(0, std::ios::beg);

            std::vector<uint8_t> data(fileSize);
            file.read(reinterpret_cast<char*>(data.data()), fileSize);
            file.close();

            auto chunk = std::make_unique<Chunk>(coord);
            if (chunk->deserialize(data)) {
                auto* chunkPtr = chunk.get();
                chunks[coord] = std::move(chunk);
                LOG_DEBUG("Loaded chunk ({}, {}, {}) from disk", coord.x, coord.y, coord.z);
                return *chunkPtr;
            }
        }
    }

    // Generate new chunk if not found on disk
    auto chunk = generateChunk(coord);
    auto* chunkPtr = chunk.get();
    chunks[coord] = std::move(chunk);

    LOG_TRACE("Generated new chunk at ({}, {}, {})", coord.x, coord.y, coord.z);

    return *chunkPtr;
}

void World::unloadChunk(const ChunkCoord& coord) {
    std::lock_guard<std::mutex> lock(chunksMutex);

    auto it = chunks.find(coord);
    if (it != chunks.end()) {
        // TODO: Save chunk to disk if dirty
        chunks.erase(it);
        LOG_TRACE("Unloaded chunk at ({}, {}, {})", coord.x, coord.y, coord.z);
    }
}

Block* World::getBlockAt(int32_t worldX, int32_t worldY, int32_t worldZ) {
    ChunkCoord chunkCoord;
    uint32_t localX, localY, localZ;
    worldToChunkLocal(worldX, worldY, worldZ, chunkCoord, localX, localY, localZ);

    Chunk* chunk = getChunk(chunkCoord);
    if (chunk == nullptr) {
        return nullptr;
    }

    return &chunk->getBlock(localX, localY, localZ);
}

const Block* World::getBlockAt(int32_t worldX, int32_t worldY, int32_t worldZ) const {
    ChunkCoord chunkCoord;
    uint32_t localX, localY, localZ;
    worldToChunkLocal(worldX, worldY, worldZ, chunkCoord, localX, localY, localZ);

    const Chunk* chunk = getChunk(chunkCoord);
    if (chunk == nullptr) {
        return nullptr;
    }

    return &chunk->getBlock(localX, localY, localZ);
}

bool World::setBlockAt(int32_t worldX, int32_t worldY, int32_t worldZ, const Block& block) {
    ChunkCoord chunkCoord;
    uint32_t localX, localY, localZ;
    worldToChunkLocal(worldX, worldY, worldZ, chunkCoord, localX, localY, localZ);

    Chunk* chunk = getChunk(chunkCoord);
    if (chunk == nullptr) {
        return false;
    }

    chunk->setBlock(localX, localY, localZ, block);
    return true;
}

std::vector<const Chunk*> World::getAllChunks() const {
    std::lock_guard<std::mutex> lock(chunksMutex);

    std::vector<const Chunk*> result;
    result.reserve(chunks.size());

    for (const auto& pair : chunks) {
        result.push_back(pair.second.get());
    }

    return result;
}

size_t World::getLoadedChunkCount() const {
    std::lock_guard<std::mutex> lock(chunksMutex);
    return chunks.size();
}

std::unique_ptr<Chunk> World::generateChunk(const ChunkCoord& coord) {
    auto chunk = std::make_unique<Chunk>(coord);

    // Simple flat world generation
    // Fill everything below Y=0 with stone, Y=0 with dirt
    glm::vec3 worldOrigin = coord.toWorldPos();

    for (uint32_t x = 0; x < CHUNK_SIZE; x++) {
        for (uint32_t z = 0; z < CHUNK_SIZE; z++) {
            for (uint32_t y = 0; y < CHUNK_SIZE; y++) {
                // Calculate world Y coordinate for this block
                int32_t worldY = static_cast<int32_t>(worldOrigin.y) + static_cast<int32_t>(y);

                Block block;
                if (worldY < 0) {
                    // Below Y=0: stone
                    block.type = BlockType::Stone;
                } else if (worldY == 0) {
                    // Y=0: grass surface
                    block.type = BlockType::Grass;
                } else {
                    // Above Y=0: air
                    block.type = BlockType::Air;
                }

                chunk->setBlock(x, y, z, block);
            }
        }
    }

    return chunk;
}

void World::worldToChunkLocal(int32_t worldX, int32_t worldY, int32_t worldZ,
                               ChunkCoord& outChunkCoord,
                               uint32_t& outLocalX, uint32_t& outLocalY, uint32_t& outLocalZ) {
    // Get chunk coordinate
    outChunkCoord.x = static_cast<int32_t>(std::floor(static_cast<float>(worldX) / CHUNK_SIZE));
    outChunkCoord.y = static_cast<int32_t>(std::floor(static_cast<float>(worldY) / CHUNK_SIZE));
    outChunkCoord.z = static_cast<int32_t>(std::floor(static_cast<float>(worldZ) / CHUNK_SIZE));

    // Get local coordinates within chunk
    outLocalX = worldX - (outChunkCoord.x * static_cast<int32_t>(CHUNK_SIZE));
    outLocalY = worldY - (outChunkCoord.y * static_cast<int32_t>(CHUNK_SIZE));
    outLocalZ = worldZ - (outChunkCoord.z * static_cast<int32_t>(CHUNK_SIZE));

    // Handle negative coordinates (modulo can give negative results)
    if (outLocalX >= CHUNK_SIZE) outLocalX += CHUNK_SIZE;
    if (outLocalY >= CHUNK_SIZE) outLocalY += CHUNK_SIZE;
    if (outLocalZ >= CHUNK_SIZE) outLocalZ += CHUNK_SIZE;
}

size_t World::saveWorld(const std::string& worldDir) {
    std::lock_guard<std::mutex> lock(chunksMutex);

    // Create world directory if it doesn't exist
    std::filesystem::create_directories(worldDir);

    size_t savedCount = 0;
    std::vector<uint8_t> serializedData;

    for (const auto& [coord, chunk] : chunks) {
        // Only save dirty chunks
        if (!chunk->isDirty()) {
            continue;
        }

        // Serialize chunk
        chunk->serialize(serializedData);

        // Create filename: chunk_x_y_z.dat
        std::string filename = worldDir + "/chunk_" +
                              std::to_string(coord.x) + "_" +
                              std::to_string(coord.y) + "_" +
                              std::to_string(coord.z) + ".dat";

        // Write to file
        std::ofstream file(filename, std::ios::binary);
        if (file.is_open()) {
            file.write(reinterpret_cast<const char*>(serializedData.data()), serializedData.size());
            file.close();
            chunk->clearDirty();
            savedCount++;
        } else {
            LOG_ERROR("Failed to save chunk ({}, {}, {}) to {}", coord.x, coord.y, coord.z, filename);
        }
    }

    if (savedCount > 0) {
        LOG_INFO("Saved {} dirty chunks to {}", savedCount, worldDir);
    } else {
        LOG_DEBUG("No dirty chunks to save (total chunks loaded: {})", chunks.size());
    }

    return savedCount;
}

std::vector<ChunkCoord> World::getChunksInRadius(const glm::vec3& centerPos, int32_t chunkRadius) const {
    std::vector<ChunkCoord> result;

    // Convert center position to chunk coordinate
    ChunkCoord centerChunk = ChunkCoord::fromWorldPos(centerPos);

    // Get all chunks in a horizontal circle around the center (X-Z plane only)
    // Load all Y levels for each X-Z position
    for (int32_t x = centerChunk.x - chunkRadius; x <= centerChunk.x + chunkRadius; x++) {
        for (int32_t z = centerChunk.z - chunkRadius; z <= centerChunk.z + chunkRadius; z++) {
            // Check if within circular radius (Euclidean distance in XZ plane)
            int32_t dx = x - centerChunk.x;
            int32_t dz = z - centerChunk.z;
            float distanceXZ = std::sqrt(static_cast<float>(dx * dx + dz * dz));

            if (distanceXZ <= static_cast<float>(chunkRadius)) {
                // Add all vertical chunks at this X-Z position (typically -1 to 1 for Y)
                for (int32_t y = -1; y <= 1; y++) {
                    result.push_back(ChunkCoord{x, y, z});
                }
            }
        }
    }

    return result;
}

size_t World::unloadDistantChunks(const std::vector<glm::vec3>& playerPositions, int32_t keepRadius) {
    std::lock_guard<std::mutex> lock(chunksMutex);

    // Build set of chunks that should be kept loaded
    std::unordered_set<ChunkCoord> chunksToKeep;

    for (const auto& pos : playerPositions) {
        ChunkCoord playerChunk = ChunkCoord::fromWorldPos(pos);

        // Mark all chunks in radius around this player
        for (int32_t x = playerChunk.x - keepRadius; x <= playerChunk.x + keepRadius; x++) {
            for (int32_t y = playerChunk.y - keepRadius; y <= playerChunk.y + keepRadius; y++) {
                for (int32_t z = playerChunk.z - keepRadius; z <= playerChunk.z + keepRadius; z++) {
                    chunksToKeep.insert(ChunkCoord{x, y, z});
                }
            }
        }
    }

    // Unload chunks not in the keep set
    size_t unloadedCount = 0;
    std::vector<ChunkCoord> toUnload;

    for (const auto& [coord, chunk] : chunks) {
        if (chunksToKeep.find(coord) == chunksToKeep.end()) {
            toUnload.push_back(coord);
        }
    }

    // Unload chunks (do this outside the iteration to avoid iterator invalidation)
    for (const auto& coord : toUnload) {
        // Save chunk if dirty before unloading
        auto it = chunks.find(coord);
        if (it != chunks.end() && it->second->isDirty()) {
            // Will be saved in next autosave
        }
        chunks.erase(coord);
        unloadedCount++;
    }

    if (unloadedCount > 0) {
        LOG_DEBUG("Unloaded {} distant chunks, {} chunks remaining", unloadedCount, chunks.size());
    }

    return unloadedCount;
}

size_t World::loadWorld(const std::string& worldDir) {
    std::lock_guard<std::mutex> lock(chunksMutex);

    if (!std::filesystem::exists(worldDir)) {
        LOG_INFO("World directory {} does not exist, will generate new world", worldDir);
        return 0;
    }

    size_t loadedCount = 0;

    // Iterate through all .dat files in the directory
    for (const auto& entry : std::filesystem::directory_iterator(worldDir)) {
        if (entry.path().extension() != ".dat") {
            continue;
        }

        // Read file
        std::ifstream file(entry.path(), std::ios::binary);
        if (!file.is_open()) {
            LOG_ERROR("Failed to open chunk file {}", entry.path().string());
            continue;
        }

        // Get file size
        file.seekg(0, std::ios::end);
        size_t fileSize = file.tellg();
        file.seekg(0, std::ios::beg);

        // Read data
        std::vector<uint8_t> data(fileSize);
        file.read(reinterpret_cast<char*>(data.data()), fileSize);
        file.close();

        // Parse chunk coordinates from filename (chunk_x_y_z.dat)
        std::string filename = entry.path().filename().string();
        size_t pos1 = filename.find('_');
        size_t pos2 = filename.find('_', pos1 + 1);
        size_t pos3 = filename.find('_', pos2 + 1);
        size_t pos4 = filename.find('.', pos3 + 1);

        if (pos1 == std::string::npos || pos2 == std::string::npos ||
            pos3 == std::string::npos || pos4 == std::string::npos) {
            LOG_ERROR("Invalid chunk filename format: {}", filename);
            continue;
        }

        int32_t x = std::stoi(filename.substr(pos1 + 1, pos2 - pos1 - 1));
        int32_t y = std::stoi(filename.substr(pos2 + 1, pos3 - pos2 - 1));
        int32_t z = std::stoi(filename.substr(pos3 + 1, pos4 - pos3 - 1));

        ChunkCoord coord{x, y, z};

        // Create chunk and deserialize
        auto chunk = std::make_unique<Chunk>(coord);
        if (chunk->deserialize(data)) {
            chunks[coord] = std::move(chunk);
            loadedCount++;
        } else {
            LOG_ERROR("Failed to deserialize chunk ({}, {}, {}) from {}", x, y, z, filename);
        }
    }

    if (loadedCount > 0) {
        LOG_INFO("Loaded {} chunks from {}", loadedCount, worldDir);
    }

    return loadedCount;
}

} // namespace engine
