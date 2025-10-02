#pragma once

#include <vulkan/vulkan.h>
#include <vector>

namespace engine {

/**
 * @brief Manages Vulkan buffer creation and memory allocation
 *
 * Handles vertex buffers, index buffers, and uniform buffers with proper
 * memory management and staging buffer transfers for optimal GPU performance.
 */
class VulkanBuffer {
public:
    /**
     * @brief Construct a new Vulkan Buffer manager
     * @param device Logical Vulkan device
     * @param physicalDevice Physical device for memory properties
     */
    VulkanBuffer(VkDevice device, VkPhysicalDevice physicalDevice);
    ~VulkanBuffer() = default;

    // Delete copy operations (Vulkan handles shouldn't be copied)
    VulkanBuffer(const VulkanBuffer&) = delete;
    VulkanBuffer& operator=(const VulkanBuffer&) = delete;

    // Allow move operations
    VulkanBuffer(VulkanBuffer&&) noexcept = default;
    VulkanBuffer& operator=(VulkanBuffer&&) noexcept = default;

    /**
     * @brief Create a vertex buffer and transfer data from CPU to GPU
     * @param data Pointer to vertex data
     * @param size Size of vertex data in bytes
     * @param commandPool Command pool for transfer commands
     * @param graphicsQueue Queue for executing transfer commands
     */
    void createVertexBuffer(const void* data, VkDeviceSize size, VkCommandPool commandPool, VkQueue graphicsQueue);

    /**
     * @brief Create an index buffer and transfer data from CPU to GPU
     * @param data Pointer to index data
     * @param size Size of index data in bytes
     * @param commandPool Command pool for transfer commands
     * @param graphicsQueue Queue for executing transfer commands
     */
    void createIndexBuffer(const void* data, VkDeviceSize size, VkCommandPool commandPool, VkQueue graphicsQueue);

    /**
     * @brief Create uniform buffers for per-frame data
     * @param count Number of uniform buffers to create (typically MAX_FRAMES_IN_FLIGHT)
     * @param bufferSize Size of each uniform buffer in bytes
     */
    void createUniformBuffers(uint32_t count, VkDeviceSize bufferSize);

    /**
     * @brief Clean up all buffer resources
     */
    void cleanup();

    /**
     * @brief Get the vertex buffer handle
     * @return VkBuffer Vertex buffer
     */
    VkBuffer getVertexBuffer() const { return vertexBuffer; }

    /**
     * @brief Get the index buffer handle
     * @return VkBuffer Index buffer
     */
    VkBuffer getIndexBuffer() const { return indexBuffer; }

    /**
     * @brief Get all uniform buffer handles
     * @return const std::vector<VkBuffer>& Vector of uniform buffers
     */
    const std::vector<VkBuffer>& getUniformBuffers() const { return uniformBuffers; }

    /**
     * @brief Get mapped memory pointers for uniform buffers
     * @return const std::vector<void*>& Vector of mapped memory pointers for direct CPU access
     */
    const std::vector<void*>& getUniformBuffersMapped() const { return uniformBuffersMapped; }

private:
    VkDevice device;
    VkPhysicalDevice physicalDevice;

    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;

    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;

    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;
    std::vector<void*> uniformBuffersMapped;

    // Helper functions
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                     VkMemoryPropertyFlags properties, VkBuffer& buffer,
                     VkDeviceMemory& bufferMemory);
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size,
                   VkCommandPool commandPool, VkQueue graphicsQueue);
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
};

} // namespace engine
