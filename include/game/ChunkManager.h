/**
 * @file ChunkManager.h
 * @brief Chunk loading, unloading, and rendering management system
 * @author Tidal Engine Team
 * @version 1.0
 */

#pragma once

#include "game/Chunk.h"
#include <unordered_map>
#include <vector>
#include <memory>
#include <climits>

// Forward declarations
class TextureManager;
class SaveSystem;

/**
 * @brief Manages loading, unloading, and rendering of voxel chunks
 *
 * The ChunkManager handles the lifecycle of chunks in the voxel world:
 * - Dynamic loading and unloading based on player position
 * - Frustum culling for efficient rendering
 * - Deferred mesh updates for thread safety
 * - Debug wireframe rendering for chunk boundaries
 * - Integration with save system for persistent world data
 *
 * @see Chunk for individual chunk implementation
 * @see SaveSystem for world persistence
 */
class ChunkManager {
public:
    /**
     * @brief Construct chunk manager with Vulkan device
     * @param device Vulkan device for rendering
     * @param textureManager Optional texture manager for block textures
     */
    ChunkManager(VulkanDevice& device, TextureManager* textureManager = nullptr);

    /**
     * @brief Set save system for world persistence
     * @param saveSystem Save system instance
     */
    void setSaveSystem(SaveSystem* saveSystem) { m_saveSystem = saveSystem; }

    /**
     * @brief Default destructor
     */
    ~ChunkManager() = default;

    /// @name Chunk Lifecycle Management
    /// @{

    /**
     * @brief Load chunks in radius around position
     * @param position World position center
     * @param radius Load radius in chunks
     */
    void loadChunksAroundPosition(const glm::vec3& position, int radius = 2);

    /**
     * @brief Unload chunks beyond distance from position
     * @param position World position center
     * @param unloadDistance Unload distance in chunks
     */
    void unloadDistantChunks(const glm::vec3& position, int unloadDistance = 4);

    /**
     * @brief Regenerate meshes for all dirty chunks (immediate)
     */
    void regenerateDirtyChunks();

    /**
     * @brief Process deferred mesh updates safely between frames
     */
    void regenerateDirtyChunksSafe();
    /// @}

    /// @name Frame State Management
    /// @{

    /**
     * @brief Set whether rendering frame is in progress
     * @param inProgress True if frame rendering is active
     * @note Used for safe mesh updates
     */
    void setFrameInProgress(bool inProgress) { m_isFrameInProgress = inProgress; }
    /// @}

    /// @name Rendering
    /// @{

    /**
     * @brief Render all loaded chunks
     * @param commandBuffer Vulkan command buffer
     */
    void renderAllChunks(VkCommandBuffer commandBuffer);

    /**
     * @brief Render only chunks visible in frustum
     * @param commandBuffer Vulkan command buffer
     * @param frustum Camera frustum for culling
     * @return Number of chunks rendered
     */
    size_t renderVisibleChunks(VkCommandBuffer commandBuffer, const Frustum& frustum);

    /**
     * @brief Render debug wireframe chunk boundaries
     * @param commandBuffer Vulkan command buffer
     * @param wireframePipeline Wireframe rendering pipeline
     * @param playerPosition Player world position
     * @param pipelineLayout Pipeline layout
     * @param descriptorSet Descriptor set for rendering
     */
    void renderChunkBoundaries(VkCommandBuffer commandBuffer, VkPipeline wireframePipeline, const glm::vec3& playerPosition, VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet);
    /// @}

    /// @name Chunk Access
    /// @{

    /**
     * @brief Get chunk at position
     * @param pos Chunk position
     * @return Pointer to chunk or nullptr if not loaded
     */
    Chunk* getChunk(const ChunkPos& pos);

    /**
     * @brief Get list of all loaded chunk positions
     * @return Vector of loaded chunk positions
     */
    std::vector<ChunkPos> getLoadedChunkPositions() const;

    /**
     * @brief Check if voxel at world position is solid
     * @param worldX World X coordinate
     * @param worldY World Y coordinate
     * @param worldZ World Z coordinate
     * @return True if voxel is solid
     */
    bool isVoxelSolidAtWorldPosition(int worldX, int worldY, int worldZ) const;

    /**
     * @brief Set voxel solidity at world position (legacy)
     * @param worldX World X coordinate
     * @param worldY World Y coordinate
     * @param worldZ World Z coordinate
     * @param solid True for solid voxel
     */
    void setVoxelAtWorldPosition(int worldX, int worldY, int worldZ, bool solid);

    /**
     * @brief Set voxel block type at world position
     * @param worldX World X coordinate
     * @param worldY World Y coordinate
     * @param worldZ World Z coordinate
     * @param blockType Block type to set
     */
    void setVoxelAtWorldPosition(int worldX, int worldY, int worldZ, BlockType blockType);
    /// @}

    /// @name Debug Information
    /// @{

    /**
     * @brief Get number of loaded chunks
     * @return Loaded chunk count
     */
    size_t getLoadedChunkCount() const { return m_loadedChunks.size(); }

    /**
     * @brief Get total vertex count across all chunks
     * @return Total vertex count
     */
    size_t getTotalVertexCount() const;

    /**
     * @brief Get total face count across all chunks
     * @return Total face count
     */
    size_t getTotalFaceCount() const;
    /// @}

private:
    /// @name Core Components
    /// @{
    VulkanDevice& m_device;                                         ///< Vulkan device reference
    TextureManager* m_textureManager;                              ///< Texture manager for block textures
    SaveSystem* m_saveSystem = nullptr;                            ///< Save system for world persistence
    std::unordered_map<ChunkPos, std::unique_ptr<Chunk>> m_loadedChunks; ///< Map of loaded chunks
    /// @}

    /// @name Deferred Updates
    /// @{
    std::vector<ChunkPos> m_deferredMeshUpdates;    ///< Chunks needing mesh regeneration
    bool m_isFrameInProgress = false;               ///< Whether rendering frame is active
    /// @}

    /// @name Debug Wireframes
    /// @{
    std::unique_ptr<VulkanBuffer> m_wireframeVertexBuffer;  ///< Wireframe vertex buffer
    std::unique_ptr<VulkanBuffer> m_wireframeIndexBuffer;   ///< Wireframe index buffer
    std::vector<ChunkVertex> m_wireframeVertices;           ///< Wireframe vertex data
    std::vector<uint32_t> m_wireframeIndices;              ///< Wireframe index data
    bool m_wireframeDirty = true;                           ///< Whether wireframe needs update
    ChunkPos m_lastPlayerChunk = ChunkPos(INT_MAX, INT_MAX, INT_MAX); ///< Last player chunk for wireframe updates
    /// @}

    /// @name Helper Functions
    /// @{

    /**
     * @brief Convert world position to chunk position
     * @param worldPos World space position
     * @return Chunk position containing the world position
     */
    ChunkPos worldPositionToChunkPos(const glm::vec3& worldPos) const;

    /**
     * @brief Create a new chunk at position
     * @param pos Chunk position to create
     */
    void createChunk(const ChunkPos& pos);

    /**
     * @brief Mark neighboring chunks as dirty for mesh updates
     * @param pos Center chunk position
     */
    void markNeighboringChunksDirty(const ChunkPos& pos);

    /**
     * @brief Check if chunk at position is loaded
     * @param pos Chunk position to check
     * @return True if chunk is loaded
     */
    bool isChunkLoaded(const ChunkPos& pos) const;

    /**
     * @brief Test if AABB intersects with frustum
     * @param aabb Bounding box to test
     * @param frustum Camera frustum
     * @return True if AABB is visible in frustum
     */
    bool isAABBInFrustum(const AABB& aabb, const Frustum& frustum);

    /**
     * @brief Generate wireframe mesh for chunk boundaries
     * @param playerPosition Player world position for culling
     */
    void generateWireframeMesh(const glm::vec3& playerPosition);

    /**
     * @brief Create Vulkan buffers for wireframe rendering
     */
    void createWireframeBuffers();

    /**
     * @brief Mark neighboring chunks for boundary updates
     * @param chunkPos Chunk position
     * @param localX Local X coordinate within chunk
     * @param localY Local Y coordinate within chunk
     * @param localZ Local Z coordinate within chunk
     */
    void markNeighboringChunksForBoundaryUpdate(const ChunkPos& chunkPos, int localX, int localY, int localZ);
    /// @}
};