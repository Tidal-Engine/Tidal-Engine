#pragma once

#include "game/Chunk.h"
#include "game/SaveSystem.h"
#include "vulkan/VulkanDevice.h"
#include "vulkan/VulkanBuffer.h"
#include "graphics/TextureManager.h"
#include "core/Camera.h"
#include <unordered_map>
#include <vector>
#include <mutex>
#include <memory>
#include <glm/glm.hpp>

// Forward declarations
class GameClient;
class ClientChunk;

class ClientChunkManager {
public:
    ClientChunkManager(VulkanDevice& device, TextureManager* textureManager = nullptr);
    ~ClientChunkManager();

    // Chunk management
    void setChunkData(const ChunkPos& pos, const ChunkData& data);
    void removeChunk(const ChunkPos& pos);
    void updateBlock(const glm::ivec3& worldPos, BlockType blockType);
    bool hasChunk(const ChunkPos& pos) const;
    std::vector<ChunkPos> getLoadedChunkPositions() const;

    // Rendering
    size_t renderVisibleChunks(VkCommandBuffer commandBuffer, const Frustum& frustum);
    void renderChunkBoundaries(VkCommandBuffer commandBuffer, VkPipeline wireframePipeline,
                              const glm::vec3& playerPosition, VkPipelineLayout pipelineLayout,
                              VkDescriptorSet descriptorSet);

    // Mesh management
    void updateDirtyMeshes();
    void updateDirtyMeshesSafe();
    void setFrameInProgress(bool inProgress) { m_isFrameInProgress = inProgress; }

    // Statistics
    size_t getLoadedChunkCount() const;
    size_t getTotalVertexCount() const;
    size_t getTotalFaceCount() const;

    // Voxel queries
    bool isVoxelSolidAtWorldPosition(int worldX, int worldY, int worldZ) const;
    bool isVoxelSolidAtWorldPositionUnsafe(int worldX, int worldY, int worldZ) const;

private:
    // Internal methods
    void invalidateNeighboringChunks(const ChunkPos& chunkPos, int localX, int localY, int localZ);
    bool isAABBInFrustum(const AABB& aabb, const Frustum& frustum) const;
    bool isChunkCompletelyOccluded(const ChunkPos& pos) const;

    // Wireframe rendering
    void generateWireframeMesh(const glm::vec3& playerPosition);
    void createWireframeBuffers();

    // Member variables
    VulkanDevice& m_device;
    TextureManager* m_textureManager;

    std::unordered_map<ChunkPos, std::unique_ptr<ClientChunk>> m_chunks;
    mutable std::mutex m_chunkMutex;

    bool m_isFrameInProgress;
    std::vector<ChunkPos> m_deferredMeshUpdates;

    // Wireframe rendering
    bool m_wireframeDirty = true;
    std::vector<ChunkVertex> m_wireframeVertices;
    std::vector<uint32_t> m_wireframeIndices;
    std::unique_ptr<VulkanBuffer> m_wireframeVertexBuffer;
    std::unique_ptr<VulkanBuffer> m_wireframeIndexBuffer;
};