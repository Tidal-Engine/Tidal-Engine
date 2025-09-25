#include "game/ClientChunkManager.h"
#include "game/ClientChunk.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <cstring>

ClientChunkManager::ClientChunkManager(VulkanDevice& device, TextureManager* textureManager)
    : m_device(device), m_textureManager(textureManager), m_isFrameInProgress(false) {
}

ClientChunkManager::~ClientChunkManager() = default;

size_t ClientChunkManager::renderVisibleChunks(VkCommandBuffer commandBuffer, const Frustum& frustum) {
    // Use a shared lock for reading chunks (or remove lock entirely if single-threaded)
    std::lock_guard<std::mutex> lock(m_chunkMutex);

    size_t renderedChunks = 0;

    for (const auto& [pos, chunk] : m_chunks) {
        if (chunk && !chunk->isEmpty()) {
            // Calculate chunk bounding box (could be cached per chunk)
            AABB chunkAABB;
            chunkAABB.min = glm::vec3(pos.x * Chunk::SIZE, pos.y * Chunk::SIZE, pos.z * Chunk::SIZE);
            chunkAABB.max = chunkAABB.min + glm::vec3(Chunk::SIZE, Chunk::SIZE, Chunk::SIZE);

            // Only render if the chunk is visible in the frustum and not completely occluded
            if (isAABBInFrustum(chunkAABB, frustum) && !isChunkCompletelyOccluded(pos)) {
                chunk->render(commandBuffer);
                renderedChunks++;
            }
        }
    }

    return renderedChunks;
}

void ClientChunkManager::renderChunkBoundaries(VkCommandBuffer commandBuffer, VkPipeline wireframePipeline,
                                              const glm::vec3& playerPosition, VkPipelineLayout pipelineLayout,
                                              VkDescriptorSet descriptorSet) {
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

void ClientChunkManager::setChunkData(const ChunkPos& pos, const ChunkData& data) {
    std::lock_guard<std::mutex> lock(m_chunkMutex);

    auto chunk = std::make_unique<ClientChunk>(pos, m_device, m_textureManager, this);
    chunk->setChunkData(data);

    // Use deferred mesh generation for new chunks too to prevent queue conflicts
    if (m_isFrameInProgress) {
        // Queue for deferred update
        if (std::find(m_deferredMeshUpdates.begin(), m_deferredMeshUpdates.end(), pos) == m_deferredMeshUpdates.end()) {
            m_deferredMeshUpdates.push_back(pos);
        }
    } else {
        chunk->regenerateMesh(); // Generate mesh immediately only when safe
    }

    m_chunks[pos] = std::move(chunk);

    // Mark wireframe as dirty since we added a new chunk
    m_wireframeDirty = true;
}

void ClientChunkManager::removeChunk(const ChunkPos& pos) {
    // Ensure device is idle before removing chunks with GPU resources
    vkDeviceWaitIdle(m_device.getDevice());

    std::lock_guard<std::mutex> lock(m_chunkMutex);
    m_chunks.erase(pos);

    // Mark wireframe as dirty since we removed a chunk
    m_wireframeDirty = true;
}

void ClientChunkManager::updateBlock(const glm::ivec3& worldPos, BlockType blockType) {
    // Convert world position to chunk coordinates (handle negative coordinates correctly)
    ChunkPos chunkPos(
        worldPos.x >= 0 ? worldPos.x / Chunk::SIZE : (worldPos.x - Chunk::SIZE + 1) / Chunk::SIZE,
        worldPos.y >= 0 ? worldPos.y / Chunk::SIZE : (worldPos.y - Chunk::SIZE + 1) / Chunk::SIZE,
        worldPos.z >= 0 ? worldPos.z / Chunk::SIZE : (worldPos.z - Chunk::SIZE + 1) / Chunk::SIZE
    );

    std::cout << "ClientChunkManager: Updating block at world (" << worldPos.x << ", " << worldPos.y << ", " << worldPos.z
              << ") -> chunk (" << chunkPos.x << ", " << chunkPos.y << ", " << chunkPos.z << ")" << std::endl;

    std::lock_guard<std::mutex> lock(m_chunkMutex);
    auto it = m_chunks.find(chunkPos);
    if (it != m_chunks.end()) {
        int localX = worldPos.x - (chunkPos.x * Chunk::SIZE);
        int localY = worldPos.y - (chunkPos.y * Chunk::SIZE);
        int localZ = worldPos.z - (chunkPos.z * Chunk::SIZE);
        std::cout << "Found chunk, updating local position (" << localX << ", " << localY << ", " << localZ << ")" << std::endl;
        it->second->updateBlock(localX, localY, localZ, blockType);

        // Invalidate neighboring chunks if this block is at a chunk boundary
        // This ensures faces between chunks are properly updated
        invalidateNeighboringChunks(chunkPos, localX, localY, localZ);
    } else {
        std::cout << "ERROR: Chunk (" << chunkPos.x << ", " << chunkPos.y << ", " << chunkPos.z << ") not found!" << std::endl;
        std::cout << "Available chunks:" << std::endl;
        for (const auto& [pos, chunk] : m_chunks) {
            std::cout << "  (" << pos.x << ", " << pos.y << ", " << pos.z << ")" << std::endl;
        }
    }
}

size_t ClientChunkManager::getLoadedChunkCount() const {
    std::lock_guard<std::mutex> lock(m_chunkMutex);
    return m_chunks.size();
}

size_t ClientChunkManager::getTotalVertexCount() const {
    std::lock_guard<std::mutex> lock(m_chunkMutex);
    size_t total = 0;
    for (const auto& [pos, chunk] : m_chunks) {
        if (chunk) {
            total += chunk->getVertexCount();
        }
    }
    return total;
}

size_t ClientChunkManager::getTotalFaceCount() const {
    std::lock_guard<std::mutex> lock(m_chunkMutex);
    size_t total = 0;
    for (const auto& [pos, chunk] : m_chunks) {
        if (chunk) {
            total += chunk->getIndexCount() / 6; // 6 indices per quad (2 triangles)
        }
    }
    return total;
}

bool ClientChunkManager::hasChunk(const ChunkPos& pos) const {
    std::lock_guard<std::mutex> lock(m_chunkMutex);
    return m_chunks.find(pos) != m_chunks.end();
}

std::vector<ChunkPos> ClientChunkManager::getLoadedChunkPositions() const {
    std::lock_guard<std::mutex> lock(m_chunkMutex);
    std::vector<ChunkPos> positions;
    positions.reserve(m_chunks.size());

    for (const auto& [pos, chunk] : m_chunks) {
        positions.push_back(pos);
    }

    return positions;
}

void ClientChunkManager::updateDirtyMeshes() {
    std::lock_guard<std::mutex> lock(m_chunkMutex);

    // Update a limited number of dirty meshes per frame to avoid stuttering
    const int MAX_UPDATES_PER_FRAME = 3;
    int updatesThisFrame = 0;

    for (const auto& [pos, chunk] : m_chunks) {
        if (chunk && chunk->isMeshDirty() && updatesThisFrame < MAX_UPDATES_PER_FRAME) {
            // If a frame is in progress, defer the mesh update to avoid queue conflicts
            if (m_isFrameInProgress) {
                // Check if this chunk is already queued for deferred update
                if (std::find(m_deferredMeshUpdates.begin(), m_deferredMeshUpdates.end(), pos) == m_deferredMeshUpdates.end()) {
                    m_deferredMeshUpdates.push_back(pos);
                }
            } else {
                chunk->regenerateMesh();
                updatesThisFrame++;
            }
        }
    }
}

void ClientChunkManager::updateDirtyMeshesSafe() {
    std::lock_guard<std::mutex> lock(m_chunkMutex);

    // Process all deferred mesh updates safely
    if (m_deferredMeshUpdates.empty()) {
        return; // Nothing to process
    }

    std::cout << "Processing " << m_deferredMeshUpdates.size() << " deferred client mesh updates" << std::endl;

    for (const ChunkPos& pos : m_deferredMeshUpdates) {
        auto it = m_chunks.find(pos);
        if (it != m_chunks.end() && it->second->isMeshDirty()) {
            try {
                std::cout << "Processing deferred client mesh update for chunk (" << pos.x << ", " << pos.y << ", " << pos.z << ")" << std::endl;
                it->second->regenerateMesh();
            } catch (const std::exception& e) {
                std::cerr << "Error processing deferred client mesh update for chunk (" << pos.x << ", " << pos.y << ", " << pos.z << "): " << e.what() << std::endl;
                // Continue processing other chunks
            }
        }
    }
    m_deferredMeshUpdates.clear();
}

void ClientChunkManager::generateWireframeMesh(const glm::vec3& playerPosition) {
    std::lock_guard<std::mutex> lock(m_chunkMutex);
    m_wireframeVertices.clear();
    m_wireframeIndices.clear();

    // Note: playerPosition parameter kept for API consistency but not used in simplified version
    (void)playerPosition; // Suppress unused parameter warning

    // Generate wireframe boxes for each loaded chunk
    for (const auto& [pos, chunk] : m_chunks) {
        // Calculate chunk world position
        glm::vec3 chunkWorldPos = glm::vec3(
            static_cast<float>(pos.x * Chunk::SIZE),
            static_cast<float>(pos.y * Chunk::SIZE),
            static_cast<float>(pos.z * Chunk::SIZE)
        );

        // Define the 8 corners of the chunk bounding box
        glm::vec3 corners[8] = {
            chunkWorldPos,                                                     // min corner
            chunkWorldPos + glm::vec3(Chunk::SIZE, 0, 0),                     // +X
            chunkWorldPos + glm::vec3(0, Chunk::SIZE, 0),                     // +Y
            chunkWorldPos + glm::vec3(0, 0, Chunk::SIZE),                     // +Z
            chunkWorldPos + glm::vec3(Chunk::SIZE, Chunk::SIZE, 0),            // +X+Y
            chunkWorldPos + glm::vec3(Chunk::SIZE, 0, Chunk::SIZE),            // +X+Z
            chunkWorldPos + glm::vec3(0, Chunk::SIZE, Chunk::SIZE),            // +Y+Z
            chunkWorldPos + glm::vec3(Chunk::SIZE, Chunk::SIZE, Chunk::SIZE)    // max corner
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

void ClientChunkManager::createWireframeBuffers() {
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

void ClientChunkManager::invalidateNeighboringChunks(const ChunkPos& chunkPos, int localX, int localY, int localZ) {
    // Check if the block is at a chunk boundary and invalidate neighboring chunks
    // This ensures that faces between chunks are properly updated when blocks change

    std::vector<ChunkPos> neighborsToUpdate;

    // Check each axis for boundary conditions
    if (localX == 0) {
        // Block is at the -X boundary, invalidate chunk to the left (-X)
        neighborsToUpdate.push_back({chunkPos.x - 1, chunkPos.y, chunkPos.z});
    }
    if (localX == Chunk::SIZE - 1) {
        // Block is at the +X boundary, invalidate chunk to the right (+X)
        neighborsToUpdate.push_back({chunkPos.x + 1, chunkPos.y, chunkPos.z});
    }

    if (localY == 0) {
        // Block is at the -Y boundary, invalidate chunk below (-Y)
        neighborsToUpdate.push_back({chunkPos.x, chunkPos.y - 1, chunkPos.z});
    }
    if (localY == Chunk::SIZE - 1) {
        // Block is at the +Y boundary, invalidate chunk above (+Y)
        neighborsToUpdate.push_back({chunkPos.x, chunkPos.y + 1, chunkPos.z});
    }

    if (localZ == 0) {
        // Block is at the -Z boundary, invalidate chunk behind (-Z)
        neighborsToUpdate.push_back({chunkPos.x, chunkPos.y, chunkPos.z - 1});
    }
    if (localZ == Chunk::SIZE - 1) {
        // Block is at the +Z boundary, invalidate chunk in front (+Z)
        neighborsToUpdate.push_back({chunkPos.x, chunkPos.y, chunkPos.z + 1});
    }

    // Mark neighboring chunks as dirty so they regenerate their meshes
    for (const auto& neighborPos : neighborsToUpdate) {
        auto neighborIt = m_chunks.find(neighborPos);
        if (neighborIt != m_chunks.end()) {
            neighborIt->second->markAsModified(); // Mark as modified for occlusion culling

            // Use deferred mesh regeneration to avoid potential deadlocks
            if (m_isFrameInProgress) {
                // Queue for deferred update if frame is in progress
                if (std::find(m_deferredMeshUpdates.begin(), m_deferredMeshUpdates.end(), neighborPos) == m_deferredMeshUpdates.end()) {
                    m_deferredMeshUpdates.push_back(neighborPos);
                }
            } else {
                // Safe to regenerate immediately if no frame in progress
                neighborIt->second->regenerateMesh();
            }
        }
    }
}

bool ClientChunkManager::isAABBInFrustum(const AABB& aabb, const Frustum& frustum) const {
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

bool ClientChunkManager::isChunkCompletelyOccluded(const ChunkPos& pos) const {
    // Calculate world Y range for this chunk
    int chunkWorldYMax = pos.y * Chunk::SIZE + Chunk::SIZE - 1;

    // For flat world terrain (stone up to Y=10, air above):
    // Underground chunks (Y < 0) are occluded if:
    // 1. They're completely below ground level (Y max < 0)
    // 2. There are solid chunks above them (surface chunks exist)
    // 3. Neither the underground chunk nor surface chunk has been modified

    if (chunkWorldYMax < 0) {
        // This is an underground chunk - first check if it's been modified
        auto undergroundIt = m_chunks.find(pos);
        if (undergroundIt != m_chunks.end() && undergroundIt->second && undergroundIt->second->isModified()) {
            return false; // Underground chunk has been modified (dug into) - must render
        }

        // Check if surface chunk above exists and hasn't been modified
        ChunkPos surfaceChunk = {pos.x, 0, pos.z}; // Surface level chunk
        auto surfaceIt = m_chunks.find(surfaceChunk);
        if (surfaceIt != m_chunks.end() && surfaceIt->second && !surfaceIt->second->isEmpty()) {
            if (surfaceIt->second->isModified()) {
                return false; // Surface chunk modified (dug through) - underground is potentially visible
            }

            // Also check neighboring surface chunks for modifications that might expose this underground chunk
            std::vector<ChunkPos> neighbors = {
                {pos.x - 1, 0, pos.z}, {pos.x + 1, 0, pos.z},  // Left/Right
                {pos.x, 0, pos.z - 1}, {pos.x, 0, pos.z + 1}   // Front/Back
            };

            for (const auto& neighbor : neighbors) {
                auto neighborIt = m_chunks.find(neighbor);
                if (neighborIt != m_chunks.end() && neighborIt->second && neighborIt->second->isModified()) {
                    return false; // Neighboring surface chunk modified - might expose underground
                }
            }

            // Surface chunk exists and is unmodified, no modified neighbors - underground chunk is occluded
            return true;
        }
    }

    // Chunks at or above ground level are not occluded by simple depth test
    return false;
}

bool ClientChunkManager::isVoxelSolidAtWorldPosition(int worldX, int worldY, int worldZ) const {
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
    std::lock_guard<std::mutex> lock(m_chunkMutex);
    auto it = m_chunks.find(chunkPos);
    if (it == m_chunks.end()) {
        return false; // Unloaded chunks are considered empty
    }

    // Check if the voxel is solid in the chunk
    return it->second->isVoxelSolid(localX, localY, localZ);
}

bool ClientChunkManager::isVoxelSolidAtWorldPositionUnsafe(int worldX, int worldY, int worldZ) const {
    // Convert world coordinates to chunk position and local coordinates (handle negative properly)
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

    // Check if chunk is loaded (NO LOCK - assumes caller has lock)
    auto it = m_chunks.find(chunkPos);
    if (it == m_chunks.end()) {
        return false; // Unloaded chunks are considered empty
    }

    // Check if the voxel is solid in the chunk
    return it->second->isVoxelSolid(localX, localY, localZ);
}