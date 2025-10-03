#include "client/ChunkRenderer.hpp"
#include "client/ChunkMesh.hpp"
#include "vulkan/VulkanBuffer.hpp"
#include "core/Logger.hpp"

#include <cstring>
#include <stdexcept>

namespace engine {

ChunkRenderer::ChunkRenderer(VkDevice device, VkPhysicalDevice physicalDevice,
                             VkCommandPool commandPool, VkQueue graphicsQueue,
                             const TextureAtlas* atlas)
    : device(device), physicalDevice(physicalDevice),
      commandPool(commandPool), graphicsQueue(graphicsQueue),
      textureAtlas(atlas) {
    LOG_INFO("Chunk renderer initialized");
}

ChunkRenderer::~ChunkRenderer() {
    cleanup();
}

void ChunkRenderer::uploadChunk(const Chunk& chunk,
                                const Chunk* neighborNegX,
                                const Chunk* neighborPosX,
                                const Chunk* neighborNegY,
                                const Chunk* neighborPosY,
                                const Chunk* neighborNegZ,
                                const Chunk* neighborPosZ) {
    const ChunkCoord& coord = chunk.getCoord();

    // Remove existing mesh if present
    removeChunk(coord);

    // Generate mesh with neighboring chunks for cross-chunk face culling
    ChunkMeshData meshData;
    ChunkMesh::generateMesh(chunk, meshData.vertices, meshData.indices, textureAtlas,
                           neighborNegX, neighborPosX,
                           neighborNegY, neighborPosY,
                           neighborNegZ, neighborPosZ);

    if (meshData.vertices.empty() || meshData.indices.empty()) {
        LOG_TRACE("Chunk ({}, {}, {}) has no visible geometry",
                  coord.x, coord.y, coord.z);
        return;
    }

    // Store mesh data in CPU memory and mark buffers dirty
    totalVertices += static_cast<uint32_t>(meshData.vertices.size());
    chunkMeshes[coord] = std::move(meshData);
    buffersDirty = true;

    LOG_INFO("Uploaded chunk ({}, {}, {}) | {} vertices, {} indices",
             coord.x, coord.y, coord.z, chunkMeshes[coord].vertices.size(), chunkMeshes[coord].indices.size());
}

void ChunkRenderer::removeChunk(const ChunkCoord& coord) {
    auto it = chunkMeshes.find(coord);
    if (it != chunkMeshes.end()) {
        totalVertices -= static_cast<uint32_t>(it->second.vertices.size());
        chunkMeshes.erase(it);
        buffersDirty = true;
    }
}

void ChunkRenderer::drawChunks(VkCommandBuffer commandBuffer) {
    // Rebuild batched buffers if dirty
    if (buffersDirty) {
        LOG_WARN("Rebuilding batched buffers during render! This should only happen when chunks change.");
        rebuildBatchedBuffers();
        buffersDirty = false;
    }

    // Nothing to draw if no chunks loaded
    if (batchedBuffers.totalIndexCount == 0) {
        return;
    }

    // Single draw call for all chunks
    VkBuffer vertexBuffers[] = {batchedBuffers.vertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, batchedBuffers.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(commandBuffer, batchedBuffers.totalIndexCount, 1, 0, 0, 0);
}

void ChunkRenderer::cleanup() {
    // Clean up batched buffers
    if (batchedBuffers.vertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, batchedBuffers.vertexBuffer, nullptr);
    }
    if (batchedBuffers.vertexMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, batchedBuffers.vertexMemory, nullptr);
    }
    if (batchedBuffers.indexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, batchedBuffers.indexBuffer, nullptr);
    }
    if (batchedBuffers.indexMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, batchedBuffers.indexMemory, nullptr);
    }

    chunkMeshes.clear();
}

void ChunkRenderer::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                 VkMemoryPropertyFlags properties,
                                 VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        LOG_ERROR("Failed to create buffer of size {}", size);
        throw std::runtime_error("Failed to create buffer");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = VulkanBuffer::findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        LOG_ERROR("Failed to allocate buffer memory of size {}", allocInfo.allocationSize);
        throw std::runtime_error("Failed to allocate buffer memory");
    }

    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}


void ChunkRenderer::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

void ChunkRenderer::rebuildBatchedBuffers() {
    LOG_DEBUG("Rebuilding batched buffers for {} chunks", chunkMeshes.size());

    // Clean up old buffers
    if (batchedBuffers.vertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, batchedBuffers.vertexBuffer, nullptr);
        batchedBuffers.vertexBuffer = VK_NULL_HANDLE;
    }
    if (batchedBuffers.vertexMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, batchedBuffers.vertexMemory, nullptr);
        batchedBuffers.vertexMemory = VK_NULL_HANDLE;
    }
    if (batchedBuffers.indexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, batchedBuffers.indexBuffer, nullptr);
        batchedBuffers.indexBuffer = VK_NULL_HANDLE;
    }
    if (batchedBuffers.indexMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, batchedBuffers.indexMemory, nullptr);
        batchedBuffers.indexMemory = VK_NULL_HANDLE;
    }

    // If no chunks, reset and return
    if (chunkMeshes.empty()) {
        batchedBuffers.totalIndexCount = 0;
        batchedBuffers.totalVertexCount = 0;
        return;
    }

    // Combine all chunk meshes into single buffers
    std::vector<Vertex> combinedVertices;
    std::vector<uint32_t> combinedIndices;

    for (const auto& [coord, meshData] : chunkMeshes) {
        uint32_t vertexOffset = static_cast<uint32_t>(combinedVertices.size());

        // Append vertices
        combinedVertices.insert(combinedVertices.end(),
                               meshData.vertices.begin(),
                               meshData.vertices.end());

        // Append indices with offset
        for (uint32_t index : meshData.indices) {
            combinedIndices.push_back(index + vertexOffset);
        }
    }

    batchedBuffers.totalVertexCount = static_cast<uint32_t>(combinedVertices.size());
    batchedBuffers.totalIndexCount = static_cast<uint32_t>(combinedIndices.size());

    LOG_DEBUG("Combined buffers: {} vertices, {} indices",
             batchedBuffers.totalVertexCount, batchedBuffers.totalIndexCount);

    // Create vertex buffer
    VkDeviceSize vertexBufferSize = sizeof(Vertex) * combinedVertices.size();
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;

    createBuffer(vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                stagingBuffer, stagingMemory);

    void* data;
    vkMapMemory(device, stagingMemory, 0, vertexBufferSize, 0, &data);
    std::memcpy(data, combinedVertices.data(), vertexBufferSize);
    vkUnmapMemory(device, stagingMemory);

    createBuffer(vertexBufferSize,
                VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                batchedBuffers.vertexBuffer, batchedBuffers.vertexMemory);

    copyBuffer(stagingBuffer, batchedBuffers.vertexBuffer, vertexBufferSize);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingMemory, nullptr);

    // Create index buffer
    VkDeviceSize indexBufferSize = sizeof(uint32_t) * combinedIndices.size();

    createBuffer(indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                stagingBuffer, stagingMemory);

    vkMapMemory(device, stagingMemory, 0, indexBufferSize, 0, &data);
    std::memcpy(data, combinedIndices.data(), indexBufferSize);
    vkUnmapMemory(device, stagingMemory);

    createBuffer(indexBufferSize,
                VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                batchedBuffers.indexBuffer, batchedBuffers.indexMemory);

    copyBuffer(stagingBuffer, batchedBuffers.indexBuffer, indexBufferSize);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingMemory, nullptr);

    LOG_DEBUG("Batched buffers rebuilt successfully");
}

} // namespace engine
