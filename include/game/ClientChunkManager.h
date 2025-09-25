/**
 * @file ClientChunkManager.h
 * @brief Client-side world chunk management with frustum culling and rendering
 * @author Tidal Engine Team
 * @version 1.0
 */

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

/**
 * @brief Client-side world chunk management and rendering coordination
 *
 * ClientChunkManager coordinates all client-side chunk operations:
 * - Chunk loading, caching, and removal based on player position
 * - Frustum culling for efficient rendering of visible chunks
 * - Mesh generation scheduling and dirty chunk management
 * - Voxel queries across chunk boundaries
 * - Integration with save system for chunk persistence
 * - Debug visualization (wireframe chunk boundaries)
 *
 * Features:
 * - Thread-safe chunk access with mutex protection
 * - Efficient spatial queries using chunk coordinate hashing
 * - Deferred mesh updates to avoid frame drops
 * - Automatic neighbor invalidation on block changes
 * - Performance statistics and monitoring
 *
 * Rendering Pipeline:
 * 1. Frustum culling determines visible chunks
 * 2. Dirty meshes regenerated during safe windows
 * 3. Visible chunks rendered with single draw calls
 * 4. Debug overlays rendered as wireframe geometry
 *
 * @see ClientChunk for individual chunk management
 * @see GameClient for main client coordination
 * @see VulkanDevice for graphics resource management
 */
class ClientChunkManager {
public:
    /**
     * @brief Construct chunk manager with rendering dependencies
     * @param device Vulkan device for graphics resources
     * @param textureManager Texture atlas manager for chunk rendering
     */
    ClientChunkManager(VulkanDevice& device, TextureManager* textureManager = nullptr);
    /**
     * @brief Destructor - cleanup all chunks and resources
     */
    ~ClientChunkManager();

    /// @name Chunk Management
    /// @{
    /**
     * @brief Load chunk data from network or save system
     * @param pos Chunk position coordinates
     * @param data Complete chunk data with voxels
     */
    void setChunkData(const ChunkPos& pos, const ChunkData& data);

    /**
     * @brief Remove chunk from memory and cleanup resources
     * @param pos Chunk position to remove
     */
    void removeChunk(const ChunkPos& pos);

    /**
     * @brief Update single block across chunk boundaries
     * @param worldPos World position of block to update
     * @param blockType New block type to set
     */
    void updateBlock(const glm::ivec3& worldPos, BlockType blockType);

    /**
     * @brief Check if chunk is loaded in memory
     * @param pos Chunk position to check
     * @return True if chunk is loaded
     */
    bool hasChunk(const ChunkPos& pos) const;

    /**
     * @brief Get list of all loaded chunk positions
     * @return Vector of loaded chunk positions
     */
    std::vector<ChunkPos> getLoadedChunkPositions() const;
    /// @}

    /// @name Rendering
    /// @{
    /**
     * @brief Render all chunks visible within camera frustum
     * @param commandBuffer Vulkan command buffer for recording
     * @param frustum Camera frustum for culling
     * @return Number of chunks rendered
     */
    size_t renderVisibleChunks(VkCommandBuffer commandBuffer, const Frustum& frustum);

    /**
     * @brief Render debug wireframe chunk boundaries
     * @param commandBuffer Vulkan command buffer for recording
     * @param wireframePipeline Pipeline for wireframe rendering
     * @param playerPosition Player position for boundary generation
     * @param pipelineLayout Pipeline layout for push constants
     * @param descriptorSet Descriptor set for uniforms
     */
    void renderChunkBoundaries(VkCommandBuffer commandBuffer, VkPipeline wireframePipeline,
                              const glm::vec3& playerPosition, VkPipelineLayout pipelineLayout,
                              VkDescriptorSet descriptorSet);
    /// @}

    /// @name Mesh Management
    /// @{
    /**
     * @brief Update all chunks marked with dirty meshes
     */
    void updateDirtyMeshes();

    /**
     * @brief Update dirty meshes with thread safety checks
     */
    void updateDirtyMeshesSafe();

    /**
     * @brief Set frame rendering state for safe mesh updates
     * @param inProgress True if frame rendering is in progress
     */
    void setFrameInProgress(bool inProgress) { m_isFrameInProgress = inProgress; }
    /// @}

    /// @name Statistics
    /// @{
    /**
     * @brief Get number of loaded chunks in memory
     * @return Loaded chunk count
     */
    size_t getLoadedChunkCount() const;

    /**
     * @brief Get total vertex count across all loaded chunks
     * @return Total vertex count for performance monitoring
     */
    size_t getTotalVertexCount() const;

    /**
     * @brief Get total face count across all loaded chunks
     * @return Total face count for performance monitoring
     */
    size_t getTotalFaceCount() const;
    /// @}

    /// @name Voxel Queries
    /// @{
    /**
     * @brief Check if voxel is solid at world position (thread-safe)
     * @param worldX World X coordinate
     * @param worldY World Y coordinate
     * @param worldZ World Z coordinate
     * @return True if voxel is solid
     */
    bool isVoxelSolidAtWorldPosition(int worldX, int worldY, int worldZ) const;

    /**
     * @brief Check if voxel is solid at world position (not thread-safe)
     * @param worldX World X coordinate
     * @param worldY World Y coordinate
     * @param worldZ World Z coordinate
     * @return True if voxel is solid
     * @warning Not thread-safe - use with caution
     */
    bool isVoxelSolidAtWorldPositionUnsafe(int worldX, int worldY, int worldZ) const;
    /// @}

private:
    /// @name Internal Methods
    /// @{
    /**
     * @brief Mark neighboring chunks for mesh regeneration
     * @param chunkPos Position of changed chunk
     * @param localX Local X coordinate of changed block
     * @param localY Local Y coordinate of changed block
     * @param localZ Local Z coordinate of changed block
     */
    void invalidateNeighboringChunks(const ChunkPos& chunkPos, int localX, int localY, int localZ);

    /**
     * @brief Test if axis-aligned bounding box is within frustum
     * @param aabb Bounding box to test
     * @param frustum Camera frustum planes
     * @return True if AABB is at least partially visible
     */
    bool isAABBInFrustum(const AABB& aabb, const Frustum& frustum) const;

    /**
     * @brief Check if chunk is completely occluded by other chunks
     * @param pos Chunk position to test
     * @return True if chunk is completely hidden
     * @note Currently returns false - occlusion culling not implemented
     */
    bool isChunkCompletelyOccluded(const ChunkPos& pos) const;
    /// @}

    /// @name Wireframe Debug Rendering
    /// @{
    /**
     * @brief Generate wireframe mesh for chunk boundary visualization
     * @param playerPosition Player position for nearby chunk selection
     */
    void generateWireframeMesh(const glm::vec3& playerPosition);

    /**
     * @brief Create or update Vulkan buffers for wireframe rendering
     */
    void createWireframeBuffers();
    /// @}

    /// @name Core Dependencies
    /// @{
    VulkanDevice& m_device;                 ///< Vulkan device for graphics resources
    TextureManager* m_textureManager;       ///< Texture atlas manager
    /// @}

    /// @name Chunk Storage
    /// @{
    std::unordered_map<ChunkPos, std::unique_ptr<ClientChunk>> m_chunks;   ///< Loaded chunks by position
    mutable std::mutex m_chunkMutex;                                        ///< Thread-safe chunk access
    /// @}

    /// @name Mesh Update Management
    /// @{
    bool m_isFrameInProgress;                       ///< Frame rendering state
    std::vector<ChunkPos> m_deferredMeshUpdates;    ///< Chunks needing mesh updates
    /// @}

    /// @name Wireframe Debug Data
    /// @{
    bool m_wireframeDirty = true;                           ///< Wireframe mesh needs update
    std::vector<ChunkVertex> m_wireframeVertices;           ///< CPU wireframe vertex data
    std::vector<uint32_t> m_wireframeIndices;               ///< CPU wireframe index data
    std::unique_ptr<VulkanBuffer> m_wireframeVertexBuffer;  ///< GPU wireframe vertex buffer
    std::unique_ptr<VulkanBuffer> m_wireframeIndexBuffer;   ///< GPU wireframe index buffer
    /// @}
};
};