#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

#include "vulkan/Vertex.hpp"
#include "core/EngineConfig.hpp"
#include "core/PerformanceMetrics.hpp"

#include <vector>
#include <set>
#include <memory>
#include <cstdint>

namespace engine {

// Forward declarations
class VulkanBuffer;
class VulkanSwapchain;
class VulkanPipeline;
class VulkanRenderer;
class NetworkClient;
class ChunkRenderer;
class InputManager;
class Camera;
class TextureAtlas;
class DebugOverlay;

/**
 * @brief Uniform buffer object for shader uniforms
 *
 * Contains all the data needed to render a frame, including transformation
 * matrices and lighting information. Aligned for GPU buffer requirements.
 * Uses vec4 for positions to ensure proper alignment and avoid wasted padding.
 */
struct UniformBufferObject {
    alignas(16) glm::mat4 model;     ///< Model transformation matrix
    alignas(16) glm::mat4 view;      ///< View transformation matrix
    alignas(16) glm::mat4 proj;      ///< Projection transformation matrix
    alignas(16) glm::vec4 lightPos;  ///< Light position in world space (w unused)
    alignas(16) glm::vec4 viewPos;   ///< Camera position in world space (w unused)
};

/**
 * @brief Main engine class that orchestrates the rendering pipeline
 *
 * The VulkanEngine class is the central component that initializes and manages
 * all Vulkan subsystems, including the swapchain, pipeline, buffers, and renderer.
 * It follows a modular design where each subsystem is encapsulated in its own class.
 */
class VulkanEngine {
public:
    VulkanEngine();
    ~VulkanEngine();

    // Delete copy operations (Vulkan resources shouldn't be copied)
    VulkanEngine(const VulkanEngine&) = delete;
    VulkanEngine& operator=(const VulkanEngine&) = delete;

    // Delete move operations (singleton-like engine)
    VulkanEngine(VulkanEngine&&) = delete;
    VulkanEngine& operator=(VulkanEngine&&) = delete;

    /**
     * @brief Start the main engine loop
     */
    void run();

private:
    // Core Vulkan objects
    SDL_Window* window = nullptr;
    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;

    // Subsystems
    std::unique_ptr<VulkanBuffer> bufferManager;
    std::unique_ptr<VulkanSwapchain> swapchain;
    std::unique_ptr<VulkanPipeline> pipeline;
    std::unique_ptr<VulkanRenderer> renderer;
    std::unique_ptr<NetworkClient> networkClient;
    std::unique_ptr<ChunkRenderer> chunkRenderer;
    std::unique_ptr<InputManager> inputManager;
    std::unique_ptr<Camera> camera;
    std::unique_ptr<TextureAtlas> textureAtlas;
    std::unique_ptr<DebugOverlay> debugOverlay;

    EngineConfig::Runtime config;
    PerformanceMetrics performanceMetrics;

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    bool framebufferResized = false;
    float deltaTime = 0.0f;
    std::chrono::steady_clock::time_point lastFrameTime;

    // ImGui resources
    VkDescriptorPool imguiDescriptorPool = VK_NULL_HANDLE;

    void initSDL();
    void initVulkan();
    void initGeometry();
    void initRenderingResources();
    void initImGui();
    void initNetworking();
    void recreateSwapchain();
    void cleanupImGui();

    void createInstance();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();

    void mainLoop();
    void cleanup();

    int rateDeviceSuitability(VkPhysicalDevice device);
    bool isDeviceSuitable(VkPhysicalDevice device);

    struct QueueFamilyIndices {
        uint32_t graphicsFamily = UINT32_MAX;
        uint32_t presentFamily = UINT32_MAX;

        bool isComplete() const {
            return graphicsFamily != UINT32_MAX && presentFamily != UINT32_MAX;
        }
    };

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
};

} // namespace engine
