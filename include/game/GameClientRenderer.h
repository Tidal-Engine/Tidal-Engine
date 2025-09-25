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

class GameClientRenderer {
public:
    GameClientRenderer(GameClient& gameClient, GLFWwindow* window, VulkanDevice& device);
    ~GameClientRenderer();

    // Initialization
    bool initialize();
    void shutdown();

    // Rendering
    void drawFrame();
    void updateUniformBuffer(uint32_t currentImage);
    void recreateSwapChain();

    // Pipeline management
    void recreateGraphicsPipeline();
    void setWireframeMode(bool enabled) { m_wireframeMode = enabled; }
    bool isWireframeMode() const { return m_wireframeMode; }

    // Getters
    VkRenderPass getRenderPass() const { return m_renderPass; }
    VkPipelineLayout getPipelineLayout() const { return m_pipelineLayout; }
    VkPipeline getGraphicsPipeline() const { return m_graphicsPipeline; }
    VkPipeline getWireframePipeline() const { return m_wireframePipeline; }
    VkCommandPool getCommandPool() const { return m_commandPool; }
    const std::vector<VkCommandBuffer>& getCommandBuffers() const { return m_commandBuffers; }
    const std::vector<VkDescriptorSet>& getDescriptorSets() const { return m_descriptorSets; }

    // Access to underlying renderer
    VulkanRenderer* getVulkanRenderer() const { return m_renderer.get(); }
    VkRenderPass getSwapChainRenderPass() const { return m_renderer ? m_renderer->getSwapChainRenderPass() : VK_NULL_HANDLE; }

private:
    GameClient& m_gameClient;
    GLFWwindow* m_window;
    VulkanDevice& m_device;
    std::unique_ptr<VulkanRenderer> m_renderer;
    TextureManager* m_textureManager = nullptr;

    // Rendering pipeline
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_graphicsPipeline = VK_NULL_HANDLE;
    VkPipeline m_wireframePipeline = VK_NULL_HANDLE;
    VkCommandPool m_commandPool = VK_NULL_HANDLE;

    // Vertex data
    std::vector<Vertex> m_vertices;
    std::vector<uint32_t> m_indices;
    std::unique_ptr<VulkanBuffer> m_vertexBuffer;
    std::unique_ptr<VulkanBuffer> m_indexBuffer;

    // Uniform buffers
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
    std::vector<std::unique_ptr<VulkanBuffer>> m_uniformBuffers;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> m_descriptorSets;
    std::vector<VkCommandBuffer> m_commandBuffers;

    // State
    bool m_wireframeMode = false;

    // Initialization methods
    void createRenderPass();
    void createDescriptorSetLayout();
    void createGraphicsPipeline();
    void createWireframePipeline();
    void createFramebuffers();
    void createCommandPool();
    void createVertexBuffer();
    void createIndexBuffer();
    void createUniformBuffers();
    void createDescriptorPool();
    void createDescriptorSets();
    void createCommandBuffers();

    // Utility methods
    VkShaderModule createShaderModule(const std::vector<char>& code);
    void cleanupSwapChain();

    // Cleanup
    void cleanupVulkan();
};