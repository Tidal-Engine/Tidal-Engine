#pragma once

#include "VulkanDevice.h"
#include "VulkanRenderer.h"
#include "VulkanBuffer.h"
#include "Camera.h"
#include "NetworkProtocol.h"
#include "ThreadPool.h"
#include "Chunk.h"
#include "SaveSystem.h"
#include "TextureManager.h"
#include "UserDataManager.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <memory>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <filesystem>
#include <string>
#include <array>

// Forward declarations
class GameServer;
class ClientChunkManager;
class NetworkClient;

// Vertex structure for rendering
struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 texCoord;

    static VkVertexInputBindingDescription getBindingDescription();
    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions();
};

// Uniform Buffer Object for shaders
struct UniformBufferObject {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

// Push constants for shaders
struct PushConstants {
    alignas(16) glm::vec3 lightPos;
    alignas(16) glm::vec3 lightColor;
    alignas(16) glm::vec3 viewPos;
    alignas(16) glm::vec3 voxelPosition;
    alignas(16) glm::vec3 voxelColor;
};

// Client configuration
struct ClientConfig {
    std::string serverAddress = "localhost";
    uint16_t serverPort = 25565;
    std::string playerName = "Player";
    uint32_t renderDistance = 8;
    bool vsync = false;  // Disable VSync for maximum performance
    bool fullscreen = false;
    uint32_t windowWidth = 1200;
    uint32_t windowHeight = 800;
    float mouseSensitivity = 0.1f;
    float movementSpeed = 2.5f;
};

// Client state for networking
enum class ClientState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    IN_GAME,
    DISCONNECTING
};

// Game state for menu system
enum class GameState {
    MAIN_MENU,
    WORLD_SELECTION,
    CONNECTING,
    LOADING,
    IN_GAME,
    PAUSED,
    SETTINGS,
    QUIT
};

// Client-side player representation
struct ClientPlayer {
    PlayerId id;
    std::string name;
    glm::vec3 position;
    float yaw;
    float pitch;
    bool isLocalPlayer = false;
};

class GameClient {
public:
    GameClient(const ClientConfig& config = ClientConfig{});
    ~GameClient();

    // Client lifecycle
    bool initialize();
    void run();
    void shutdown();

    // Connection management
    bool connectToServer(const std::string& address, uint16_t port);
    void connectToLocalServer(GameServer* localServer);
    bool startSingleplayer(const std::string& worldName = "default");
    void disconnect();

    // Game mode transitions
    void startSingleplayerGame(const std::string& worldName = "default");

    // World management
    void initializeSaveDirectory();
    void scanForWorlds();
    bool createNewWorld(const std::string& worldName);
    bool isValidWorld(const std::string& worldPath) const;
    std::string getSaveDirectory() const;

    // Client state
    ClientState getState() const { return m_state; }
    bool isConnected() const { return m_state == ClientState::CONNECTED || m_state == ClientState::IN_GAME; }

    // Game state
    GameState getGameState() const { return m_gameState; }
    void setGameState(GameState state) { m_gameState = state; }

    // Input handling
    void processInput(float deltaTime);
    void handleMouseInput(double xpos, double ypos);
    void handleKeyInput(int key, int action, int mods);
    void handleMouseButton(int button, int action, int mods);

    // Rendering
    void render(float deltaTime);

    // Game events
    void onBlockPlace(const glm::ivec3& position, BlockType blockType);
    void onBlockBreak(const glm::ivec3& position);
    void onPlayerMove();

    // Network message handling
    void processServerMessage(const NetworkPacket& packet);
    void handleNetworkMessage(const NetworkPacket& packet) { processServerMessage(packet); }

    // Configuration
    const ClientConfig& getConfig() const { return m_config; }
    void setConfig(const ClientConfig& config) { m_config = config; }

private:
    ClientConfig m_config;
    std::atomic<ClientState> m_state{ClientState::DISCONNECTED};
    std::atomic<GameState> m_gameState{GameState::MAIN_MENU};

    // Vulkan/Graphics
    GLFWwindow* m_window = nullptr;
    std::unique_ptr<VulkanDevice> m_device;
    std::unique_ptr<VulkanRenderer> m_renderer;

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

    // Camera and input
    Camera m_camera;
    float m_lastX = 0.0f;
    float m_lastY = 0.0f;
    bool m_firstMouse = true;
    bool m_keys[1024] = {};
    bool m_mouseCaptured = true;

    // Timing
    float m_lastFrameTime = 0.0f;
    float m_deltaTime = 0.0f;

    // Game state
    std::unique_ptr<ClientChunkManager> m_chunkManager;
    std::unordered_map<PlayerId, ClientPlayer> m_players;
    PlayerId m_localPlayerId = INVALID_PLAYER_ID;
    mutable std::mutex m_playerMutex;

    // UI state
    int m_selectedHotbarSlot = 0;
    static constexpr int HOTBAR_SLOTS = 8;
    BlockType m_hotbarBlocks[HOTBAR_SLOTS] = {
        BlockType::STONE, BlockType::DIRT, BlockType::GRASS, BlockType::WOOD,
        BlockType::SAND, BlockType::BRICK, BlockType::COBBLESTONE, BlockType::SNOW
    };

    // Debug info
    struct DebugInfo {
        uint32_t drawCalls = 0;
        uint32_t renderedVoxels = 0;
        uint32_t totalVoxels = 0;
        uint32_t culledVoxels = 0;
        float frameTime = 0.0f;
        float fps = 0.0f;
    } m_debugInfo;

    // ImGui
    VkDescriptorPool m_imguiDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_atlasTextureDescriptor = VK_NULL_HANDLE;
    bool m_showDebugWindow = false;
    bool m_showPerformanceHUD = false;
    bool m_showRenderingHUD = false;
    bool m_showCameraHUD = false;
    bool m_wireframeMode = false;
    bool m_showChunkBoundaries = false;

    // Networking
    GameServer* m_localServer = nullptr;  // For singleplayer mode
    std::unique_ptr<GameServer> m_integratedServer;  // Integrated server for singleplayer
    std::unique_ptr<NetworkClient> m_networkClient;
    MessageQueue<NetworkPacket> m_incomingMessages;
    MessageQueue<NetworkPacket> m_outgoingMessages;

    // Threading
    std::unique_ptr<std::thread> m_networkThread;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_cleanedUp{false};

    // World management
    std::vector<std::string> m_availableWorlds;
    std::string m_saveDirectory;
    char m_newWorldName[64] = {};
    bool m_showCreateWorldDialog = false;

    // Texture and user data management
    std::unique_ptr<UserDataManager> m_userDataManager;
    std::unique_ptr<TextureManager> m_textureManager;

    // Initialization methods
    void initWindow();
    void initVulkan();
    void initImGui();
    void createRenderPass();
    void createDescriptorSetLayout();
    void createGraphicsPipeline();
    void createWireframePipeline();
    void recreateGraphicsPipeline();
    void createFramebuffers();
    void createCommandPool();
    void createVertexBuffer();
    void createIndexBuffer();
    void createUniformBuffers();
    void createDescriptorPool();
    void createDescriptorSets();
    void createCommandBuffers();
    void createAtlasTextureDescriptor();

    // Utility methods
    VkShaderModule createShaderModule(const std::vector<char>& code);
    void recreateSwapChain();
    void cleanupSwapChain();

    // Rendering methods
    void drawFrame();
    void updateUniformBuffer(uint32_t currentImage);
    void renderImGui(VkCommandBuffer commandBuffer);
    void renderMainMenu();
    void renderWorldSelection();
    void renderLoadingScreen();
    void renderPauseMenu();
    void renderDebugWindow();
    void renderPerformanceHUD();
    void renderRenderingHUD();
    void renderCameraHUD();
    void renderCrosshair();
    void renderHotbar();

    // Input methods
    void updateTransforms(float deltaTime);
    void handleBlockInteraction(bool isBreaking);
    void loadChunksAroundPlayer();

    // Raycast helper for ClientChunkManager
    BlockHitResult raycastVoxelsClient(const Ray& ray, ClientChunkManager* chunkManager, float maxDistance);

    // Network methods
    void sendToServer(const NetworkPacket& packet);
    void processIncomingMessages();
    void networkThreadFunc();

    // Message handlers
    void handleServerHello(const ServerHelloMessage& msg);
    void handlePlayerUpdate(const PlayerUpdateMessage& msg);
    void handleBlockUpdate(const BlockUpdateMessage& msg);
    void handleChunkData(const ChunkDataMessage& msg, const std::vector<uint8_t>& packetPayload);

    // Cleanup
    void cleanupImGui();
    void cleanupVulkan();

    // Static callbacks
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void mouseCallback(GLFWwindow* window, double xpos, double ypos);
    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
};

// Client-side chunk manager (only handles rendering)
class ClientChunkManager {
public:
    ClientChunkManager(VulkanDevice& device, TextureManager* textureManager = nullptr);
    ~ClientChunkManager() = default;

    // Chunk rendering
    size_t renderVisibleChunks(VkCommandBuffer commandBuffer, const Frustum& frustum);
    void renderChunkBoundaries(VkCommandBuffer commandBuffer, VkPipeline wireframePipeline, const glm::vec3& playerPosition, VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet);

    // Chunk data from server
    void setChunkData(const ChunkPos& pos, const ChunkData& data);
    void removeChunk(const ChunkPos& pos);

    // Block updates from server
    void updateBlock(const glm::ivec3& worldPos, BlockType blockType);

    // Utility
    size_t getLoadedChunkCount() const;
    size_t getTotalVertexCount() const;
    size_t getTotalFaceCount() const;
    bool hasChunk(const ChunkPos& pos) const;
    bool isVoxelSolidAtWorldPosition(int worldX, int worldY, int worldZ) const;

    // Internal lock-free version for use when mutex is already held
    bool isVoxelSolidAtWorldPositionUnsafe(int worldX, int worldY, int worldZ) const;
    std::vector<ChunkPos> getLoadedChunkPositions() const;
    void updateDirtyMeshes();
    void updateDirtyMeshesSafe(); // Process deferred mesh updates safely between frames

    // Frame state tracking for safe mesh updates
    void setFrameInProgress(bool inProgress) { m_isFrameInProgress = inProgress; }

private:
    VulkanDevice& m_device;
    TextureManager* m_textureManager;
    std::unordered_map<ChunkPos, std::unique_ptr<class ClientChunk>> m_chunks;
    mutable std::mutex m_chunkMutex;

    // Deferred mesh update system
    std::vector<ChunkPos> m_deferredMeshUpdates;
    bool m_isFrameInProgress = false;

    // Debug wireframe rendering
    std::unique_ptr<VulkanBuffer> m_wireframeVertexBuffer;
    std::unique_ptr<VulkanBuffer> m_wireframeIndexBuffer;
    std::vector<ChunkVertex> m_wireframeVertices;
    std::vector<uint32_t> m_wireframeIndices;
    bool m_wireframeDirty = true;

    // Helper functions
    void generateWireframeMesh(const glm::vec3& playerPosition);
    void createWireframeBuffers();
    void invalidateNeighboringChunks(const ChunkPos& chunkPos, int localX, int localY, int localZ);
    bool isAABBInFrustum(const AABB& aabb, const Frustum& frustum) const;
    bool isChunkCompletelyOccluded(const ChunkPos& pos) const;
};

// Client-side chunk (rendering only)
class ClientChunk {
public:
    ClientChunk(const ChunkPos& position, VulkanDevice& device, TextureManager* textureManager = nullptr, ClientChunkManager* chunkManager = nullptr);
    ~ClientChunk() = default;

    // Rendering
    void render(VkCommandBuffer commandBuffer);
    bool isEmpty() const { return m_vertices.empty(); }
    size_t getVertexCount() const { return m_vertices.size(); }
    size_t getIndexCount() const { return m_indices.size(); }

    // Data management
    void setChunkData(const ChunkData& data);
    void updateBlock(int x, int y, int z, BlockType blockType);
    void regenerateMesh();
    bool isMeshDirty() const { return m_meshDirty; }

    // Position
    const ChunkPos& getPosition() const { return m_position; }

    // Voxel access
    bool isVoxelSolid(int x, int y, int z) const;
    BlockType getVoxelType(int x, int y, int z) const;

    // Modification tracking for occlusion culling
    bool isModified() const { return m_modified; }
    void markAsModified() { m_modified = true; }

private:
    ChunkPos m_position;
    VulkanDevice& m_device;
    TextureManager* m_textureManager;
    ClientChunkManager* m_chunkManager;

    // Rendering data only (no game logic)
    std::vector<ChunkVertex> m_vertices;
    std::vector<uint32_t> m_indices;
    std::unique_ptr<VulkanBuffer> m_vertexBuffer;
    std::unique_ptr<VulkanBuffer> m_indexBuffer;
    bool m_meshDirty = true;
    bool m_modified = false;  // Track if chunk has been modified by player

    // Voxel data for mesh generation
    BlockType m_voxels[CHUNK_SIZE][CHUNK_SIZE][CHUNK_SIZE];

    // Mesh generation
    void generateMesh();
    void addVoxelFace(int x, int y, int z, int faceDir, BlockType blockType);
    bool shouldRenderFace(int x, int y, int z, int faceDir) const;
    void createBuffers();
};