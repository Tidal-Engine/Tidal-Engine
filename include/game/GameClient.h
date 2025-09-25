/**
 * @file GameClient.h
 * @brief Main game client with Vulkan rendering, networking, and UI systems
 * @author Tidal Engine Team
 * @version 1.0
 */

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

/**
 * @brief Vertex data structure for rendering pipeline
 */
struct Vertex {
    glm::vec3 pos;      ///< Vertex position in 3D space
    glm::vec3 normal;   ///< Vertex normal for lighting
    glm::vec2 texCoord; ///< Texture coordinates

    /**
     * @brief Get Vulkan vertex input binding description
     * @return Binding description for vertex buffer
     */
    static VkVertexInputBindingDescription getBindingDescription();

    /**
     * @brief Get Vulkan vertex attribute descriptions
     * @return Array of attribute descriptions for position, normal, texcoord
     */
    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions();
};

/**
 * @brief Uniform buffer object for shader transformations
 */
struct UniformBufferObject {
    alignas(16) glm::mat4 model;    ///< Model transformation matrix
    alignas(16) glm::mat4 view;     ///< View transformation matrix
    alignas(16) glm::mat4 proj;     ///< Projection transformation matrix
};

/**
 * @brief Push constants for shader lighting calculations
 */
struct PushConstants {
    alignas(16) glm::vec3 lightPos;     ///< Light source position
    alignas(16) glm::vec3 lightColor;   ///< Light color and intensity
    alignas(16) glm::vec3 viewPos;      ///< Camera/view position
    alignas(16) glm::vec3 voxelPosition;///< Current voxel position
    alignas(16) glm::vec3 voxelColor;   ///< Voxel color tint
};

/**
 * @brief Client configuration settings
 */
struct ClientConfig {
    std::string serverAddress = "localhost";   ///< Server hostname or IP
    uint16_t serverPort = 25565;               ///< Server port number
    std::string playerName = "Player";         ///< Player display name
    uint32_t renderDistance = 8;               ///< Chunk render distance
    bool vsync = false;                        ///< Enable VSync (disabled for performance)
    bool fullscreen = false;                   ///< Fullscreen mode toggle
    uint32_t windowWidth = 1200;               ///< Window width in pixels
    uint32_t windowHeight = 800;               ///< Window height in pixels
    float mouseSensitivity = 0.1f;             ///< Mouse look sensitivity
    float movementSpeed = 2.5f;                ///< Player movement speed
};

/**
 * @brief Network connection states
 */
enum class ClientState {
    DISCONNECTED,   ///< Not connected to any server
    CONNECTING,     ///< Attempting to connect
    CONNECTED,      ///< Connected but not in game
    IN_GAME,        ///< Actively playing in world
    DISCONNECTING   ///< Disconnecting from server
};

/**
 * @brief UI and game flow states
 */
enum class GameState {
    MAIN_MENU,      ///< Main menu screen
    WORLD_SELECTION,///< World selection screen
    CONNECTING,     ///< Connecting to server screen
    LOADING,        ///< Loading world data
    IN_GAME,        ///< Active gameplay
    PAUSED,         ///< Game paused (ESC menu)
    SETTINGS,       ///< Settings/options menu
    QUIT            ///< Quitting application
};

/**
 * @brief Client-side player representation
 */
struct ClientPlayer {
    PlayerId id;                ///< Unique player identifier
    std::string name;           ///< Player display name
    glm::vec3 position;         ///< Player world position
    float yaw;                  ///< Player yaw rotation
    float pitch;                ///< Player pitch rotation
    bool isLocalPlayer = false; ///< True if this is the local player
};

/**
 * @brief Performance and debug statistics
 */
struct DebugInfo {
    uint32_t drawCalls = 0;         ///< Number of draw calls per frame
    uint32_t renderedVoxels = 0;    ///< Voxels rendered this frame
    uint32_t totalVoxels = 0;       ///< Total voxels in loaded chunks
    uint32_t culledVoxels = 0;      ///< Voxels culled by frustum
    float frameTime = 0.0f;         ///< Frame time in milliseconds
    float fps = 0.0f;               ///< Frames per second
};

/**
 * @brief Main game client class coordinating all game systems
 *
 * GameClient is the central coordinator for the client-side game application.
 * It manages and integrates multiple subsystems:
 *
 * Core Systems:
 * - Vulkan rendering pipeline with modern graphics features
 * - Input handling for keyboard, mouse, and UI interactions
 * - Network communication with game servers
 * - User interface rendering with ImGui integration
 * - World persistence and save system integration
 *
 * Game Features:
 * - Single-player and multiplayer support
 * - Real-time voxel world rendering with frustum culling
 * - Block placement and destruction mechanics
 * - Player movement and camera controls
 * - Debug overlays and performance monitoring
 *
 * Architecture:
 * - Modular design with specialized subsystem classes
 * - Thread-safe operations for network and rendering
 * - Event-driven input and UI system
 * - Configuration-driven graphics and gameplay settings
 *
 * @see GameClientRenderer for rendering system
 * @see GameClientInput for input handling
 * @see GameClientNetwork for networking
 * @see GameClientUI for user interface
 * @see ClientChunkManager for world management
 */
class GameClient {
    friend class GameClientRenderer; ///< Allow renderer access to private members
public:
    /**
     * @brief Construct game client with configuration
     * @param config Client configuration settings
     */
    GameClient(const ClientConfig& config = ClientConfig{});
    /**
     * @brief Destructor - cleanup all systems
     */
    ~GameClient();

    /// @name Client Lifecycle
    /// @{
    /**
     * @brief Initialize all client systems
     * @return True if initialization successful
     */
    bool initialize();

    /**
     * @brief Run the main game loop
     */
    void run();

    /**
     * @brief Shutdown and cleanup all systems
     */
    void shutdown();
    /// @}

    /// @name Connection Management
    /// @{
    /**
     * @brief Connect to integrated local server (singleplayer)
     * @param localServer Pointer to local server instance
     * @note Network connections handled by GameClientNetwork
     */
    void connectToLocalServer(GameServer* localServer);

    /**
     * @brief Start singleplayer game with integrated server
     * @param worldName Name of world to load/create
     * @return True if singleplayer started successfully
     */
    bool startSingleplayer(const std::string& worldName = "default");

    /**
     * @brief Disconnect from current server
     */
    void disconnect();
    /// @}

    /// @name World Management
    /// @{
    /**
     * @brief Initialize save directory structure
     */
    void initializeSaveDirectory();

    /**
     * @brief Scan for available worlds in save directory
     */
    void scanForWorlds();

    /**
     * @brief Create new world with default settings
     * @param worldName Name for the new world
     * @return True if world created successfully
     */
    bool createNewWorld(const std::string& worldName);

    /**
     * @brief Validate world directory structure
     * @param worldPath Path to world directory
     * @return True if world is valid
     */
    bool isValidWorld(const std::string& worldPath) const;

    /**
     * @brief Get save directory path
     * @return Absolute path to save directory
     */
    std::string getSaveDirectory() const;
    /// @}

    /// @name Game Mode Transitions
    /// @{
    /**
     * @brief Transition to singleplayer game mode
     * @param worldName World to load for singleplayer
     */
    void startSingleplayerGame(const std::string& worldName = "default");

    /**
     * @brief Save current state and return to main menu
     */
    void saveAndQuitToTitle();
    /// @}

    /// @name World Access for UI
    /// @{
    /**
     * @brief Get list of available worlds for UI display
     * @return Reference to available worlds vector
     */
    const std::vector<std::string>& getAvailableWorlds() const { return m_availableWorlds; }
    /// @}

    /// @name Client State
    /// @{
    /**
     * @brief Get current network connection state
     * @return Current client state
     */
    ClientState getState() const { return m_state; }

    /**
     * @brief Check if client is connected to server
     * @return True if connected or in-game
     */
    bool isConnected() const { return m_state == ClientState::CONNECTED || m_state == ClientState::IN_GAME; }
    /// @}

    /// @name Game State
    /// @{
    /**
     * @brief Get current game flow state
     * @return Current game state
     */
    GameState getGameState() const { return m_gameState; }

    /**
     * @brief Set game flow state
     * @param state New game state to set
     */
    void setGameState(GameState state) { m_gameState = state; }

    /**
     * @brief Request application shutdown
     */
    void requestQuit() { m_running = false; }
    /// @}

    /// @name System Access
    /// @{
    /**
     * @brief Get rendering system instance
     * @return Pointer to renderer system
     */
    GameClientRenderer* getRenderer() { return m_renderer.get(); }

    /**
     * @brief Get input system instance
     * @return Pointer to input system
     */
    GameClientInput* getInput() { return m_input.get(); }

    /**
     * @brief Get network system instance
     * @return Pointer to network system
     */
    GameClientNetwork* getNetwork() { return m_network.get(); }

    /**
     * @brief Get UI system instance
     * @return Pointer to UI system
     */
    GameClientUI* getUI() { return m_ui.get(); }
    /// @}


    /// @name Input Interface (called by GameClientInput)
    /// @{
    /**
     * @brief Get currently selected hotbar slot
     * @return Selected hotbar slot index
     */
    int getSelectedHotbarSlot() const { return m_selectedHotbarSlot; }

    /**
     * @brief Set selected hotbar slot
     * @param slot Hotbar slot index to select
     */
    void setSelectedHotbarSlot(int slot) { m_selectedHotbarSlot = slot; }

    /**
     * @brief Toggle wireframe rendering mode
     */
    void toggleWireframeMode() { m_wireframeMode = !m_wireframeMode; }
    /// @}

    /// @name Network Interface (called by GameClientNetwork)
    /// @{
    /**
     * @brief Set client network state
     * @param state New client state
     */
    void setState(ClientState state) { m_state = state; }

    /**
     * @brief Get local server instance for singleplayer
     * @return Pointer to local server or nullptr
     */
    GameServer* getLocalServer() const { return m_localServer; }

    /**
     * @brief Get local player's unique identifier
     * @return Local player ID
     */
    PlayerId getLocalPlayerId() const { return m_localPlayerId; }

    /**
     * @brief Set local player's unique identifier
     * @param id Player ID to set
     */
    void setLocalPlayerId(PlayerId id) { m_localPlayerId = id; }

    /**
     * @brief Get camera reference for movement and rendering
     * @return Reference to main camera
     */
    Camera& getCamera() { return m_camera; }

    /**
     * @brief Get chunk manager for world data access
     * @return Pointer to chunk manager
     */
    ClientChunkManager* getChunkManager() const { return m_chunkManager.get(); }
    /// @}

    /// @name UI and Debug Accessors
    /// @{
    /**
     * @brief Get current debug and performance information
     * @return Reference to debug info structure
     */
    const DebugInfo& getDebugInfo() const { return m_debugInfo; }

    /**
     * @brief Check if wireframe rendering mode is active
     * @return True if wireframe mode enabled
     */
    bool isWireframeMode() const { return m_wireframeMode; }
    /// @}

    /// @name Hotbar Interface
    /// @{
    /**
     * @brief Get number of available hotbar slots
     * @return Number of hotbar slots
     */
    static constexpr int getHotbarSlots() { return HOTBAR_SLOTS; }

    /**
     * @brief Get array of block types in hotbar
     * @return Pointer to hotbar block array
     */
    const BlockType* getHotbarBlocks() const { return m_hotbarBlocks; }

    /**
     * @brief Get texture manager for rendering
     * @return Pointer to texture manager
     */
    TextureManager* getTextureManager() const { return m_textureManager.get(); }

    /**
     * @brief Set selected hotbar slot from UI interaction
     * @param slot Slot index to select
     */
    void setSelectedHotbarSlotFromUI(int slot) { m_selectedHotbarSlot = slot; }
    /// @}

    /// @name Network Message Handlers (called by GameClientNetwork)
    /// @{
    /**
     * @brief Handle player position/rotation update from server
     * @param msg Player update message
     */
    void handlePlayerUpdate(const PlayerUpdateMessage& msg);

    /**
     * @brief Handle chunk data received from server
     * @param msg Chunk data message header
     * @param compressedData Compressed chunk voxel data
     */
    void handleChunkData(const ChunkDataMessage& msg, const std::vector<uint8_t>& compressedData);
    /// @}

    /// @note Rendering operations handled by GameClientRenderer

    /// @name Game Events
    /// @{
    /**
     * @brief Handle block placement event
     * @param position World position of block placement
     * @param blockType Type of block being placed
     */
    void onBlockPlace(const glm::ivec3& position, BlockType blockType);

    /**
     * @brief Handle block destruction event
     * @param position World position of block being broken
     */
    void onBlockBreak(const glm::ivec3& position);

    /**
     * @brief Handle player movement event
     */
    void onPlayerMove();
    /// @}

    /// @name Interaction Helpers
    /// @{
    /**
     * @brief Cast ray through voxel world for block interaction
     * @param ray Ray to cast from camera
     * @param chunkManager Chunk manager to query
     * @param maxDistance Maximum raycast distance
     * @return Hit result with block information
     */
    BlockHitResult raycastVoxelsClient(const Ray& ray, ClientChunkManager* chunkManager, float maxDistance);
    /// @}

    /// @name Network Message Processing (delegated to GameClientNetwork)
    /// @{
    /**
     * @brief Process incoming server message
     * @param packet Network packet to process
     */
    void processServerMessage(const NetworkPacket& packet);

    /**
     * @brief Handle network message (compatibility wrapper)
     * @param packet Network packet to handle
     */
    void handleNetworkMessage(const NetworkPacket& packet) { processServerMessage(packet); }
    /// @}

    /// @name Configuration
    /// @{
    /**
     * @brief Get current client configuration
     * @return Reference to client configuration
     */
    const ClientConfig& getConfig() const { return m_config; }

    /**
     * @brief Update client configuration settings
     * @param config New configuration to apply
     */
    void setConfig(const ClientConfig& config) { m_config = config; }
    /// @}

private:
    ClientConfig m_config;                                  ///< Client configuration settings
    std::atomic<ClientState> m_state{ClientState::DISCONNECTED};   ///< Current network state
    std::atomic<GameState> m_gameState{GameState::MAIN_MENU};      ///< Current game flow state

    /// @name Vulkan/Graphics
    /// @{
    GLFWwindow* m_window = nullptr;         ///< GLFW window handle
    std::unique_ptr<VulkanDevice> m_device; ///< Vulkan device wrapper
    /// @}

    /// @name Subsystems (delegated to specialized classes)
    /// @{
    std::unique_ptr<GameClientRenderer> m_renderer;     ///< Rendering subsystem

    std::unique_ptr<GameClientInput> m_input;           ///< Input handling subsystem

    std::unique_ptr<GameClientNetwork> m_network;       ///< Network communication subsystem

    std::unique_ptr<GameClientUI> m_ui;                 ///< User interface subsystem
    /// @}


    /// @name Core Game Components
    /// @{
    Camera m_camera;                                    ///< Main camera for rendering and movement

    /// @name Timing
    /// @{
    float m_lastFrameTime = 0.0f;                       ///< Previous frame timestamp
    float m_deltaTime = 0.0f;                           ///< Time between frames
    /// @}

    /// @name World and Player State
    /// @{
    std::unique_ptr<ClientChunkManager> m_chunkManager; ///< World chunk management
    std::unordered_map<PlayerId, ClientPlayer> m_players;  ///< Connected player data
    PlayerId m_localPlayerId = INVALID_PLAYER_ID;       ///< Local player identifier
    mutable std::mutex m_playerMutex;                    ///< Thread-safe player access
    /// @}

    /// @name UI and Interaction State
    /// @{
    int m_selectedHotbarSlot = 0;                       ///< Currently selected hotbar slot
    static constexpr int HOTBAR_SLOTS = 8;              ///< Number of hotbar slots
    BlockType m_hotbarBlocks[HOTBAR_SLOTS] = {          ///< Block types in hotbar
        BlockType::STONE, BlockType::DIRT, BlockType::GRASS, BlockType::WOOD,
        BlockType::SAND, BlockType::BRICK, BlockType::COBBLESTONE, BlockType::SNOW
    };

    DebugInfo m_debugInfo;                              ///< Performance and debug statistics

    bool m_wireframeMode = false;                       ///< Wireframe rendering toggle
    /// @}

    /// @name Networking (managed by GameClientNetwork)
    /// @{
    GameServer* m_localServer = nullptr;                ///< Local server for singleplayer
    std::unique_ptr<GameServer> m_integratedServer;     ///< Integrated server instance
    /// @}

    /// @name Threading
    /// @{
    std::unique_ptr<std::thread> m_networkThread;       ///< Network processing thread
    std::atomic<bool> m_running{false};                 ///< Main loop running flag
    std::atomic<bool> m_cleanedUp{false};               ///< Cleanup completion flag
    /// @}

    /// @name World Management
    /// @{
    std::vector<std::string> m_availableWorlds;         ///< List of available world names
    std::string m_saveDirectory;                        ///< Save directory path
    /// @}

    /// @name Resource Management
    /// @{
    std::unique_ptr<UserDataManager> m_userDataManager; ///< User preferences and settings
    std::unique_ptr<TextureManager> m_textureManager;   ///< Texture atlas and management
    /// @}
    /// @}

    /// @name Private Implementation
    /// @{
    /**
     * @brief Initialize GLFW window
     */
    void initWindow();

    /**
     * @brief Initialize Vulkan graphics system
     */
    void initVulkan();

    /// @note Rendering methods delegated to GameClientRenderer and GameClientUI

    /**
     * @brief Update camera transforms and movement
     * @param deltaTime Time since last frame
     */
    void updateTransforms(float deltaTime);

    /**
     * @brief Load chunks around player position
     */
    void loadChunksAroundPlayer();


    /**
     * @brief Handle block update message from server
     * @param msg Block update message
     */
    void handleBlockUpdate(const BlockUpdateMessage& msg);

    /**
     * @brief Cleanup Vulkan resources
     */
    void cleanupVulkan();

    /**
     * @brief GLFW framebuffer resize callback
     * @param window GLFW window handle
     * @param width New window width
     * @param height New window height
     */
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
    /// @}
};

/// @note ClientChunkManager is now in separate files

/// @note ClientChunk is now in separate files