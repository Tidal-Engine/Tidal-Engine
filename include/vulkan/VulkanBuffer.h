/**
 * @file VulkanBuffer.h
 * @brief Vulkan buffer management with GPU memory allocation and mapping
 * @author Tidal Engine Team
 * @version 1.0
 */

#pragma once

#include "vulkan/VulkanDevice.h"

/**
 * @brief Vulkan buffer wrapper for GPU memory management
 *
 * The VulkanBuffer class provides a high-level interface for creating and managing
 * Vulkan buffers with associated GPU memory. It handles:
 * - Buffer creation and memory allocation
 * - Memory mapping for CPU-GPU data transfer
 * - Alignment requirements for different buffer types
 * - Instance-based data organization for uniform/vertex buffers
 * - Descriptor buffer info generation for shader binding
 *
 * Common use cases:
 * - Vertex buffers for geometry data
 * - Index buffers for indexed rendering
 * - Uniform buffers for shader constants
 * - Storage buffers for compute shader data
 *
 * @note This class follows RAII principles - resources are automatically cleaned up
 * @see VulkanDevice for the underlying device abstraction
 */
class VulkanBuffer {
public:
    /**
     * @brief Construct Vulkan buffer with specified parameters
     * @param device Vulkan device for buffer creation
     * @param instanceSize Size of each data instance in bytes
     * @param instanceCount Number of instances to allocate
     * @param usageFlags Vulkan buffer usage flags (VK_BUFFER_USAGE_*)
     * @param memoryPropertyFlags Memory property flags (VK_MEMORY_PROPERTY_*)
     * @param minOffsetAlignment Minimum alignment requirement for instances
     *
     * Creates a Vulkan buffer with associated GPU memory. The total buffer size
     * is calculated as instanceSize * instanceCount, properly aligned according
     * to device requirements and minOffsetAlignment.
     *
     * @note Memory is allocated but not initially mapped
     */
    VulkanBuffer(
        VulkanDevice& device,
        VkDeviceSize instanceSize,
        uint32_t instanceCount,
        VkBufferUsageFlags usageFlags,
        VkMemoryPropertyFlags memoryPropertyFlags,
        VkDeviceSize minOffsetAlignment = 1);

    /**
     * @brief Destructor - cleans up Vulkan buffer and associated memory
     * @note Automatically unmaps memory if currently mapped
     */
    ~VulkanBuffer();

    // Non-copyable to prevent accidental resource duplication
    VulkanBuffer(const VulkanBuffer&) = delete;  ///< Copy constructor deleted
    VulkanBuffer& operator=(const VulkanBuffer&) = delete;  ///< Copy assignment deleted

    /// @name Memory Mapping
    /// @{
    /**
     * @brief Map buffer memory for CPU access
     * @param size Number of bytes to map (VK_WHOLE_SIZE for entire buffer)
     * @param offset Byte offset into the buffer
     * @return VK_SUCCESS on success, error code otherwise
     *
     * Maps the specified region of buffer memory to CPU-accessible address space.
     * After mapping, use writeToBuffer() or getMappedMemory() to access data.
     *
     * @warning Only works with buffers created with VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
     */
    VkResult map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);

    /**
     * @brief Unmap previously mapped buffer memory
     * @note Safe to call multiple times - does nothing if not currently mapped
     */
    void unmap();
    /// @}

    /// @name Buffer Operations
    /// @{
    /**
     * @brief Write data to mapped buffer memory
     * @param data Pointer to source data
     * @param size Number of bytes to copy (VK_WHOLE_SIZE for entire buffer)
     * @param offset Byte offset into the buffer
     *
     * Copies data from CPU memory to the mapped buffer region. Buffer must
     * be mapped before calling this function.
     *
     * @warning Undefined behavior if buffer is not mapped or if size/offset exceed bounds
     */
    void writeToBuffer(void* data, VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);

    /**
     * @brief Flush mapped memory changes to GPU
     * @param size Number of bytes to flush (VK_WHOLE_SIZE for entire buffer)
     * @param offset Byte offset into the buffer
     * @return VK_SUCCESS on success, error code otherwise
     *
     * Ensures CPU writes to mapped memory are visible to GPU. Required for
     * buffers without VK_MEMORY_PROPERTY_HOST_COHERENT_BIT.
     */
    VkResult flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);

    /**
     * @brief Create descriptor buffer info for shader binding
     * @param size Buffer range size for descriptor (VK_WHOLE_SIZE for entire buffer)
     * @param offset Byte offset into the buffer
     * @return VkDescriptorBufferInfo structure for descriptor set binding
     *
     * Generates descriptor info structure used when binding buffers to
     * descriptor sets for shader access (uniform buffers, storage buffers, etc.)
     */
    VkDescriptorBufferInfo descriptorInfo(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);

    /**
     * @brief Invalidate mapped memory to read fresh GPU data
     * @param size Number of bytes to invalidate (VK_WHOLE_SIZE for entire buffer)
     * @param offset Byte offset into the buffer
     * @return VK_SUCCESS on success, error code otherwise
     *
     * Ensures CPU reads from mapped memory see fresh GPU writes. Required for
     * buffers without VK_MEMORY_PROPERTY_HOST_COHERENT_BIT when reading GPU data.
     */
    VkResult invalidate(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    /// @}

    /// @name Instance-based Operations
    /// @{
    /**
     * @brief Write data to a specific instance in the buffer
     * @param data Pointer to source data (must be instanceSize bytes)
     * @param index Instance index (0 to instanceCount-1)
     *
     * Writes data to the specified instance slot in the buffer.
     * Automatically calculates the correct offset based on instance size and alignment.
     *
     * @warning Buffer must be mapped and index must be valid
     */
    void writeToIndex(void* data, int index);

    /**
     * @brief Flush a specific instance to GPU
     * @param index Instance index to flush
     * @return VK_SUCCESS on success, error code otherwise
     *
     * Flushes only the memory region corresponding to the specified instance.
     * More efficient than flushing the entire buffer when only one instance changed.
     */
    VkResult flushIndex(int index);

    /**
     * @brief Create descriptor info for a specific instance
     * @param index Instance index
     * @return VkDescriptorBufferInfo for the specified instance
     *
     * Creates descriptor info that points to a single instance within the buffer.
     * Useful for binding individual uniform buffer objects.
     */
    VkDescriptorBufferInfo descriptorInfoForIndex(int index);

    /**
     * @brief Invalidate a specific instance for CPU reads
     * @param index Instance index to invalidate
     * @return VK_SUCCESS on success, error code otherwise
     */
    VkResult invalidateIndex(int index);
    /// @}

    /// @name Accessors
    /// @{
    VkBuffer getBuffer() const { return m_buffer; }  ///< Get underlying Vulkan buffer handle
    void* getMappedMemory() const { return m_mapped; }  ///< Get mapped memory pointer (nullptr if not mapped)
    uint32_t getInstanceCount() const { return m_instanceCount; }  ///< Get number of instances in buffer
    VkDeviceSize getInstanceSize() const { return m_instanceSize; }  ///< Get size of each instance in bytes
    VkDeviceSize getAlignmentSize() const { return m_instanceSize; }  ///< Get alignment size for instances
    VkBufferUsageFlags getUsageFlags() const { return m_usageFlags; }  ///< Get buffer usage flags
    VkMemoryPropertyFlags getMemoryPropertyFlags() const { return m_memoryPropertyFlags; }  ///< Get memory property flags
    VkDeviceSize getBufferSize() const { return m_bufferSize; }  ///< Get total buffer size in bytes
    /// @}

private:
    /**
     * @brief Calculate proper alignment for instance size
     * @param instanceSize Size of each instance
     * @param minOffsetAlignment Minimum required alignment
     * @return Aligned instance size
     *
     * Calculates the proper alignment for instances based on device requirements
     * and minimum alignment constraints. Ensures instances are properly spaced
     * in memory for GPU access patterns.
     */
    static VkDeviceSize getAlignment(VkDeviceSize instanceSize, VkDeviceSize minOffsetAlignment);

    /// @name Member Variables
    /// @{
    VulkanDevice& m_device;                             ///< Reference to Vulkan device
    void* m_mapped = nullptr;                           ///< Mapped memory pointer (nullptr if not mapped)
    VkBuffer m_buffer = VK_NULL_HANDLE;                 ///< Vulkan buffer handle
    VkDeviceMemory m_memory = VK_NULL_HANDLE;           ///< Allocated device memory handle

    VkDeviceSize m_bufferSize;                          ///< Total buffer size in bytes
    uint32_t m_instanceCount;                           ///< Number of instances in buffer
    VkDeviceSize m_instanceSize;                        ///< Size of each instance in bytes
    VkDeviceSize m_alignmentSize;                       ///< Alignment size for instances
    VkBufferUsageFlags m_usageFlags;                    ///< Buffer usage flags
    VkMemoryPropertyFlags m_memoryPropertyFlags;        ///< Memory property flags
    /// @}
};