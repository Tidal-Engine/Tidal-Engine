#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <string>

namespace engine {

/**
 * @brief Manages Vulkan graphics pipeline and shader resources
 *
 * Handles render pass creation, shader loading, descriptor sets for uniforms,
 * and the complete graphics pipeline configuration including vertex input,
 * rasterization, and depth testing.
 */
class VulkanPipeline {
public:
    /**
     * @brief Construct a new Vulkan Pipeline manager
     * @param device Logical Vulkan device
     * @param extent Swapchain extent for viewport configuration
     * @param imageFormat Swapchain image format for render pass
     */
    VulkanPipeline(VkDevice device, VkExtent2D extent, VkFormat imageFormat);
    ~VulkanPipeline() = default;

    // Delete copy operations (Vulkan handles shouldn't be copied)
    VulkanPipeline(const VulkanPipeline&) = delete;
    VulkanPipeline& operator=(const VulkanPipeline&) = delete;

    // Allow move operations
    VulkanPipeline(VulkanPipeline&&) noexcept = default;
    VulkanPipeline& operator=(VulkanPipeline&&) noexcept = default;

    /**
     * @brief Create render pass with color and depth attachments
     */
    void createRenderPass();

    /**
     * @brief Create descriptor set layout for uniform buffers
     */
    void createDescriptorSetLayout();

    /**
     * @brief Create graphics pipeline from SPIR-V shader files
     * @param vertShaderPath Path to compiled vertex shader (.spv)
     * @param fragShaderPath Path to compiled fragment shader (.spv)
     */
    void createGraphicsPipeline(const std::string& vertShaderPath, const std::string& fragShaderPath);

    /**
     * @brief Create descriptor pool for allocating descriptor sets
     * @param maxSets Maximum number of descriptor sets to allocate
     */
    void createDescriptorPool(uint32_t maxSets);

    /**
     * @brief Create descriptor sets and bind uniform buffers
     * @param uniformBuffers Vector of uniform buffer handles
     * @param bufferSize Size of each uniform buffer
     */
    void createDescriptorSets(const std::vector<VkBuffer>& uniformBuffers, VkDeviceSize bufferSize);

    /**
     * @brief Update descriptor sets with texture sampler
     * @param textureImageView Texture image view to bind
     * @param textureSampler Texture sampler to bind
     */
    void updateTextureDescriptors(VkImageView textureImageView, VkSampler textureSampler);

    /**
     * @brief Clean up all pipeline resources
     */
    void cleanup();

    /**
     * @brief Get the render pass handle
     * @return VkRenderPass Render pass
     */
    VkRenderPass getRenderPass() const { return renderPass; }

    /**
     * @brief Get the descriptor set layout
     * @return VkDescriptorSetLayout Descriptor set layout
     */
    VkDescriptorSetLayout getDescriptorSetLayout() const { return descriptorSetLayout; }

    /**
     * @brief Get the pipeline layout
     * @return VkPipelineLayout Pipeline layout
     */
    VkPipelineLayout getPipelineLayout() const { return pipelineLayout; }

    /**
     * @brief Get the graphics pipeline
     * @return VkPipeline Graphics pipeline
     */
    VkPipeline getPipeline() const { return graphicsPipeline; }

    /**
     * @brief Get the descriptor pool
     * @return VkDescriptorPool Descriptor pool
     */
    VkDescriptorPool getDescriptorPool() const { return descriptorPool; }

    /**
     * @brief Get all descriptor sets
     * @return const std::vector<VkDescriptorSet>& Vector of descriptor sets
     */
    const std::vector<VkDescriptorSet>& getDescriptorSets() const { return descriptorSets; }

private:
    VkDevice device;
    VkExtent2D extent;
    VkFormat imageFormat;

    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline graphicsPipeline = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> descriptorSets;

    VkShaderModule createShaderModule(const std::vector<char>& code);
    std::vector<char> readFile(const std::string& filename);
};

} // namespace engine
