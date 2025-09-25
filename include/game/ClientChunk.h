/**
 * @file ClientChunk.h
 * @brief Client-side chunk representation with Vulkan mesh generation
 * @author Tidal Engine Team
 * @version 1.0
 */

#pragma once

#include "game/Chunk.h"
#include "game/SaveSystem.h"
#include "vulkan/VulkanDevice.h"
#include "vulkan/VulkanBuffer.h"
#include "graphics/TextureManager.h"
#include <memory>
#include <vector>
#include <cstring>

// Forward declarations
class ClientChunkManager;

/**
 * @brief Client-side chunk with rendering and mesh generation capabilities
 *
 * ClientChunk represents a single chunk of voxel data on the client:
 * - Stores voxel data in a 3D array for fast access
 * - Generates optimized meshes for Vulkan rendering
 * - Handles visibility culling and face removal
 * - Manages Vulkan vertex and index buffers
 * - Integrates with texture atlas for efficient rendering
 *
 * Features:
 * - Greedy mesh generation for reduced vertex count
 * - Automatic mesh regeneration on voxel changes
 * - Vulkan buffer management and command recording
 * - Thread-safe voxel queries and modifications
 * - Integration with chunk manager for neighbor queries
 *
 * Rendering Pipeline:
 * 1. Voxel data changes trigger mesh dirty flag
 * 2. Mesh regeneration creates vertex/index arrays
 * 3. Vulkan buffers updated with new geometry
 * 4. Chunk rendered with single draw call
 *
 * @see ClientChunkManager for chunk coordination
 * @see ChunkData for serialization format
 * @see VulkanDevice for graphics resource management
 */
class ClientChunk {
public:
    /**
     * @brief Construct client chunk with required dependencies
     * @param position Chunk position in world coordinates
     * @param device Vulkan device for buffer management
     * @param textureManager Texture atlas manager for rendering
     * @param chunkManager Parent chunk manager for neighbor queries
     */
    ClientChunk(const ChunkPos& position, VulkanDevice& device,
                TextureManager* textureManager, ClientChunkManager* chunkManager);
    /**
     * @brief Default destructor - Vulkan resources cleaned up automatically
     */
    ~ClientChunk() = default;

    /// @name Data Management
    /// @{
    /**
     * @brief Set entire chunk data from save system or network
     * @param data Complete chunk data with voxel array
     */
    void setChunkData(const ChunkData& data);

    /**
     * @brief Update single voxel and mark mesh for regeneration
     * @param x Local X coordinate (0-31)
     * @param y Local Y coordinate (0-31)
     * @param z Local Z coordinate (0-31)
     * @param blockType New block type to set
     */
    void updateBlock(int x, int y, int z, BlockType blockType);
    /// @}

    /// @name Voxel Queries
    /// @{
    /**
     * @brief Check if voxel at local coordinates is solid
     * @param x Local X coordinate (0-31)
     * @param y Local Y coordinate (0-31)
     * @param z Local Z coordinate (0-31)
     * @return True if voxel is solid (not air)
     */
    bool isVoxelSolid(int x, int y, int z) const;

    /**
     * @brief Get block type at local coordinates
     * @param x Local X coordinate (0-31)
     * @param y Local Y coordinate (0-31)
     * @param z Local Z coordinate (0-31)
     * @return Block type at specified position
     */
    BlockType getVoxelType(int x, int y, int z) const;
    /// @}

    /// @name Rendering
    /// @{
    /**
     * @brief Record rendering commands to Vulkan command buffer
     * @param commandBuffer Command buffer to record draw calls
     */
    void render(VkCommandBuffer commandBuffer);

    /**
     * @brief Force mesh regeneration from current voxel data
     */
    void regenerateMesh();
    /// @}

    /// @name State Queries
    /// @{
    /**
     * @brief Check if chunk has no visible geometry
     * @return True if chunk mesh is empty
     */
    bool isEmpty() const { return m_vertices.empty(); }

    /**
     * @brief Check if mesh needs regeneration
     * @return True if mesh is dirty and needs update
     */
    bool isMeshDirty() const { return m_meshDirty; }

    /**
     * @brief Check if chunk has been modified since last save
     * @return True if chunk needs to be saved
     */
    bool isModified() const { return m_modified; }

    /**
     * @brief Mark chunk as modified for save system
     */
    void markAsModified() { m_modified = true; }
    /// @}

    /// @name Statistics
    /// @{
    /**
     * @brief Get number of vertices in current mesh
     * @return Vertex count for performance monitoring
     */
    size_t getVertexCount() const { return m_vertices.size(); }

    /**
     * @brief Get number of indices in current mesh
     * @return Index count for performance monitoring
     */
    size_t getIndexCount() const { return m_indices.size(); }
    /// @}

private:
    /// @name Mesh Generation
    /// @{
    /**
     * @brief Generate optimized mesh from voxel data
     */
    void generateMesh();

    /**
     * @brief Add single voxel face to mesh geometry
     * @param x Local X coordinate of voxel
     * @param y Local Y coordinate of voxel
     * @param z Local Z coordinate of voxel
     * @param faceDir Face direction index (0-5)
     * @param blockType Block type for texture selection
     */
    void addVoxelFace(int x, int y, int z, int faceDir, BlockType blockType);

    /**
     * @brief Check if voxel face should be rendered (not occluded)
     * @param x Local X coordinate of voxel
     * @param y Local Y coordinate of voxel
     * @param z Local Z coordinate of voxel
     * @param faceDir Face direction to check
     * @return True if face should be rendered
     */
    bool shouldRenderFace(int x, int y, int z, int faceDir) const;

    /**
     * @brief Create or update Vulkan vertex and index buffers
     */
    void createBuffers();
    /// @}

    /// @name Core Data
    /// @{
    ChunkPos m_position;                            ///< Chunk position in world
    VulkanDevice& m_device;                         ///< Vulkan device reference
    TextureManager* m_textureManager;               ///< Texture atlas manager
    ClientChunkManager* m_chunkManager;             ///< Parent chunk manager
    /// @}

    /// @name Voxel Data
    /// @{
    BlockType m_voxels[Chunk::SIZE][Chunk::SIZE][Chunk::SIZE];  ///< 3D voxel array
    bool m_meshDirty = true;                        ///< Mesh needs regeneration
    bool m_modified = false;                        ///< Chunk needs saving
    /// @}

    /// @name Rendering Data
    /// @{
    std::vector<ChunkVertex> m_vertices;            ///< CPU vertex data
    std::vector<uint32_t> m_indices;                ///< CPU index data
    std::unique_ptr<VulkanBuffer> m_vertexBuffer;   ///< GPU vertex buffer
    std::unique_ptr<VulkanBuffer> m_indexBuffer;    ///< GPU index buffer
    /// @}
};