/**
 * @file Chunk.h
 * @brief Voxel chunk system with greedy meshing and Vulkan rendering
 * @author Tidal Engine Team
 * @version 1.0
 */

#pragma once

#include "vulkan/VulkanBuffer.h"
#include "vulkan/VulkanDevice.h"
#include "core/Camera.h"
#include <glm/glm.hpp>
#include <vector>
#include <memory>

// Forward declarations
class ChunkManager;
class TextureManager;

/**
 * @brief Block types for voxel world texture mapping
 */
enum class BlockType : uint8_t {
    AIR = 0,        ///< Empty space (transparent)
    STONE = 1,      ///< Stone block
    DIRT = 2,       ///< Dirt block
    GRASS = 3,      ///< Grass block
    WOOD = 4,       ///< Wood block
    SAND = 5,       ///< Sand block
    BRICK = 6,      ///< Brick block
    COBBLESTONE = 7,///< Cobblestone block
    SNOW = 8,       ///< Snow block
    COUNT           ///< Keep this last for counting
};

/**
 * @brief Vertex data structure for chunk rendering
 */
struct ChunkVertex {
    glm::vec3 pos;           ///< Local position (cube corner)
    glm::vec3 normal;        ///< Face normal
    glm::vec2 texCoord;      ///< Texture coordinates
    glm::vec3 worldOffset;   ///< Voxel world position offset
    glm::vec3 color;         ///< Voxel color

    /**
     * @brief Get Vulkan vertex input binding description
     * @return Binding description for vertex buffer
     */
    static VkVertexInputBindingDescription getBindingDescription();

    /**
     * @brief Get Vulkan vertex attribute descriptions
     * @return Array of attribute descriptions for shader input
     */
    static std::array<VkVertexInputAttributeDescription, 5> getAttributeDescriptions();
};

/**
 * @brief 3D position for chunk identification
 */
struct ChunkPos {
    int x, y, z;    ///< Chunk coordinates in world space

    /**
     * @brief Default constructor
     * @param x X coordinate (default 0)
     * @param y Y coordinate (default 0)
     * @param z Z coordinate (default 0)
     */
    ChunkPos(int x = 0, int y = 0, int z = 0) : x(x), y(y), z(z) {}

    /**
     * @brief Equality comparison operator
     * @param other Other chunk position to compare
     * @return True if positions are equal
     */
    bool operator==(const ChunkPos& other) const {
        return x == other.x && y == other.y && z == other.z;
    }
};

/**
 * @brief Hash function for ChunkPos to use in unordered_map
 */
namespace std {
    template<>
    struct hash<ChunkPos> {
        /**
         * @brief Hash function for ChunkPos
         * @param pos Chunk position to hash
         * @return Hash value for the position
         */
        size_t operator()(const ChunkPos& pos) const {
            return hash<int>()(pos.x) ^ (hash<int>()(pos.y) << 1) ^ (hash<int>()(pos.z) << 2);
        }
    };
}

/**
 * @brief A 32x32x32 voxel chunk with greedy meshing and Vulkan rendering
 *
 * The Chunk class represents a 3D volume of voxels that can be efficiently
 * rendered using Vulkan. It supports:
 * - Voxel data storage and manipulation
 * - Greedy meshing for optimized geometry generation
 * - Vulkan buffer management for rendering
 * - Frustum culling via bounding box calculation
 * - Terrain generation with various block types
 *
 * @see ChunkManager for chunk loading and unloading
 * @see BlockType for available voxel types
 */
class Chunk {
public:
    static constexpr int SIZE = 32;     ///< Chunk size in voxels (32x32x32)

    /**
     * @brief Construct a new chunk at the given position
     * @param position Chunk position in chunk coordinates
     * @param device Vulkan device for buffer creation
     * @param chunkManager Optional chunk manager for neighbor queries
     * @param textureManager Optional texture manager for texture coordinates
     */
    Chunk(const ChunkPos& position, VulkanDevice& device, ChunkManager* chunkManager = nullptr, TextureManager* textureManager = nullptr);

    /**
     * @brief Default destructor
     */
    ~Chunk() = default;

    /// @name Chunk Data Management
    /// @{

    /**
     * @brief Generate terrain data for this chunk using noise
     */
    void generateTerrain();

    /**
     * @brief Regenerate mesh from current voxel data
     */
    void regenerateMesh();

    /**
     * @brief Check if chunk mesh needs regeneration
     * @return True if mesh is dirty and needs update
     */
    bool isDirty() const { return m_meshDirty; }

    /**
     * @brief Mark chunk mesh as up-to-date
     */
    void markClean() { m_meshDirty = false; }

    /**
     * @brief Mark chunk mesh as needing regeneration
     */
    void markDirty() { m_meshDirty = true; }
    /// @}

    /// @name Voxel Access
    /// @{

    /**
     * @brief Check if voxel at position is solid (non-air)
     * @param x X coordinate within chunk (0-31)
     * @param y Y coordinate within chunk (0-31)
     * @param z Z coordinate within chunk (0-31)
     * @return True if voxel is solid
     */
    bool isVoxelSolid(int x, int y, int z) const;

    /**
     * @brief Get block type at voxel position
     * @param x X coordinate within chunk (0-31)
     * @param y Y coordinate within chunk (0-31)
     * @param z Z coordinate within chunk (0-31)
     * @return Block type at the position
     */
    BlockType getVoxelType(int x, int y, int z) const;

    /**
     * @brief Set block type at voxel position
     * @param x X coordinate within chunk (0-31)
     * @param y Y coordinate within chunk (0-31)
     * @param z Z coordinate within chunk (0-31)
     * @param blockType New block type to set
     */
    void setVoxel(int x, int y, int z, BlockType blockType);

    /**
     * @brief Set voxel solidity (legacy compatibility)
     * @param x X coordinate within chunk (0-31)
     * @param y Y coordinate within chunk (0-31)
     * @param z Z coordinate within chunk (0-31)
     * @param solid True for solid voxel, false for air
     */
    void setVoxel(int x, int y, int z, bool solid);

    /**
     * @brief Get texture coordinates for block type
     * @param blockType Block type to get coordinates for
     * @return Texture coordinates in atlas
     */
    glm::vec2 getTextureCoords(BlockType blockType) const;
    /// @}

    /// @name Rendering
    /// @{

    /**
     * @brief Render chunk using Vulkan command buffer
     * @param commandBuffer Vulkan command buffer to record draw calls
     */
    void render(VkCommandBuffer commandBuffer);

    /**
     * @brief Check if chunk has no visible geometry
     * @return True if chunk is empty (no vertices)
     */
    bool isEmpty() const { return m_vertices.empty(); }
    /// @}

    /// @name Position and Bounds
    /// @{

    /**
     * @brief Get chunk position in chunk coordinates
     * @return Chunk position
     */
    const ChunkPos& getPosition() const { return m_position; }

    /**
     * @brief Get world space offset for this chunk
     * @return World position offset
     */
    glm::vec3 getWorldOffset() const;

    /**
     * @brief Calculate axis-aligned bounding box for frustum culling
     * @return AABB covering this chunk
     */
    AABB getBoundingBox() const;
    /// @}

    /// @name Debugging
    /// @{

    /**
     * @brief Get number of vertices in mesh (for debugging)
     * @return Vertex count
     */
    size_t getVertexCount() const { return m_vertices.size(); }

    /**
     * @brief Get number of indices in mesh (for debugging)
     * @return Index count
     */
    size_t getIndexCount() const { return m_indices.size(); }
    /// @}

private:
    /// @name Core Data
    /// @{
    ChunkPos m_position;            ///< Chunk position in chunk coordinates
    VulkanDevice& m_device;         ///< Vulkan device reference
    ChunkManager* m_chunkManager;   ///< Chunk manager for neighbor queries
    TextureManager* m_textureManager; ///< Texture manager for UV coordinates
    /// @}

    /// @name Voxel Storage
    /// @{
    BlockType m_voxels[SIZE][SIZE][SIZE];   ///< Voxel data (X x Y x Z), BlockType(0) = air
    bool m_meshDirty = true;                ///< Whether mesh needs regeneration
    /// @}

    /// @name Rendering Data
    /// @{
    std::vector<ChunkVertex> m_vertices;            ///< Generated mesh vertices
    std::vector<uint32_t> m_indices;               ///< Generated mesh indices
    std::unique_ptr<VulkanBuffer> m_vertexBuffer;  ///< Vulkan vertex buffer
    std::unique_ptr<VulkanBuffer> m_indexBuffer;   ///< Vulkan index buffer
    /// @}

    /// @name Mesh Generation
    /// @{

    /**
     * @brief Generate optimized mesh using greedy meshing algorithm
     */
    void generateGreedyMesh();

    /**
     * @brief Generate simple cube-per-voxel mesh
     */
    void generateVoxelMesh();

    /**
     * @brief Add a face for a single voxel
     * @param x Voxel X coordinate
     * @param y Voxel Y coordinate
     * @param z Voxel Z coordinate
     * @param faceDir Face direction (0-5)
     * @param blockType Block type for texturing
     */
    void addVoxelFace(int x, int y, int z, int faceDir, BlockType blockType);

    /**
     * @brief Add a greedy mesh face for efficiency
     * @param faceDir Face direction (0-5)
     */
    void addGreedySideFace(int faceDir);

    /**
     * @brief Check if a face should be rendered (not occluded)
     * @param x Voxel X coordinate
     * @param y Voxel Y coordinate
     * @param z Voxel Z coordinate
     * @param faceDir Face direction (0-5)
     * @return True if face should be rendered
     */
    bool shouldRenderFace(int x, int y, int z, int faceDir) const;

    /**
     * @brief Create Vulkan vertex and index buffers from mesh data
     */
    void createBuffers();
    /// @}

    /// @name Debug Tools
    /// @{
    #ifdef _DEBUG
    /**
     * @brief Verify triangle winding order for proper culling
     * @return True if all triangles have correct winding order
     */
    bool verifyWindingOrder() const;

    /**
     * @brief Check if triangle is counter-clockwise
     * @param v0 First vertex
     * @param v1 Second vertex
     * @param v2 Third vertex
     * @param expectedNormal Expected face normal
     * @return True if triangle is counter-clockwise
     */
    bool isTriangleCCW(const ChunkVertex& v0, const ChunkVertex& v1, const ChunkVertex& v2, const glm::vec3& expectedNormal) const;
    #endif
    /// @}
};