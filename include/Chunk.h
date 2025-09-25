#pragma once

#include "VulkanBuffer.h"
#include "VulkanDevice.h"
#include "Camera.h"
#include <glm/glm.hpp>
#include <vector>
#include <memory>

// Forward declarations
class ChunkManager;
class TextureManager;

// Block types for texture mapping
enum class BlockType : uint8_t {
    AIR = 0,        // Empty space
    STONE = 1,
    DIRT = 2,
    GRASS = 3,
    WOOD = 4,
    SAND = 5,
    BRICK = 6,
    COBBLESTONE = 7,
    SNOW = 8,
    COUNT // Keep this last for counting
};

struct ChunkVertex {
    glm::vec3 pos;           // Local position (cube corner)
    glm::vec3 normal;        // Face normal
    glm::vec2 texCoord;      // Texture coordinates
    glm::vec3 worldOffset;   // Voxel world position offset
    glm::vec3 color;         // Voxel color

    static VkVertexInputBindingDescription getBindingDescription();
    static std::array<VkVertexInputAttributeDescription, 5> getAttributeDescriptions();
};

struct ChunkPos {
    int x, y, z;

    ChunkPos(int x = 0, int y = 0, int z = 0) : x(x), y(y), z(z) {}

    bool operator==(const ChunkPos& other) const {
        return x == other.x && y == other.y && z == other.z;
    }
};

// Hash function for ChunkPos to use in unordered_map
namespace std {
    template<>
    struct hash<ChunkPos> {
        size_t operator()(const ChunkPos& pos) const {
            return hash<int>()(pos.x) ^ (hash<int>()(pos.y) << 1) ^ (hash<int>()(pos.z) << 2);
        }
    };
}

class Chunk {
public:
    static constexpr int SIZE = 32;

    Chunk(const ChunkPos& position, VulkanDevice& device, ChunkManager* chunkManager = nullptr, TextureManager* textureManager = nullptr);
    ~Chunk() = default;

    // Chunk data management
    void generateTerrain();
    void regenerateMesh();
    bool isDirty() const { return m_meshDirty; }
    void markClean() { m_meshDirty = false; }
    void markDirty() { m_meshDirty = true; }

    // Voxel access
    bool isVoxelSolid(int x, int y, int z) const;
    BlockType getVoxelType(int x, int y, int z) const;
    void setVoxel(int x, int y, int z, BlockType blockType);
    void setVoxel(int x, int y, int z, bool solid); // Legacy compatibility
    glm::vec2 getTextureCoords(BlockType blockType) const;

    // Rendering
    void render(VkCommandBuffer commandBuffer);
    bool isEmpty() const { return m_vertices.empty(); }

    // Position
    const ChunkPos& getPosition() const { return m_position; }
    glm::vec3 getWorldOffset() const;
    AABB getBoundingBox() const;

    // Mesh data access (for debugging)
    size_t getVertexCount() const { return m_vertices.size(); }
    size_t getIndexCount() const { return m_indices.size(); }

private:
    ChunkPos m_position;
    VulkanDevice& m_device;
    ChunkManager* m_chunkManager;
    TextureManager* m_textureManager;

    // Voxel data - SIZE x SIZE x SIZE (X x Y x Z)
    // Note: BlockType(0) represents air/empty space
    BlockType m_voxels[SIZE][SIZE][SIZE];
    bool m_meshDirty = true;

    // Mesh data
    std::vector<ChunkVertex> m_vertices;
    std::vector<uint32_t> m_indices;
    std::unique_ptr<VulkanBuffer> m_vertexBuffer;
    std::unique_ptr<VulkanBuffer> m_indexBuffer;

    // Mesh generation
    void generateGreedyMesh();
    void generateVoxelMesh();
    void addVoxelFace(int x, int y, int z, int faceDir, BlockType blockType);
    void addGreedySideFace(int faceDir);
    bool shouldRenderFace(int x, int y, int z, int faceDir) const;
    void createBuffers();

    // Winding order verification (debug only)
    #ifdef _DEBUG
    bool verifyWindingOrder() const;
    bool isTriangleCCW(const ChunkVertex& v0, const ChunkVertex& v1, const ChunkVertex& v2, const glm::vec3& expectedNormal) const;
    #endif
};