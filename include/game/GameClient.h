#pragma once

#include "vulkan/VulkanDevice.h"
#include "vulkan/VulkanRenderer.h"
#include "vulkan/VulkanBuffer.h"
#include "core/Camera.h"
#include "network/NetworkProtocol.h"
#include "core/ThreadPool.h"
#include "game/Chunk.h"
#include "game/SaveSystem.h"
#include "game/ClientChunkManager.h"
#include "graphics/TextureManager.h"
#include "system/UserDataManager.h"

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
class GameClientRenderer;
class GameClientInput;
class GameClientNetwork;
class GameClientUI;

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

// Debug information structure
struct DebugInfo {
    uint32_t drawCalls = 0;
    uint32_t renderedVoxels = 0;
    uint32_t totalVoxels = 0;
    uint32_t culledVoxels = 0;
    float frameTime = 0.0f;
    float fps = 0.0f;
};

class GameClient {
    friend class GameClientRenderer; // Allow renderer access to private members
public:
    GameClient(const ClientConfig& config = ClientConfig{});
    ~GameClient();

    // Client lifecycle
    bool initialize();
    void run();
    void shutdown();

    // Connection management
    // connectToServer now handled by GameClientNetwork
    void connectToLocalServer(GameServer* localServer);
    bool startSingleplayer(const std::string& worldName = "default");
    void disconnect();

    // World management
    void initializeSaveDirectory();
    void scanForWorlds();
    bool createNewWorld(const std::string& worldName);
    bool isValidWorld(const std::string& worldPath) const;
    std::string getSaveDirectory() const;

    // Game mode transitions
    void startSingleplayerGame(const std::string& worldName = "default");
    void saveAndQuitToTitle();

    // World access for UI
    const std::vector<std::string>& getAvailableWorlds() const { return m_availableWorlds; }

    // Client state
    ClientState getState() const { return m_state; }
    bool isConnected() const { return m_state == ClientState::CONNECTED || m_state == ClientState::IN_GAME; }

    // Game state
    GameState getGameState() const { return m_gameState; }
    void setGameState(GameState state) { m_gameState = state; }
    void requestQuit() { m_running = false; }

    // Rendering system access
    GameClientRenderer* getRenderer() { return m_renderer.get(); }

    // Input system access
    GameClientInput* getInput() { return m_input.get(); }

    // Network system access
    GameClientNetwork* getNetwork() { return m_network.get(); }

    // UI system access
    GameClientUI* getUI() { return m_ui.get(); }


    // Input-related methods (called by GameClientInput)
    int getSelectedHotbarSlot() const { return m_selectedHotbarSlot; }
    void setSelectedHotbarSlot(int slot) { m_selectedHotbarSlot = slot; }
    // Input handling methods now in GameClientInput
    void toggleWireframeMode() { m_wireframeMode = !m_wireframeMode; }

    // Network-related methods (called by GameClientNetwork)
    void setState(ClientState state) { m_state = state; }
    GameServer* getLocalServer() const { return m_localServer; }
    PlayerId getLocalPlayerId() const { return m_localPlayerId; }
    void setLocalPlayerId(PlayerId id) { m_localPlayerId = id; }
    Camera& getCamera() { return m_camera; }
    ClientChunkManager* getChunkManager() const { return m_chunkManager.get(); }

    // UI/Debug accessors
    const DebugInfo& getDebugInfo() const { return m_debugInfo; }
    bool isWireframeMode() const { return m_wireframeMode; }

    // Hotbar accessors
    static constexpr int getHotbarSlots() { return HOTBAR_SLOTS; }
    const BlockType* getHotbarBlocks() const { return m_hotbarBlocks; }
    TextureManager* getTextureManager() const { return m_textureManager.get(); }
    void setSelectedHotbarSlotFromUI(int slot) { m_selectedHotbarSlot = slot; }

    // Network message handlers (called by GameClientNetwork)
    void handlePlayerUpdate(const PlayerUpdateMessage& msg);
    void handleChunkData(const ChunkDataMessage& msg, const std::vector<uint8_t>& compressedData);

    // Rendering now handled by GameClientRenderer

    // Game events
    void onBlockPlace(const glm::ivec3& position, BlockType blockType);
    void onBlockBreak(const glm::ivec3& position);
    void onPlayerMove();

    // Raycast helper for block interaction
    BlockHitResult raycastVoxelsClient(const Ray& ray, ClientChunkManager* chunkManager, float maxDistance);

    // Network message handling (delegated to network system)
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

    // Rendering (delegated to GameClientRenderer)
    std::unique_ptr<GameClientRenderer> m_renderer;

    // Input system (delegated to GameClientInput)
    std::unique_ptr<GameClientInput> m_input;

    // Network system (delegated to GameClientNetwork)
    std::unique_ptr<GameClientNetwork> m_network;

    // UI system (delegated to GameClientUI)
    std::unique_ptr<GameClientUI> m_ui;


    // Camera
    Camera m_camera;

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
    DebugInfo m_debugInfo;

    // UI state (wireframe mode kept in GameClient for renderer access)
    bool m_wireframeMode = false;

    // Networking (managed by GameClientNetwork)
    GameServer* m_localServer = nullptr;  // For singleplayer mode
    std::unique_ptr<GameServer> m_integratedServer;  // Integrated server for singleplayer

    // Threading
    std::unique_ptr<std::thread> m_networkThread;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_cleanedUp{false};

    // World management (UI state moved to GameClientUI)
    std::vector<std::string> m_availableWorlds;
    std::string m_saveDirectory;

    // Texture and user data management
    std::unique_ptr<UserDataManager> m_userDataManager;
    std::unique_ptr<TextureManager> m_textureManager;

    // Initialization methods
    void initWindow();
    void initVulkan();

    // Rendering methods (delegated to GameClientRenderer)
    // Rendering methods now in GameClientRenderer and GameClientUI

    // Input methods
    void updateTransforms(float deltaTime);
    void loadChunksAroundPlayer();


    // Message handlers
    void handleBlockUpdate(const BlockUpdateMessage& msg);

    // Cleanup
    void cleanupVulkan();

    // Static callbacks
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
};

// ClientChunkManager is now in separate files

// ClientChunk is now in separate files