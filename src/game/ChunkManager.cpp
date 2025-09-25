#include "game/ChunkManager.h"
#include "game/SaveSystem.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>

ChunkManager::ChunkManager(VulkanDevice& device, TextureManager* textureManager)
    : m_device(device), m_textureManager(textureManager), m_isFrameInProgress(false) {
    // Create initial 3x3 grid of chunks centered at origin
    for (int x = -1; x <= 1; x++) {
        for (int z = -1; z <= 1; z++) {
            createChunk(ChunkPos(x, z));
        }
    }
}

void ChunkManager::loadChunksAroundPosition(const glm::vec3& position, int radius) {
    ChunkPos centerChunk = worldPositionToChunkPos(position);

    // Load chunks in a 3D cube around the center position
    // Limit Y radius to avoid loading too many chunks vertically
    int yRadius = std::min(radius, 3); // Only load 3 chunks above/below

    for (int x = centerChunk.x - radius; x <= centerChunk.x + radius; x++) {
        for (int y = centerChunk.y - yRadius; y <= centerChunk.y + yRadius; y++) {
            for (int z = centerChunk.z - radius; z <= centerChunk.z + radius; z++) {
                ChunkPos pos(x, y, z);
                if (!isChunkLoaded(pos)) {
                    createChunk(pos);
                }
            }
        }
    }
}

void ChunkManager::unloadDistantChunks(const glm::vec3& position, int unloadDistance) {
    ChunkPos centerChunk = worldPositionToChunkPos(position);

    // Find chunks that are too far away
    std::vector<ChunkPos> chunksToUnload;

    for (const auto& [pos, chunk] : m_loadedChunks) {
        int dx = std::abs(pos.x - centerChunk.x);
        int dy = std::abs(pos.y - centerChunk.y);
        int dz = std::abs(pos.z - centerChunk.z);
        int maxDistance = std::max({dx, dy, dz});

        if (maxDistance > unloadDistance) {
            chunksToUnload.push_back(pos);
        }
    }

    // Unload the distant chunks
    for (const ChunkPos& pos : chunksToUnload) {
        m_loadedChunks.erase(pos);
    }

    // Mark wireframe as dirty if any chunks were unloaded
    if (!chunksToUnload.empty()) {
        m_wireframeDirty = true;
    }
}

void ChunkManager::regenerateDirtyChunks() {
    for (auto& [pos, chunk] : m_loadedChunks) {
        if (chunk->isDirty()) {
            // If a frame is in progress, defer the mesh update to avoid queue conflicts
            if (m_isFrameInProgress) {
                // Check if this chunk is already queued for deferred update
                if (std::find(m_deferredMeshUpdates.begin(), m_deferredMeshUpdates.end(), pos) == m_deferredMeshUpdates.end()) {
                    m_deferredMeshUpdates.push_back(pos);
                }
            } else {
                chunk->regenerateMesh();
            }
        }
    }
}

void ChunkManager::regenerateDirtyChunksSafe() {
    // Process all deferred mesh updates safely
    if (m_deferredMeshUpdates.empty()) {
        return; // Nothing to process
    }

    std::cout << "Processing " << m_deferredMeshUpdates.size() << " deferred mesh updates" << std::endl;

    for (const ChunkPos& pos : m_deferredMeshUpdates) {
        auto it = m_loadedChunks.find(pos);
        if (it != m_loadedChunks.end() && it->second->isDirty()) {
            try {
                std::cout << "Processing deferred mesh update for chunk (" << pos.x << ", " << pos.y << ", " << pos.z << ")" << std::endl;
                it->second->regenerateMesh();
            } catch (const std::exception& e) {
                std::cerr << "Error processing deferred mesh update for chunk (" << pos.x << ", " << pos.y << ", " << pos.z << "): " << e.what() << std::endl;
                // Continue processing other chunks
            }
        }
    }
    m_deferredMeshUpdates.clear();
}

void ChunkManager::renderAllChunks(VkCommandBuffer commandBuffer) {
    for (auto& [pos, chunk] : m_loadedChunks) {
        if (!chunk->isEmpty()) {
            chunk->render(commandBuffer);
        }
    }
}

size_t ChunkManager::renderVisibleChunks(VkCommandBuffer commandBuffer, const Frustum& frustum) {
    size_t renderedChunks = 0;
    for (auto& [pos, chunk] : m_loadedChunks) {
        if (!chunk->isEmpty()) {
            // Get the chunk's bounding box
            AABB chunkAABB = chunk->getBoundingBox();

            // Only render if the chunk is visible in the frustum
            if (isAABBInFrustum(chunkAABB, frustum)) {
                chunk->render(commandBuffer);
                renderedChunks++;
            }
        }
    }
    return renderedChunks;
}

bool ChunkManager::isAABBInFrustum(const AABB& aabb, const Frustum& frustum) {
    // For each plane, check if the AABB is completely on the outside
    for (const auto& plane : frustum.planes) {
        // Find the positive vertex (the vertex of the AABB that is furthest along the plane normal)
        glm::vec3 positiveVertex = aabb.min;
        if (plane.normal.x >= 0) positiveVertex.x = aabb.max.x;
        if (plane.normal.y >= 0) positiveVertex.y = aabb.max.y;
        if (plane.normal.z >= 0) positiveVertex.z = aabb.max.z;

        // If the positive vertex is on the negative side of the plane, the AABB is outside
        if (plane.distanceToPoint(positiveVertex) < 0) {
            return false;
        }
    }

    // AABB is either intersecting or inside the frustum
    return true;
}

Chunk* ChunkManager::getChunk(const ChunkPos& pos) {
    auto it = m_loadedChunks.find(pos);
    return (it != m_loadedChunks.end()) ? it->second.get() : nullptr;
}

std::vector<ChunkPos> ChunkManager::getLoadedChunkPositions() const {
    std::vector<ChunkPos> positions;
    positions.reserve(m_loadedChunks.size());

    for (const auto& [pos, chunk] : m_loadedChunks) {
        positions.push_back(pos);
    }

    return positions;
}

size_t ChunkManager::getTotalVertexCount() const {
    size_t total = 0;
    for (const auto& [pos, chunk] : m_loadedChunks) {
        total += chunk->getVertexCount();
    }
    return total;
}

size_t ChunkManager::getTotalFaceCount() const {
    size_t total = 0;
    for (const auto& [pos, chunk] : m_loadedChunks) {
        total += chunk->getIndexCount() / 6; // 6 indices per quad (2 triangles)
    }
    return total;
}

ChunkPos ChunkManager::worldPositionToChunkPos(const glm::vec3& worldPos) const {
    // Convert world coordinates to chunk coordinates
    int chunkX = static_cast<int>(std::floor(worldPos.x / Chunk::SIZE));
    int chunkY = static_cast<int>(std::floor(worldPos.y / Chunk::SIZE));
    int chunkZ = static_cast<int>(std::floor(worldPos.z / Chunk::SIZE));
    return ChunkPos(chunkX, chunkY, chunkZ);
}

void ChunkManager::createChunk(const ChunkPos& pos) {
    auto chunk = std::make_unique<Chunk>(pos, m_device, this, m_textureManager);

    // Try to load chunk from save system first
    bool loadedFromSave = false;
    if (m_saveSystem && m_saveSystem->loadChunkToChunk(*chunk)) {
        loadedFromSave = true;
        std::cout << "Loaded chunk (" << pos.x << ", " << pos.y << ", " << pos.z << ") from save" << std::endl;
    } else {
        // Generate new terrain if no save data exists
        chunk->generateTerrain();
        // std::cout << "Generated new chunk (" << pos.x << ", " << pos.y << ", " << pos.z << ")" << std::endl;
    }

    chunk->regenerateMesh(); // Generate mesh regardless of source
    m_loadedChunks[pos] = std::move(chunk);

    // Mark neighboring chunks as dirty so they regenerate their meshes
    // This fixes face culling issues at chunk boundaries
    markNeighboringChunksDirty(pos);

    // Mark wireframe as dirty since we added a new chunk
    m_wireframeDirty = true;
}

void ChunkManager::markNeighboringChunksDirty(const ChunkPos& pos) {
    // Check all 6 neighboring chunks (±X, ±Y, ±Z)
    std::vector<ChunkPos> neighbors = {
        ChunkPos(pos.x + 1, pos.y, pos.z),
        ChunkPos(pos.x - 1, pos.y, pos.z),
        ChunkPos(pos.x, pos.y + 1, pos.z),
        ChunkPos(pos.x, pos.y - 1, pos.z),
        ChunkPos(pos.x, pos.y, pos.z + 1),
        ChunkPos(pos.x, pos.y, pos.z - 1)
    };

    for (const auto& neighborPos : neighbors) {
        auto it = m_loadedChunks.find(neighborPos);
        if (it != m_loadedChunks.end()) {
            // Neighbor chunk exists, mark it dirty so it regenerates mesh
            it->second->markDirty();
        }
    }
}

bool ChunkManager::isChunkLoaded(const ChunkPos& pos) const {
    return m_loadedChunks.find(pos) != m_loadedChunks.end();
}

bool ChunkManager::isVoxelSolidAtWorldPosition(int worldX, int worldY, int worldZ) const {
    // Convert world coordinates to chunk position and local coordinates
    ChunkPos chunkPos(worldX / Chunk::SIZE, worldY / Chunk::SIZE, worldZ / Chunk::SIZE);

    // Handle negative coordinates properly (floor division)
    if (worldX < 0 && worldX % Chunk::SIZE != 0) chunkPos.x--;
    if (worldY < 0 && worldY % Chunk::SIZE != 0) chunkPos.y--;
    if (worldZ < 0 && worldZ % Chunk::SIZE != 0) chunkPos.z--;

    // Get local coordinates within the chunk
    int localX = worldX - (chunkPos.x * Chunk::SIZE);
    int localY = worldY - (chunkPos.y * Chunk::SIZE);
    int localZ = worldZ - (chunkPos.z * Chunk::SIZE);

    // Ensure local coordinates are in valid range
    if (localX < 0) localX += Chunk::SIZE;
    if (localY < 0) localY += Chunk::SIZE;
    if (localZ < 0) localZ += Chunk::SIZE;

    // Check if chunk is loaded
    auto it = m_loadedChunks.find(chunkPos);
    if (it == m_loadedChunks.end()) {
        return false; // Unloaded chunks are considered empty
    }

    // Check if the voxel is solid in the chunk
    return it->second->isVoxelSolid(localX, localY, localZ);
}

void ChunkManager::setVoxelAtWorldPosition(int worldX, int worldY, int worldZ, bool solid) {
    // Convert world coordinates to chunk position and local coordinates
    ChunkPos chunkPos(worldX / Chunk::SIZE, worldY / Chunk::SIZE, worldZ / Chunk::SIZE);

    // Handle negative coordinates properly (floor division)
    if (worldX < 0 && worldX % Chunk::SIZE != 0) chunkPos.x--;
    if (worldY < 0 && worldY % Chunk::SIZE != 0) chunkPos.y--;
    if (worldZ < 0 && worldZ % Chunk::SIZE != 0) chunkPos.z--;

    // Get local coordinates within the chunk
    int localX = worldX - (chunkPos.x * Chunk::SIZE);
    int localY = worldY - (chunkPos.y * Chunk::SIZE);
    int localZ = worldZ - (chunkPos.z * Chunk::SIZE);

    // Ensure local coordinates are in valid range
    if (localX < 0) localX += Chunk::SIZE;
    if (localY < 0) localY += Chunk::SIZE;
    if (localZ < 0) localZ += Chunk::SIZE;

    // Check if chunk is loaded, if not create it
    auto it = m_loadedChunks.find(chunkPos);
    if (it == m_loadedChunks.end()) {
        createChunk(chunkPos);
        it = m_loadedChunks.find(chunkPos);
        if (it == m_loadedChunks.end()) {
            return; // Failed to create chunk
        }
    }

    // Set the voxel in the chunk and mark it as dirty for mesh regeneration
    it->second->setVoxel(localX, localY, localZ, solid);

    // Mark neighboring chunks as dirty for cross-chunk face culling updates
    markNeighboringChunksForBoundaryUpdate(chunkPos, localX, localY, localZ);
}

void ChunkManager::setVoxelAtWorldPosition(int worldX, int worldY, int worldZ, BlockType blockType) {
    // Convert world coordinates to chunk position and local coordinates
    ChunkPos chunkPos(worldX / Chunk::SIZE, worldY / Chunk::SIZE, worldZ / Chunk::SIZE);

    // Handle negative coordinates properly (floor division)
    if (worldX < 0 && worldX % Chunk::SIZE != 0) chunkPos.x--;
    if (worldY < 0 && worldY % Chunk::SIZE != 0) chunkPos.y--;
    if (worldZ < 0 && worldZ % Chunk::SIZE != 0) chunkPos.z--;

    // Get local coordinates within the chunk
    int localX = worldX - (chunkPos.x * Chunk::SIZE);
    int localY = worldY - (chunkPos.y * Chunk::SIZE);
    int localZ = worldZ - (chunkPos.z * Chunk::SIZE);

    // Ensure local coordinates are in valid range
    if (localX < 0) localX += Chunk::SIZE;
    if (localY < 0) localY += Chunk::SIZE;
    if (localZ < 0) localZ += Chunk::SIZE;

    // Check if chunk is loaded, if not create it
    auto it = m_loadedChunks.find(chunkPos);
    if (it == m_loadedChunks.end()) {
        createChunk(chunkPos);
        it = m_loadedChunks.find(chunkPos);
        if (it == m_loadedChunks.end()) {
            return; // Failed to create chunk
        }
    }

    // Set the voxel in the chunk and mark it as dirty for mesh regeneration
    it->second->setVoxel(localX, localY, localZ, blockType);

    // Mark neighboring chunks as dirty for cross-chunk face culling updates
    markNeighboringChunksForBoundaryUpdate(chunkPos, localX, localY, localZ);
}

void ChunkManager::markNeighboringChunksForBoundaryUpdate(const ChunkPos& chunkPos, int localX, int localY, int localZ) {
    // If the modified voxel is on a chunk boundary, mark neighboring chunks as dirty too
    // This ensures cross-chunk face culling updates properly
    std::vector<ChunkPos> affectedNeighbors;

    // Check if voxel is on any chunk boundary
    if (localX == 0) {
        affectedNeighbors.push_back(ChunkPos(chunkPos.x - 1, chunkPos.y, chunkPos.z));
    }
    if (localX == Chunk::SIZE - 1) {
        affectedNeighbors.push_back(ChunkPos(chunkPos.x + 1, chunkPos.y, chunkPos.z));
    }
    if (localY == 0) {
        affectedNeighbors.push_back(ChunkPos(chunkPos.x, chunkPos.y - 1, chunkPos.z));
    }
    if (localY == Chunk::SIZE - 1) {
        affectedNeighbors.push_back(ChunkPos(chunkPos.x, chunkPos.y + 1, chunkPos.z));
    }
    if (localZ == 0) {
        affectedNeighbors.push_back(ChunkPos(chunkPos.x, chunkPos.y, chunkPos.z - 1));
    }
    if (localZ == Chunk::SIZE - 1) {
        affectedNeighbors.push_back(ChunkPos(chunkPos.x, chunkPos.y, chunkPos.z + 1));
    }

    // Mark affected neighboring chunks as dirty
    for (const ChunkPos& neighborPos : affectedNeighbors) {
        auto neighborIt = m_loadedChunks.find(neighborPos);
        if (neighborIt != m_loadedChunks.end()) {
            neighborIt->second->markDirty();
        }
    }
}

void ChunkManager::renderChunkBoundaries(VkCommandBuffer commandBuffer, VkPipeline wireframePipeline, const glm::vec3& playerPosition, VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet) {
    // Only regenerate wireframe when chunks are loaded/unloaded, not for player movement
    if (m_wireframeDirty) {
        generateWireframeMesh(playerPosition);
        createWireframeBuffers();
        m_wireframeDirty = false;
    }

    if (m_wireframeVertexBuffer && !m_wireframeVertices.empty()) {
        // Bind the wireframe pipeline
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, wireframePipeline);

        // Bind descriptor sets for the wireframe pipeline
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

        VkBuffer vertexBuffers[] = {m_wireframeVertexBuffer->getBuffer()};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(commandBuffer, m_wireframeIndexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(m_wireframeIndices.size()), 1, 0, 0, 0);
    }
}

void ChunkManager::generateWireframeMesh(const glm::vec3& playerPosition) {
    m_wireframeVertices.clear();
    m_wireframeIndices.clear();

    // Note: playerPosition parameter kept for API consistency but not used in simplified version
    (void)playerPosition; // Suppress unused parameter warning

    // Generate wireframe boxes for each loaded chunk
    for (const auto& [pos, chunk] : m_loadedChunks) {
        // Calculate chunk world position
        glm::vec3 chunkWorldPos = glm::vec3(
            static_cast<float>(pos.x * Chunk::SIZE),
            static_cast<float>(pos.y * Chunk::SIZE),
            static_cast<float>(pos.z * Chunk::SIZE)
        );

        // Define the 8 corners of the chunk bounding box
        glm::vec3 corners[8] = {
            chunkWorldPos,                                                    // min corner
            chunkWorldPos + glm::vec3(Chunk::SIZE, 0, 0),                    // +X
            chunkWorldPos + glm::vec3(0, Chunk::SIZE, 0),                    // +Y
            chunkWorldPos + glm::vec3(0, 0, Chunk::SIZE),                    // +Z
            chunkWorldPos + glm::vec3(Chunk::SIZE, Chunk::SIZE, 0),          // +X+Y
            chunkWorldPos + glm::vec3(Chunk::SIZE, 0, Chunk::SIZE),          // +X+Z
            chunkWorldPos + glm::vec3(0, Chunk::SIZE, Chunk::SIZE),          // +Y+Z
            chunkWorldPos + glm::vec3(Chunk::SIZE, Chunk::SIZE, Chunk::SIZE) // max corner
        };

        // Simple consistent color for all wireframes - bright enough to see but not distracting
        glm::vec3 color = glm::vec3(0.6f, 0.6f, 0.6f); // Light gray for all chunks

        uint32_t startIndex = static_cast<uint32_t>(m_wireframeVertices.size());

        // Add vertices
        for (int i = 0; i < 8; i++) {
            m_wireframeVertices.push_back({
                corners[i],                     // position
                glm::vec3(0.0f, 1.0f, 0.0f),   // normal (not used for wireframe)
                glm::vec2(0.0f, 0.0f),         // texCoord (not used for wireframe)
                glm::vec3(0.0f),               // worldOffset (already in world coords)
                color                          // color
            });
        }

        // Define the 12 edges of a cube
        uint32_t edges[12][2] = {
            // Bottom face edges
            {0, 1}, {1, 4}, {4, 2}, {2, 0},
            // Top face edges
            {3, 5}, {5, 7}, {7, 6}, {6, 3},
            // Vertical edges
            {0, 3}, {1, 5}, {4, 7}, {2, 6}
        };

        // Add indices for edges (lines)
        for (int i = 0; i < 12; i++) {
            m_wireframeIndices.push_back(startIndex + edges[i][0]);
            m_wireframeIndices.push_back(startIndex + edges[i][1]);
        }
    }
}

void ChunkManager::createWireframeBuffers() {
    if (m_wireframeVertices.empty()) return;

    // Create vertex buffer
    VkDeviceSize vertexBufferSize = sizeof(m_wireframeVertices[0]) * m_wireframeVertices.size();

    VkBuffer vertexStagingBuffer;
    VkDeviceMemory vertexStagingBufferMemory;
    m_device.createBuffer(vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                         vertexStagingBuffer, vertexStagingBufferMemory);

    void* vertexData;
    vkMapMemory(m_device.getDevice(), vertexStagingBufferMemory, 0, vertexBufferSize, 0, &vertexData);
    memcpy(vertexData, m_wireframeVertices.data(), static_cast<size_t>(vertexBufferSize));
    vkUnmapMemory(m_device.getDevice(), vertexStagingBufferMemory);

    m_wireframeVertexBuffer = std::make_unique<VulkanBuffer>(
        m_device,
        sizeof(m_wireframeVertices[0]),
        static_cast<uint32_t>(m_wireframeVertices.size()),
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    m_device.copyBuffer(vertexStagingBuffer, m_wireframeVertexBuffer->getBuffer(), vertexBufferSize);

    vkDestroyBuffer(m_device.getDevice(), vertexStagingBuffer, nullptr);
    vkFreeMemory(m_device.getDevice(), vertexStagingBufferMemory, nullptr);

    // Create index buffer
    VkDeviceSize indexBufferSize = sizeof(m_wireframeIndices[0]) * m_wireframeIndices.size();

    VkBuffer indexStagingBuffer;
    VkDeviceMemory indexStagingBufferMemory;
    m_device.createBuffer(indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                         indexStagingBuffer, indexStagingBufferMemory);

    void* indexData;
    vkMapMemory(m_device.getDevice(), indexStagingBufferMemory, 0, indexBufferSize, 0, &indexData);
    memcpy(indexData, m_wireframeIndices.data(), static_cast<size_t>(indexBufferSize));
    vkUnmapMemory(m_device.getDevice(), indexStagingBufferMemory);

    m_wireframeIndexBuffer = std::make_unique<VulkanBuffer>(
        m_device,
        sizeof(m_wireframeIndices[0]),
        static_cast<uint32_t>(m_wireframeIndices.size()),
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    m_device.copyBuffer(indexStagingBuffer, m_wireframeIndexBuffer->getBuffer(), indexBufferSize);

    vkDestroyBuffer(m_device.getDevice(), indexStagingBuffer, nullptr);
    vkFreeMemory(m_device.getDevice(), indexStagingBufferMemory, nullptr);
}