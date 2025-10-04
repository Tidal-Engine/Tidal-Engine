#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

#include "vulkan/Vertex.hpp"
#include "core/EngineConfig.hpp"
#include "core/PerformanceMetrics.hpp"
#include "client/Raycaster.hpp"
#include "shared/Chunk.hpp"
#include "shared/ChunkCoord.hpp"

#include <vector>
#include <set>
#include <memory>
#include <cstdint>
#include <optional>
#include <queue>
#include <mutex>
#include <future>

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
class BlockOutlineRenderer;

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
    std::unique_ptr<BlockOutlineRenderer> blockOutlineRenderer;

    EngineConfig::Runtime config;
    PerformanceMetrics performanceMetrics;

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    bool framebufferResized = false;
    float deltaTime = 0.0f;
    std::chrono::steady_clock::time_point lastFrameTime;

    // Raycasting
    std::optional<RaycastHit> targetedBlock;

    // Position update throttling
    std::chrono::steady_clock::time_point lastPositionUpdate;
    glm::vec3 lastSentPosition;

    // Block breaking state
    std::chrono::steady_clock::time_point lastBlockBreak;
    bool wasLeftClickPressed = false;  // Track previous frame's button state
    static constexpr float BLOCK_BREAK_COOLDOWN = 0.25f;  // seconds between breaks when holding

    // ImGui resources
    VkDescriptorPool imguiDescriptorPool = VK_NULL_HANDLE;

    // Async chunk loading
    struct PendingChunk {
        ChunkCoord coord{0, 0, 0};
        Chunk chunk{ChunkCoord{0, 0, 0}};  // Copy of chunk data for async processing
        Chunk neighborNegX{ChunkCoord{0, 0, 0}};
        Chunk neighborPosX{ChunkCoord{0, 0, 0}};
        Chunk neighborNegY{ChunkCoord{0, 0, 0}};
        Chunk neighborPosY{ChunkCoord{0, 0, 0}};
        Chunk neighborNegZ{ChunkCoord{0, 0, 0}};
        Chunk neighborPosZ{ChunkCoord{0, 0, 0}};
        bool hasNegX = false;
        bool hasPosX = false;
        bool hasNegY = false;
        bool hasPosY = false;
        bool hasNegZ = false;
        bool hasPosZ = false;
    };

    struct CompletedMesh {
        ChunkCoord coord;
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
    };

    std::queue<PendingChunk> pendingChunks;
    std::queue<CompletedMesh> completedMeshes;
    std::mutex pendingChunksMutex;
    std::mutex completedMeshesMutex;
    std::vector<std::future<void>> meshGenerationTasks;

    static constexpr size_t MAX_CHUNKS_PER_FRAME = 10;

    void processPendingChunks();
    void uploadCompletedMeshes();

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
