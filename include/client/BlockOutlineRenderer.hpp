#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <optional>
#include <vector>
#include <array>
#include <string>
#include "client/Raycaster.hpp"

namespace engine {

/**
 * @brief Simple vertex for line rendering
 */
struct LineVertex {
    glm::vec3 position;
    glm::vec3 color;

    static VkVertexInputBindingDescription getBindingDescription();
    static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions();
};

/**
 * @brief Renders wireframe outline around targeted block
 *
 * Uses line primitives to draw a white wireframe cube in 3D world space
 */
class BlockOutlineRenderer {
public:
    BlockOutlineRenderer(VkDevice device, VkPhysicalDevice physicalDevice,
                        VkCommandPool commandPool, VkQueue graphicsQueue);
    ~BlockOutlineRenderer();

    // Delete copy operations
    BlockOutlineRenderer(const BlockOutlineRenderer&) = delete;
    BlockOutlineRenderer& operator=(const BlockOutlineRenderer&) = delete;

    // Allow move operations
    BlockOutlineRenderer(BlockOutlineRenderer&&) noexcept = default;
    BlockOutlineRenderer& operator=(BlockOutlineRenderer&&) noexcept = default;

    /**
     * @brief Create rendering resources (pipeline, buffers)
     */
    void init(VkRenderPass renderPass, VkExtent2D swapchainExtent,
             VkDescriptorSetLayout descriptorSetLayout);

    /**
     * @brief Update the outline based on targeted block
     */
    void update(const std::optional<RaycastHit>& targetedBlock);

    /**
     * @brief Record draw commands for the outline
     */
    void draw(VkCommandBuffer commandBuffer, VkDescriptorSet descriptorSet);

    /**
     * @brief Cleanup Vulkan resources
     */
    void cleanup();

    /**
     * @brief Check if there's an outline to render
     */
    bool hasOutline() const { return shouldRender; }

private:
    VkDevice device;
    VkPhysicalDevice physicalDevice;
    VkCommandPool commandPool;
    VkQueue graphicsQueue;

    // Pipeline resources
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkShaderModule vertShaderModule = VK_NULL_HANDLE;
    VkShaderModule fragShaderModule = VK_NULL_HANDLE;

    // Vertex buffer for the 24 line vertices (12 edges * 2 vertices per edge)
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;

    bool shouldRender = false;

    /**
     * @brief Read shader file
     */
    static std::vector<char> readFile(const std::string& filename);

    /**
     * @brief Create shader module from SPIR-V code
     */
    VkShaderModule createShaderModule(const std::vector<char>& code);

    /**
     * @brief Create the graphics pipeline for line rendering
     */
    void createPipeline(VkRenderPass renderPass, VkExtent2D swapchainExtent,
                       VkDescriptorSetLayout descriptorSetLayout);

    /**
     * @brief Create vertex buffer
     */
    void createVertexBuffer();

    /**
     * @brief Update vertex buffer with new block position
     */
    void updateVertexBuffer(const glm::ivec3& blockPos);

    /**
     * @brief Helper: Find memory type
     */
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
};

} // namespace engine
