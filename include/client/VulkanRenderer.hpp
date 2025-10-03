#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

namespace engine {

struct UniformBufferObject;
class ChunkRenderer;

/**
 * @brief Manages Vulkan command buffers and frame rendering
 *
 * Handles command pool creation, command buffer recording, depth resources,
 * synchronization primitives (semaphores and fences), and the main draw loop
 * with uniform buffer updates.
 */
class VulkanRenderer {
public:
    /**
     * @brief Construct a new Vulkan Renderer
     * @param device Logical Vulkan device
     * @param physicalDevice Physical device for memory queries
     * @param graphicsQueueFamily Graphics queue family index
     * @param graphicsQueue Graphics queue for command submission
     * @param presentQueue Present queue for swapchain presentation
     */
    VulkanRenderer(VkDevice device, VkPhysicalDevice physicalDevice,
                   uint32_t graphicsQueueFamily, VkQueue graphicsQueue,
                   VkQueue presentQueue);
    ~VulkanRenderer() = default;

    // Delete copy operations (Vulkan handles shouldn't be copied)
    VulkanRenderer(const VulkanRenderer&) = delete;
    VulkanRenderer& operator=(const VulkanRenderer&) = delete;

    // Allow move operations
    VulkanRenderer(VulkanRenderer&&) noexcept = default;
    VulkanRenderer& operator=(VulkanRenderer&&) noexcept = default;

    /**
     * @brief Create command pool for allocating command buffers
     */
    void createCommandPool();

    /**
     * @brief Create depth buffer resources
     * @param extent Swapchain extent for depth image dimensions
     */
    void createDepthResources(VkExtent2D extent);

    /**
     * @brief Recreate depth resources after swapchain recreation
     * @param extent New swapchain extent
     */
    void recreateDepthResources(VkExtent2D extent);

    /**
     * @brief Create command buffers for frame rendering
     * @param count Number of command buffers (typically MAX_FRAMES_IN_FLIGHT)
     */
    void createCommandBuffers(uint32_t count);

    /**
     * @brief Create synchronization objects (semaphores and fences)
     * @param maxFramesInFlight Number of frames that can be processed concurrently
     */
    void createSyncObjects(uint32_t maxFramesInFlight);

    /**
     * @brief Record rendering commands into a command buffer
     * @param commandBuffer Command buffer to record into
     * @param imageIndex Swapchain image index
     * @param renderPass Render pass to use
     * @param framebuffers Vector of framebuffers
     * @param extent Render area extent
     * @param pipeline Graphics pipeline
     * @param pipelineLayout Pipeline layout
     * @param vertexBuffer Vertex buffer to bind
     * @param indexBuffer Index buffer to bind
     * @param indexCount Number of indices to draw
     * @param descriptorSets Descriptor sets for uniforms
     */
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex,
                            VkRenderPass renderPass, const std::vector<VkFramebuffer>& framebuffers,
                            VkExtent2D extent, VkPipeline pipeline, VkPipelineLayout pipelineLayout,
                            VkBuffer vertexBuffer, VkBuffer indexBuffer, uint32_t indexCount,
                            const std::vector<VkDescriptorSet>& descriptorSets) const;

    /**
     * @brief Draw a frame with synchronization and presentation
     * @param swapchain Swapchain to present to
     * @param framebuffers Vector of framebuffers
     * @param renderPass Render pass
     * @param extent Render area extent
     * @param pipeline Graphics pipeline
     * @param pipelineLayout Pipeline layout
     * @param vertexBuffer Vertex buffer
     * @param indexBuffer Index buffer
     * @param indexCount Number of indices
     * @param descriptorSets Descriptor sets
     * @param uniformBuffersMapped Mapped uniform buffer pointers
     * @param maxFramesInFlight Maximum frames in flight
     * @return true if swapchain needs recreation, false otherwise
     */
    bool drawFrame(VkSwapchainKHR swapchain, const std::vector<VkFramebuffer>& framebuffers,
                  VkRenderPass renderPass, VkExtent2D extent,
                  VkPipeline pipeline, VkPipelineLayout pipelineLayout,
                  VkBuffer vertexBuffer, VkBuffer indexBuffer, uint32_t indexCount,
                  const std::vector<VkDescriptorSet>& descriptorSets,
                  const std::vector<void*>& uniformBuffersMapped,
                  uint32_t maxFramesInFlight);

    /**
     * @brief Wait for device to finish all operations
     */
    void waitIdle();

    /**
     * @brief Clean up all renderer resources
     */
    void cleanup();

    /**
     * @brief Get the command pool handle
     * @return VkCommandPool Command pool
     */
    VkCommandPool getCommandPool() const { return commandPool; }

    /**
     * @brief Get the depth image view
     * @return VkImageView Depth image view
     */
    VkImageView getDepthImageView() const { return depthImageView; }

    /**
     * @brief Get the current frame index
     * @return uint32_t Current frame index
     */
    uint32_t getCurrentFrame() const { return currentFrame; }

    /**
     * @brief Set chunk renderer for drawing voxel chunks
     */
    void setChunkRenderer(ChunkRenderer* renderer) { chunkRenderer = renderer; }

private:
    VkDevice device;
    VkPhysicalDevice physicalDevice;
    uint32_t graphicsQueueFamily;
    VkQueue graphicsQueue;
    VkQueue presentQueue;

    VkCommandPool commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers;

    VkImage depthImage = VK_NULL_HANDLE;
    VkDeviceMemory depthImageMemory = VK_NULL_HANDLE;
    VkImageView depthImageView = VK_NULL_HANDLE;

    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    uint32_t currentFrame = 0;

    ChunkRenderer* chunkRenderer = nullptr;

    /**
     * @brief Update uniform buffer with current frame data (currently unused)
     * @param uniformBufferMapped Pointer to mapped uniform buffer memory
     */
    void updateUniformBuffer(void* uniformBufferMapped);

    /**
     * @brief Create a Vulkan image with specified properties
     * @param width Image width in pixels
     * @param height Image height in pixels
     * @param format Image format
     * @param tiling Image tiling mode
     * @param usage Usage flags for the image
     * @param properties Memory property flags
     * @param image Output image handle
     * @param imageMemory Output memory handle
     */
    void createImage(uint32_t width, uint32_t height, VkFormat format,
                    VkImageTiling tiling, VkImageUsageFlags usage,
                    VkMemoryPropertyFlags properties, VkImage& image,
                    VkDeviceMemory& imageMemory);

    /**
     * @brief Create an image view for an existing image
     * @param image Image to create view for
     * @param format Image format
     * @param aspectFlags Aspect mask (color, depth, etc.)
     * @return Created image view handle
     */
    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
};

} // namespace engine
