#pragma once

#include "VulkanDevice.h"
#include "VulkanRenderer.h"
#include "VulkanBuffer.h"
#include "Camera.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <memory>
#include <vector>

struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 texCoord;

    static VkVertexInputBindingDescription getBindingDescription();
    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions();
};

struct UniformBufferObject {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

struct PushConstants {
    alignas(16) glm::vec3 lightPos;
    alignas(16) glm::vec3 lightColor;
    alignas(16) glm::vec3 viewPos;
};

class Application {
public:
    Application();
    ~Application();

    void run();

private:
    void initWindow();
    void initVulkan();
    void createRenderPass();
    void createDescriptorSetLayout();
    void createGraphicsPipeline();
    void createFramebuffers();
    void createCommandPool();
    void createTextureImage();
    void createTextureImageView();
    void createTextureSampler();
    void createVertexBuffer();
    void createIndexBuffer();
    void createUniformBuffers();
    void createDescriptorPool();
    void createDescriptorSets();
    void createCommandBuffers();

    VkShaderModule createShaderModule(const std::vector<char>& code);
    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);

    void mainLoop();
    void drawFrame();
    void updateUniformBuffer(uint32_t currentImage);
    void recreateSwapChain();
    void cleanupSwapChain();

    void processInput(float deltaTime);
    void updateTransforms(float deltaTime);

    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void mouseCallback(GLFWwindow* window, double xpos, double ypos);
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);

    // Window properties
    static constexpr uint32_t WIDTH = 800;
    static constexpr uint32_t HEIGHT = 600;
    const std::string WINDOW_TITLE = "Tidal Engine - Vulkan Cube Demo";

    GLFWwindow* m_window;
    std::unique_ptr<VulkanDevice> m_device;
    std::unique_ptr<VulkanRenderer> m_renderer;

    VkRenderPass m_renderPass;
    VkDescriptorSetLayout m_descriptorSetLayout;
    VkPipelineLayout m_pipelineLayout;
    VkPipeline m_graphicsPipeline;

    VkCommandPool m_commandPool;

    VkImage m_textureImage;
    VkDeviceMemory m_textureImageMemory;
    VkImageView m_textureImageView;
    VkSampler m_textureSampler;

    std::vector<Vertex> m_vertices;
    std::vector<uint32_t> m_indices;
    std::unique_ptr<VulkanBuffer> m_vertexBuffer;
    std::unique_ptr<VulkanBuffer> m_indexBuffer;

    std::vector<std::unique_ptr<VulkanBuffer>> m_uniformBuffers;
    std::vector<void*> m_uniformBuffersMapped;

    VkDescriptorPool m_descriptorPool;
    std::vector<VkDescriptorSet> m_descriptorSets;

    std::vector<VkCommandBuffer> m_commandBuffers;

    bool m_framebufferResized = false;

    // Camera and input
    Camera m_camera;
    float m_lastX = WIDTH / 2.0f;
    float m_lastY = HEIGHT / 2.0f;
    bool m_firstMouse = true;
    bool m_keys[1024] = {};

    // Animation
    float m_lastFrameTime = 0.0f;

    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
};