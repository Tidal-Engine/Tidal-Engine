#pragma once

#include "shared/Chunk.hpp"
#include "shared/ChunkCoord.hpp"
#include "vulkan/Vertex.hpp"

#include <vulkan/vulkan.h>
#include <unordered_map>
#include <vector>
#include <memory>

namespace engine {

// Forward declarations
class VulkanBuffer;
class TextureAtlas;

/**
 * @brief Manages rendering of voxel chunks
 *
 * Stores GPU buffers for each chunk and handles mesh updates.
 */
class ChunkRenderer {
public:
    /**
     * @brief Construct chunk renderer
     * @param device Vulkan logical device
     * @param physicalDevice Vulkan physical device
     * @param commandPool Command pool for buffer operations
     * @param graphicsQueue Graphics queue for command submission
     * @param atlas Texture atlas for block textures (optional)
     */
    ChunkRenderer(VkDevice device, VkPhysicalDevice physicalDevice,
                 VkCommandPool commandPool, VkQueue graphicsQueue,
                 const TextureAtlas* atlas = nullptr);
    ~ChunkRenderer();

    // Delete copy/move operations
    ChunkRenderer(const ChunkRenderer&) = delete;
    ChunkRenderer& operator=(const ChunkRenderer&) = delete;
    ChunkRenderer(ChunkRenderer&&) = delete;
    ChunkRenderer& operator=(ChunkRenderer&&) = delete;

    /**
     * @brief Upload chunk mesh to GPU
     * @param chunk Chunk to upload
     * @param neighborNegX Neighboring chunk in -X direction (optional, for cross-chunk culling)
     * @param neighborPosX Neighboring chunk in +X direction (optional, for cross-chunk culling)
     * @param neighborNegY Neighboring chunk in -Y direction (optional, for cross-chunk culling)
     * @param neighborPosY Neighboring chunk in +Y direction (optional, for cross-chunk culling)
     * @param neighborNegZ Neighboring chunk in -Z direction (optional, for cross-chunk culling)
     * @param neighborPosZ Neighboring chunk in +Z direction (optional, for cross-chunk culling)
     */
    void uploadChunk(const Chunk& chunk,
                    const Chunk* neighborNegX = nullptr,
                    const Chunk* neighborPosX = nullptr,
                    const Chunk* neighborNegY = nullptr,
                    const Chunk* neighborPosY = nullptr,
                    const Chunk* neighborNegZ = nullptr,
                    const Chunk* neighborPosZ = nullptr);

    /**
     * @brief Upload pre-generated chunk mesh to GPU
     * @param coord Chunk coordinate
     * @param vertices Pre-generated vertex data
     * @param indices Pre-generated index data
     */
    void uploadChunkMesh(const ChunkCoord& coord,
                        const std::vector<Vertex>& vertices,
                        const std::vector<uint32_t>& indices);

    /**
     * @brief Remove chunk from GPU
     */
    void removeChunk(const ChunkCoord& coord);

    /**
     * @brief Draw all loaded chunks
     * @param commandBuffer Command buffer to record into
     */
    void drawChunks(VkCommandBuffer commandBuffer);

    /**
     * @brief Get number of loaded chunk meshes
     */
    size_t getLoadedChunkCount() const { return chunkMeshes.size(); }

    /**
     * @brief Get number of chunks (alias for getLoadedChunkCount)
     */
    uint32_t getChunkCount() const { return static_cast<uint32_t>(chunkMeshes.size()); }

    /**
     * @brief Get total vertices across all chunks
     */
    uint32_t getTotalVertices() const { return totalVertices; }

    /**
     * @brief Cleanup all GPU resources
     */
    void cleanup();

private:
    VkDevice device;
    VkPhysicalDevice physicalDevice;
    VkCommandPool commandPool;
    VkQueue graphicsQueue;
    const TextureAtlas* textureAtlas;

    /**
     * @brief CPU-side mesh data for a single chunk (before batching)
     */
    struct ChunkMeshData {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
    };

    /**
     * @brief Batched GPU buffers containing all chunks
     */
    struct BatchedBuffers {
        VkBuffer vertexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory vertexMemory = VK_NULL_HANDLE;
        VkBuffer indexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory indexMemory = VK_NULL_HANDLE;
        uint32_t totalIndexCount = 0;
        uint32_t totalVertexCount = 0;
    };

    std::unordered_map<ChunkCoord, ChunkMeshData> chunkMeshes;
    BatchedBuffers batchedBuffers;
    bool buffersDirty = true;  ///< True when buffers need rebuilding
    uint32_t totalVertices = 0;  ///< Total vertices across all chunks

    /**
     * @brief Create a Vulkan buffer with specified properties
     * @param size Buffer size in bytes
     * @param usage Buffer usage flags
     * @param properties Memory property flags
     * @param buffer Output buffer handle
     * @param bufferMemory Output memory handle
     */
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                     VkMemoryPropertyFlags properties,
                     VkBuffer& buffer, VkDeviceMemory& bufferMemory);

    /**
     * @brief Copy data from one buffer to another using transfer command
     * @param srcBuffer Source buffer
     * @param dstBuffer Destination buffer
     * @param size Number of bytes to copy
     */
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

    /**
     * @brief Rebuild batched GPU buffers from all chunk meshes
     */
    void rebuildBatchedBuffers();
};

} // namespace engine
