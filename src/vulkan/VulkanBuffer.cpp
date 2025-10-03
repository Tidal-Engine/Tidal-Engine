#include "vulkan/VulkanBuffer.hpp"
#include "core/Logger.hpp"

#include <stdexcept>
#include <cstring>

namespace engine {

VulkanBuffer::VulkanBuffer(VkDevice device, VkPhysicalDevice physicalDevice)
    : device(device), physicalDevice(physicalDevice) {
}

void VulkanBuffer::createVertexBuffer(const void* data, VkDeviceSize size,
                                     VkCommandPool commandPool, VkQueue graphicsQueue) {
    LOG_DEBUG("Creating vertex buffer (size: {} bytes)", size);

    // Acquire staging buffer from pool
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingBufferMemory = VK_NULL_HANDLE;
    void* mappedData = nullptr;
    acquireStagingBuffer(size, stagingBuffer, stagingBufferMemory, &mappedData);

    // Copy data to staging buffer
    memcpy(mappedData, data, static_cast<size_t>(size));

    // Create device local buffer
    createBuffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                vertexBuffer, vertexBufferMemory);

    // Copy from staging to device local
    copyBuffer(stagingBuffer, vertexBuffer, size, commandPool, graphicsQueue);

    // Release staging buffer back to pool
    releaseStagingBuffer(stagingBuffer, stagingBufferMemory);

    LOG_DEBUG("Vertex buffer created successfully");
}

void VulkanBuffer::createIndexBuffer(const void* data, VkDeviceSize size,
                                    VkCommandPool commandPool, VkQueue graphicsQueue) {
    LOG_DEBUG("Creating index buffer (size: {} bytes)", size);

    // Acquire staging buffer from pool
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingBufferMemory = VK_NULL_HANDLE;
    void* mappedData = nullptr;
    acquireStagingBuffer(size, stagingBuffer, stagingBufferMemory, &mappedData);

    // Copy data to staging buffer
    memcpy(mappedData, data, static_cast<size_t>(size));

    // Create device local buffer
    createBuffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                indexBuffer, indexBufferMemory);

    // Copy from staging to device local
    copyBuffer(stagingBuffer, indexBuffer, size, commandPool, graphicsQueue);

    // Release staging buffer back to pool
    releaseStagingBuffer(stagingBuffer, stagingBufferMemory);

    LOG_DEBUG("Index buffer created successfully");
}

void VulkanBuffer::createUniformBuffers(uint32_t count, VkDeviceSize bufferSize) {
    LOG_DEBUG("Creating {} uniform buffers (size: {} bytes each)", count, bufferSize);

    uniformBuffers.resize(count);
    uniformBuffersMemory.resize(count);
    uniformBuffersMapped.resize(count);

    for (size_t i = 0; i < count; i++) {
        createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    uniformBuffers[i], uniformBuffersMemory[i]);

        if (vkMapMemory(device, uniformBuffersMemory[i], 0, bufferSize, 0, &uniformBuffersMapped[i]) != VK_SUCCESS) {
            LOG_ERROR("Failed to map uniform buffer {} memory", i);
            throw std::runtime_error("Failed to map uniform buffer memory");
        }
    }

    LOG_DEBUG("Uniform buffers created successfully");
}

void VulkanBuffer::cleanup() {
    LOG_DEBUG("Cleaning up buffers");

    // Cleanup staging buffer pool
    for (auto& entry : stagingBufferPool) {
        if (entry.buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, entry.buffer, nullptr);
        }
        if (entry.memory != VK_NULL_HANDLE) {
            vkFreeMemory(device, entry.memory, nullptr);
        }
    }
    stagingBufferPool.clear();

    for (size_t i = 0; i < uniformBuffers.size(); i++) {
        vkDestroyBuffer(device, uniformBuffers[i], nullptr);
        vkFreeMemory(device, uniformBuffersMemory[i], nullptr);
    }

    if (indexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, indexBuffer, nullptr);
        vkFreeMemory(device, indexBufferMemory, nullptr);
    }

    if (vertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, vertexBuffer, nullptr);
        vkFreeMemory(device, vertexBufferMemory, nullptr);
    }
}

void VulkanBuffer::acquireStagingBuffer(VkDeviceSize size, VkBuffer& buffer,
                                       VkDeviceMemory& bufferMemory, void** mappedMemory) {
    // Look for available buffer in pool with sufficient size
    for (auto& entry : stagingBufferPool) {
        if (!entry.inUse && entry.size >= size) {
            entry.inUse = true;
            buffer = entry.buffer;
            bufferMemory = entry.memory;
            *mappedMemory = entry.mapped;
            LOG_TRACE("Reusing staging buffer from pool (size: {})", entry.size);
            return;
        }
    }

    // No suitable buffer found, create new one
    LOG_DEBUG("Creating new staging buffer for pool (size: {})", size);

    StagingBufferEntry newEntry;
    newEntry.size = size;
    newEntry.inUse = true;

    createBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                newEntry.buffer, newEntry.memory);

    if (vkMapMemory(device, newEntry.memory, 0, size, 0, &newEntry.mapped) != VK_SUCCESS) {
        vkDestroyBuffer(device, newEntry.buffer, nullptr);
        vkFreeMemory(device, newEntry.memory, nullptr);
        LOG_ERROR("Failed to map staging buffer memory");
        throw std::runtime_error("Failed to map staging buffer memory");
    }

    buffer = newEntry.buffer;
    bufferMemory = newEntry.memory;
    *mappedMemory = newEntry.mapped;

    stagingBufferPool.push_back(newEntry);
    LOG_DEBUG("Added new staging buffer to pool (total: {})", stagingBufferPool.size());
}

void VulkanBuffer::releaseStagingBuffer(VkBuffer buffer, VkDeviceMemory bufferMemory) {
    for (auto& entry : stagingBufferPool) {
        if (entry.buffer == buffer && entry.memory == bufferMemory) {
            entry.inUse = false;
            LOG_TRACE("Released staging buffer back to pool");
            return;
        }
    }
    LOG_WARN("Attempted to release staging buffer not in pool");
}

void VulkanBuffer::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                               VkMemoryPropertyFlags properties, VkBuffer& buffer,
                               VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        LOG_ERROR("Failed to create buffer");
        throw std::runtime_error("Failed to create buffer");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        LOG_ERROR("Failed to allocate buffer memory");
        throw std::runtime_error("Failed to allocate buffer memory");
    }

    if (vkBindBufferMemory(device, buffer, bufferMemory, 0) != VK_SUCCESS) {
        vkDestroyBuffer(device, buffer, nullptr);
        vkFreeMemory(device, bufferMemory, nullptr);
        LOG_ERROR("Failed to bind buffer memory");
        throw std::runtime_error("Failed to bind buffer memory");
    }
}

void VulkanBuffer::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size,
                             VkCommandPool commandPool, VkQueue graphicsQueue) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
        LOG_ERROR("Failed to allocate command buffer for buffer copy");
        throw std::runtime_error("Failed to allocate command buffer for buffer copy");
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
        LOG_ERROR("Failed to begin command buffer for buffer copy");
        throw std::runtime_error("Failed to begin command buffer for buffer copy");
    }

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
        LOG_ERROR("Failed to end command buffer for buffer copy");
        throw std::runtime_error("Failed to end command buffer for buffer copy");
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
        LOG_ERROR("Failed to submit buffer copy command");
        throw std::runtime_error("Failed to submit buffer copy command");
    }

    if (vkQueueWaitIdle(graphicsQueue) != VK_SUCCESS) {
        LOG_WARN("Queue wait idle failed after buffer copy");
    }

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

uint32_t VulkanBuffer::findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter,
                                      VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    LOG_ERROR("Failed to find suitable memory type (typeFilter: {}, properties: {})", typeFilter, properties);
    throw std::runtime_error("Failed to find suitable memory type");
}

} // namespace engine
