#pragma once

#include "game/Chunk.h"
#include <unordered_map>
#include <vector>
#include <memory>
#include <climits>

// Forward declarations
class TextureManager;
class SaveSystem;

class ChunkManager {
public:
    ChunkManager(VulkanDevice& device, TextureManager* textureManager = nullptr);
    void setSaveSystem(SaveSystem* saveSystem) { m_saveSystem = saveSystem; }
    ~ChunkManager() = default;

    // Chunk management
    void loadChunksAroundPosition(const glm::vec3& position, int radius = 2);
    void unloadDistantChunks(const glm::vec3& position, int unloadDistance = 4);
    void regenerateDirtyChunks();
    void regenerateDirtyChunksSafe(); // Process deferred mesh updates safely between frames

    // Frame state tracking for safe mesh updates
    void setFrameInProgress(bool inProgress) { m_isFrameInProgress = inProgress; }

    // Rendering
    void renderAllChunks(VkCommandBuffer commandBuffer);
    size_t renderVisibleChunks(VkCommandBuffer commandBuffer, const Frustum& frustum);
    void renderChunkBoundaries(VkCommandBuffer commandBuffer, VkPipeline wireframePipeline, const glm::vec3& playerPosition, VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet);

    // Chunk access
    Chunk* getChunk(const ChunkPos& pos);
    std::vector<ChunkPos> getLoadedChunkPositions() const;
    bool isVoxelSolidAtWorldPosition(int worldX, int worldY, int worldZ) const;
    void setVoxelAtWorldPosition(int worldX, int worldY, int worldZ, bool solid);
    void setVoxelAtWorldPosition(int worldX, int worldY, int worldZ, BlockType blockType);

    // Debug info
    size_t getLoadedChunkCount() const { return m_loadedChunks.size(); }
    size_t getTotalVertexCount() const;
    size_t getTotalFaceCount() const;

private:
    VulkanDevice& m_device;
    TextureManager* m_textureManager;
    SaveSystem* m_saveSystem = nullptr;
    std::unordered_map<ChunkPos, std::unique_ptr<Chunk>> m_loadedChunks;

    // Deferred mesh update system
    std::vector<ChunkPos> m_deferredMeshUpdates;
    bool m_isFrameInProgress = false;

    // Debug wireframe rendering
    std::unique_ptr<VulkanBuffer> m_wireframeVertexBuffer;
    std::unique_ptr<VulkanBuffer> m_wireframeIndexBuffer;
    std::vector<ChunkVertex> m_wireframeVertices;
    std::vector<uint32_t> m_wireframeIndices;
    bool m_wireframeDirty = true;
    ChunkPos m_lastPlayerChunk = ChunkPos(INT_MAX, INT_MAX, INT_MAX); // Invalid position to force initial update

    // Helper functions
    ChunkPos worldPositionToChunkPos(const glm::vec3& worldPos) const;
    void createChunk(const ChunkPos& pos);
    void markNeighboringChunksDirty(const ChunkPos& pos);
    bool isChunkLoaded(const ChunkPos& pos) const;
    bool isAABBInFrustum(const AABB& aabb, const Frustum& frustum);
    void generateWireframeMesh(const glm::vec3& playerPosition);
    void createWireframeBuffers();
    void markNeighboringChunksForBoundaryUpdate(const ChunkPos& chunkPos, int localX, int localY, int localZ);
};