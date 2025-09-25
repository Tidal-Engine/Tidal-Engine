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

class ClientChunk {
public:
    ClientChunk(const ChunkPos& position, VulkanDevice& device,
                TextureManager* textureManager, ClientChunkManager* chunkManager);
    ~ClientChunk() = default;

    // Data management
    void setChunkData(const ChunkData& data);
    void updateBlock(int x, int y, int z, BlockType blockType);

    // Voxel queries
    bool isVoxelSolid(int x, int y, int z) const;
    BlockType getVoxelType(int x, int y, int z) const;

    // Rendering
    void render(VkCommandBuffer commandBuffer);
    void regenerateMesh();

    // State queries
    bool isEmpty() const { return m_vertices.empty(); }
    bool isMeshDirty() const { return m_meshDirty; }
    bool isModified() const { return m_modified; }
    void markAsModified() { m_modified = true; }

    // Statistics
    size_t getVertexCount() const { return m_vertices.size(); }
    size_t getIndexCount() const { return m_indices.size(); }

private:
    // Mesh generation
    void generateMesh();
    void addVoxelFace(int x, int y, int z, int faceDir, BlockType blockType);
    bool shouldRenderFace(int x, int y, int z, int faceDir) const;
    void createBuffers();

    // Member variables
    ChunkPos m_position;
    VulkanDevice& m_device;
    TextureManager* m_textureManager;
    ClientChunkManager* m_chunkManager;

    BlockType m_voxels[Chunk::SIZE][Chunk::SIZE][Chunk::SIZE];
    bool m_meshDirty = true;
    bool m_modified = false;

    // Rendering data
    std::vector<ChunkVertex> m_vertices;
    std::vector<uint32_t> m_indices;
    std::unique_ptr<VulkanBuffer> m_vertexBuffer;
    std::unique_ptr<VulkanBuffer> m_indexBuffer;
};