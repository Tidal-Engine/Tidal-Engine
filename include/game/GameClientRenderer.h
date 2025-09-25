/**
 * @file GameClientRenderer.h
 * @brief Vulkan-based rendering system for game client
 * @author Tidal Engine Team
 * @version 1.0
 */

#pragma once

#include "vulkan/VulkanDevice.h"
#include "vulkan/VulkanRenderer.h"
#include "vulkan/VulkanBuffer.h"
#include "graphics/TextureManager.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#include <memory>
#include <vector>

// Forward declarations
class GameClient;
class ClientChunkManager;
struct Vertex;
struct UniformBufferObject;

/**
 * @brief High-performance Vulkan rendering system for the game client
 *
 * GameClientRenderer manages all aspects of client-side rendering:
 * - Vulkan graphics pipeline setup and management
 * - Vertex and index buffer management for voxel geometry
 * - Uniform buffer updates for shader transformations
 * - Command buffer recording and submission
 * - Swap chain recreation and resizing
 * - Wireframe and solid rendering modes
 *
 * Features:
 * - Modern Vulkan API with descriptor sets
 * - Efficient mesh rendering with indexed geometry
 * - Dynamic pipeline switching (solid/wireframe)
 * - Integration with texture atlas system
 * - Frame synchronization and double buffering
 *
 * @see GameClient for main client coordination
 * @see VulkanDevice for device abstraction
 * @see VulkanRenderer for low-level Vulkan operations
 */
class GameClientRenderer {
public:
    /**
     * @brief Construct renderer with required dependencies
     * @param gameClient Reference to main game client
     * @param window GLFW window handle for swap chain
     * @param device Vulkan device for resource management
     */
    GameClientRenderer(GameClient& gameClient, GLFWwindow* window, VulkanDevice& device);
    /**
     * @brief Destructor - cleanup all rendering resources
     */
    ~GameClientRenderer();

    /// @name Initialization
    /// @{
    /**
     * @brief Initialize rendering system and create resources
     * @return True if initialization successful
     */
    bool initialize();

    /**
     * @brief Shutdown and cleanup all rendering resources
     */
    void shutdown();
    /// @}

    /// @name Rendering Operations
    /// @{
    /**
     * @brief Render complete frame with current scene state
     */
    void drawFrame();

    /**
     * @brief Update uniform buffer with current transformation matrices
     * @param currentImage Current swap chain image index
     */
    void updateUniformBuffer(uint32_t currentImage);

    /**
     * @brief Recreate swap chain after window resize or format change
     */
    void recreateSwapChain();
    /// @}

    /// @name Pipeline Management
    /// @{
    /**
     * @brief Recreate graphics pipeline with current settings
     */
    void recreateGraphicsPipeline();

    /**
     * @brief Set wireframe rendering mode
     * @param enabled True to enable wireframe mode
     */
    void setWireframeMode(bool enabled) { m_wireframeMode = enabled; }

    /**
     * @brief Check if wireframe mode is active
     * @return True if wireframe mode enabled
     */
    bool isWireframeMode() const { return m_wireframeMode; }
    /// @}

    /// @name Resource Accessors
    /// @{
    /**
     * @brief Get main render pass handle
     * @return Vulkan render pass
     */
    VkRenderPass getRenderPass() const { return m_renderPass; }

    /**
     * @brief Get pipeline layout for push constants and descriptors
     * @return Vulkan pipeline layout
     */
    VkPipelineLayout getPipelineLayout() const { return m_pipelineLayout; }

    /**
     * @brief Get solid rendering graphics pipeline
     * @return Vulkan graphics pipeline for solid rendering
     */
    VkPipeline getGraphicsPipeline() const { return m_graphicsPipeline; }

    /**
     * @brief Get wireframe rendering graphics pipeline
     * @return Vulkan graphics pipeline for wireframe rendering
     */
    VkPipeline getWireframePipeline() const { return m_wireframePipeline; }

    /**
     * @brief Get command pool for command buffer allocation
     * @return Vulkan command pool
     */
    VkCommandPool getCommandPool() const { return m_commandPool; }

    /**
     * @brief Get command buffers for frame recording
     * @return Vector of command buffers
     */
    const std::vector<VkCommandBuffer>& getCommandBuffers() const { return m_commandBuffers; }

    /**
     * @brief Get descriptor sets for uniform binding
     * @return Vector of descriptor sets
     */
    const std::vector<VkDescriptorSet>& getDescriptorSets() const { return m_descriptorSets; }
    /// @}

    /// @name Renderer Integration
    /// @{
    /**
     * @brief Get low-level Vulkan renderer instance
     * @return Pointer to VulkanRenderer or nullptr
     */
    VulkanRenderer* getVulkanRenderer() const { return m_renderer.get(); }

    /**
     * @brief Get swap chain render pass from underlying renderer
     * @return Vulkan render pass for swap chain or VK_NULL_HANDLE
     */
    VkRenderPass getSwapChainRenderPass() const { return m_renderer ? m_renderer->getSwapChainRenderPass() : VK_NULL_HANDLE; }
    /// @}

private:
    GameClient& m_gameClient;                           ///< Reference to main game client
    GLFWwindow* m_window;                               ///< GLFW window handle
    VulkanDevice& m_device;                             ///< Vulkan device abstraction
    std::unique_ptr<VulkanRenderer> m_renderer;         ///< Low-level Vulkan renderer
    TextureManager* m_textureManager = nullptr;         ///< Texture atlas manager

    /// @name Vulkan Pipeline Resources
    /// @{
    VkRenderPass m_renderPass = VK_NULL_HANDLE;             ///< Main render pass
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE; ///< Descriptor set layout
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;     ///< Pipeline layout for uniforms
    VkPipeline m_graphicsPipeline = VK_NULL_HANDLE;         ///< Solid rendering pipeline
    VkPipeline m_wireframePipeline = VK_NULL_HANDLE;        ///< Wireframe rendering pipeline
    VkCommandPool m_commandPool = VK_NULL_HANDLE;           ///< Command pool for buffers
    /// @}

    /// @name Vertex Data
    /// @{
    std::vector<Vertex> m_vertices;                     ///< CPU vertex data
    std::vector<uint32_t> m_indices;                    ///< CPU index data
    std::unique_ptr<VulkanBuffer> m_vertexBuffer;       ///< GPU vertex buffer
    std::unique_ptr<VulkanBuffer> m_indexBuffer;        ///< GPU index buffer
    /// @}

    /// @name Uniform and Descriptor Resources
    /// @{
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;      ///< Double buffering
    std::vector<std::unique_ptr<VulkanBuffer>> m_uniformBuffers; ///< Per-frame uniform buffers
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE; ///< Descriptor pool for sets
    std::vector<VkDescriptorSet> m_descriptorSets;      ///< Descriptor sets for uniforms
    std::vector<VkCommandBuffer> m_commandBuffers;      ///< Command buffers for recording
    /// @}

    /// @name Rendering State
    /// @{
    bool m_wireframeMode = false;                       ///< Wireframe rendering toggle
    /// @}

    /// @name Private Implementation
    /// @{
    /**
     * @brief Create main render pass for scene rendering
     */
    void createRenderPass();

    /**
     * @brief Create descriptor set layout for uniforms and textures
     */
    void createDescriptorSetLayout();

    /**
     * @brief Create solid rendering graphics pipeline
     */
    void createGraphicsPipeline();

    /**
     * @brief Create wireframe rendering graphics pipeline
     */
    void createWireframePipeline();

    /**
     * @brief Create framebuffers for swap chain images
     */
    void createFramebuffers();

    /**
     * @brief Create command pool for command buffer allocation
     */
    void createCommandPool();

    /**
     * @brief Create and upload vertex buffer data
     */
    void createVertexBuffer();

    /**
     * @brief Create and upload index buffer data
     */
    void createIndexBuffer();

    /**
     * @brief Create uniform buffers for all frames in flight
     */
    void createUniformBuffers();

    /**
     * @brief Create descriptor pool for descriptor set allocation
     */
    void createDescriptorPool();

    /**
     * @brief Create descriptor sets and bind resources
     */
    void createDescriptorSets();

    /**
     * @brief Create and allocate command buffers
     */
    void createCommandBuffers();

    /**
     * @brief Create Vulkan shader module from SPIR-V bytecode
     * @param code SPIR-V shader bytecode
     * @return Created shader module handle
     */
    VkShaderModule createShaderModule(const std::vector<char>& code);

    /**
     * @brief Cleanup swap chain dependent resources
     */
    void cleanupSwapChain();

    /**
     * @brief Cleanup all Vulkan rendering resources
     */
    void cleanupVulkan();
    /// @}
};
};