#include "GameClient.h"
#include "GameServer.h"
#include "NetworkManager.h"
#include "NetworkProtocol.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <cstdlib>
#include <array>
#include <cmath>

// Vertex implementation
VkVertexInputBindingDescription Vertex::getBindingDescription() {
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return bindingDescription;
}

std::array<VkVertexInputAttributeDescription, 3> Vertex::getAttributeDescriptions() {
    std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(Vertex, pos);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(Vertex, normal);

    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

    return attributeDescriptions;
}

// Vertex data for a cube with consistent CCW winding when viewed from outside
const std::vector<Vertex> vertices = {
    // Front face (+Z) - CCW from outside: (-0.5,-0.5,0.5)→(0.5,-0.5,0.5)→(0.5,0.5,0.5)→(-0.5,0.5,0.5)
    {{-0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
    {{ 0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
    {{ 0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
    {{-0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},

    // Back face (-Z) - CCW from outside: (0.5,-0.5,-0.5)→(-0.5,-0.5,-0.5)→(-0.5,0.5,-0.5)→(0.5,0.5,-0.5)
    {{ 0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}},
    {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}},
    {{-0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}},
    {{ 0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}},

    // Additional faces would go here...
};

const std::vector<uint32_t> indices = {
    0, 1, 2, 2, 3, 0,  // Front face
    4, 5, 6, 6, 7, 4,  // Back face
    // Additional indices would go here...
};

GameClient::GameClient(const ClientConfig& config)
    : m_config(config)
    , m_vertices(vertices)
    , m_indices(indices)
    , m_camera(glm::vec3(0.0f, 18.0f, 0.0f)) {
    initializeSaveDirectory();
    scanForWorlds();
}

GameClient::~GameClient() {
    shutdown();
}

bool GameClient::initialize() {
    std::cout << "Initializing Tidal Engine Client..." << std::endl;

    try {
        initWindow();

        initVulkan();

        // Only initialize game systems if not in menu mode
        if (m_gameState != GameState::MAIN_MENU) {
            // Initialize chunk manager for rendering
            m_chunkManager = std::make_unique<ClientChunkManager>(*m_device);

            // Initialize network client
            m_networkClient = std::make_unique<NetworkClient>();
            m_networkClient->setGameClient(this);
        }

        initImGui();

        // Create ImGui atlas texture descriptor after both texture manager and ImGui are ready
        if (m_textureManager) {
            createAtlasTextureDescriptor();
        }

        m_running = true;
        std::cout << "Client initialized successfully!" << std::endl;

        // Test chunk creation disabled for flat terrain testing
        // if (m_chunkManager) {
        //     std::cout << "Creating test chunk for debugging..." << std::endl;
        //     ChunkPos testPos = {0, 0, 0};
        //     ChunkData testData;
        //     // Fill with some test blocks - all stone for now
        //     for (int x = 0; x < CHUNK_SIZE; x++) {
        //         for (int y = 0; y < 8; y++) { // Only bottom 8 blocks
        //             for (int z = 0; z < CHUNK_SIZE; z++) {
        //                 testData.voxels[x][y][z] = BlockType::STONE;
        //             }
        //         }
        //     }
        //     m_chunkManager->setChunkData(testPos, testData);
        // }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize client: " << e.what() << std::endl;
        return false;
    }
}

void GameClient::run() {
    if (!m_running) {
        std::cerr << "Client not initialized! Call initialize() first." << std::endl;
        return;
    }

    std::cout << "Starting client..." << std::endl;

    // Main game loop
    auto lastTime = std::chrono::high_resolution_clock::now();

    while (m_running && !glfwWindowShouldClose(m_window)) {
        auto currentTime = std::chrono::high_resolution_clock::now();
        m_deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
        lastTime = currentTime;

        // Update debug info
        m_debugInfo.frameTime = m_deltaTime * 1000.0f; // Convert to milliseconds
        m_debugInfo.fps = (m_deltaTime > 0.0f) ? (1.0f / m_deltaTime) : 0.0f;

        glfwPollEvents();

        processInput(m_deltaTime);

        // Only process game messages when not in menu
        if (m_gameState != GameState::MAIN_MENU) {
            processIncomingMessages();
        }

        // Check if we should transition from LOADING to IN_GAME
        if (m_gameState == GameState::LOADING && m_chunkManager) {
            // Wait until we have some initial chunks loaded around spawn
            size_t loadedChunks = m_chunkManager->getLoadedChunkCount();
            if (loadedChunks >= 1) {  // At least spawn chunk loaded
                std::cout << "Initial chunks loaded (" << loadedChunks << "), starting game!" << std::endl;
                m_gameState = GameState::IN_GAME;
                glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                m_mouseCaptured = true;
                m_firstMouse = true;
            }
        }

        render(m_deltaTime);
    }

    shutdown();
}

void GameClient::shutdown() {
    if (!m_running || m_cleanedUp.load()) return;

    std::cout << "Shutting down client..." << std::endl;
    m_running = false;
    m_cleanedUp = true;

    // Disconnect from server
    disconnect();

    // Wait for device idle
    if (m_device) {
        vkDeviceWaitIdle(m_device->getDevice());
    }

    // Cleanup in reverse order
    cleanupImGui();
    m_chunkManager.reset();

    // Explicitly cleanup renderer before other Vulkan resources
    m_renderer.reset();

    cleanupVulkan();

    if (m_window) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
    glfwTerminate();

    std::cout << "Client shutdown complete." << std::endl;
}

bool GameClient::connectToServer(const std::string& address, uint16_t port) {
    if (m_state != ClientState::DISCONNECTED) {
        return false;
    }

    std::cout << "Connecting to server " << address << ":" << port << "..." << std::endl;
    m_state = ClientState::CONNECTING;

    if (!m_networkClient) {
        std::cerr << "Network client not initialized!" << std::endl;
        m_state = ClientState::DISCONNECTED;
        return false;
    }

    // Connect using NetworkClient
    if (m_networkClient->connect(address, port, m_config.playerName)) {
        m_state = ClientState::CONNECTED;
        std::cout << "Connected to server!" << std::endl;
        return true;
    } else {
        std::cerr << "Failed to connect to server!" << std::endl;
        m_state = ClientState::DISCONNECTED;
        return false;
    }
}

void GameClient::connectToLocalServer(GameServer* localServer) {
    if (!localServer) return;

    std::cout << "Connecting to local server..." << std::endl;
    m_localServer = localServer;
    m_state = ClientState::CONNECTED;

    // Set up integrated server callback for message passing
    m_localServer->setIntegratedClientCallback([this](PlayerId playerId, const NetworkPacket& packet) {
        // Only process messages for our local player
        if (playerId == m_localPlayerId) {
            // Process message directly in the client
            processServerMessage(packet);
        }
    });

    // Add ourselves as a player to the local server
    PlayerId playerId;
    if (m_localServer->addPlayer(m_config.playerName, playerId)) {
        m_localPlayerId = playerId;
        m_state = ClientState::IN_GAME;
        std::cout << "Connected to local server as player " << playerId << std::endl;
    }
}

bool GameClient::startSingleplayer(const std::string& worldName) {
    if (m_state != ClientState::DISCONNECTED) {
        std::cerr << "Cannot start singleplayer: client not in disconnected state" << std::endl;
        return false;
    }

    std::cout << "Starting singleplayer world: " << worldName << std::endl;

    // Create integrated server configuration
    ServerConfig serverConfig;
    serverConfig.serverName = "Integrated Server";
    serverConfig.worldName = worldName;
    serverConfig.port = 25565; // Default port (not used for integrated server)
    serverConfig.maxPlayers = 1;
    serverConfig.enableServerCommands = false;
    serverConfig.enableRemoteAccess = false;

    // Create and initialize integrated server
    m_integratedServer = std::make_unique<GameServer>(serverConfig);

    if (!m_integratedServer->initialize()) {
        std::cerr << "Failed to initialize integrated server!" << std::endl;
        m_integratedServer.reset();
        return false;
    }

    // Connect to the integrated server
    connectToLocalServer(m_integratedServer.get());

    if (m_state == ClientState::IN_GAME) {
        std::cout << "Singleplayer world '" << worldName << "' started successfully!" << std::endl;
        return true;
    } else {
        std::cerr << "Failed to connect to integrated server!" << std::endl;
        m_integratedServer.reset();
        return false;
    }
}

void GameClient::disconnect() {
    if (m_state == ClientState::DISCONNECTED) return;

    std::cout << "Disconnecting from server..." << std::endl;
    m_state = ClientState::DISCONNECTING;

    // Disconnect from network server
    if (m_networkClient) {
        m_networkClient->disconnect();
    }

    // Disconnect from local server
    if (m_localServer && m_localPlayerId != INVALID_PLAYER_ID) {
        m_localServer->removePlayer(m_localPlayerId);
        m_localPlayerId = INVALID_PLAYER_ID;
    }

    m_localServer = nullptr;

    // Shutdown integrated server if running
    if (m_integratedServer) {
        std::cout << "Shutting down integrated server..." << std::endl;
        m_integratedServer->shutdown();
        m_integratedServer.reset();
    }

    // === COMPLETE CLIENT RESET ===
    std::cout << "Performing complete client reset..." << std::endl;

    // Clear all world data
    if (m_chunkManager) {
        std::cout << "Clearing all chunk data..." << std::endl;
        auto chunkPositions = m_chunkManager->getLoadedChunkPositions();
        for (const auto& pos : chunkPositions) {
            m_chunkManager->removeChunk(pos);
        }
        // Reset chunk manager completely
        m_chunkManager.reset();
    }

    // Reset ALL player state
    std::cout << "Resetting player state..." << std::endl;
    {
        std::lock_guard<std::mutex> lock(m_playerMutex);
        m_players.clear();  // Clear all other players
    }
    m_localPlayerId = INVALID_PLAYER_ID;

    // Reset camera to spawn position
    m_camera = Camera(glm::vec3(0.0f, 50.0f, 0.0f));

    // Reset hotbar and inventory
    m_selectedHotbarSlot = 0;
    // Note: m_hotbarBlocks is static array, no need to reset

    // Reset input state
    memset(m_keys, false, sizeof(m_keys));
    m_mouseCaptured = false;
    m_firstMouse = true;

    // Reset debug/UI state
    m_showDebugWindow = false;
    m_showPerformanceHUD = false;
    m_showRenderingHUD = false;
    m_showCameraHUD = false;
    m_showChunkBoundaries = false;
    m_showCreateWorldDialog = false;

    // Clear debug info
    memset(&m_debugInfo, 0, sizeof(m_debugInfo));

    // Reset cursor to menu mode
    glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    // TODO: Future mod system cleanup
    // When mod system is implemented, add here:
    // - Unload all dynamic mods
    // - Clear mod-added content/blocks/entities
    // - Reset mod-specific client state
    // - Clear mod network protocols

    std::cout << "Complete client reset finished." << std::endl;

    m_state = ClientState::DISCONNECTED;
    std::cout << "Disconnected from server." << std::endl;
}

void GameClient::processInput(float deltaTime) {
    // Speed boost when holding Ctrl
    float speedMultiplier = m_keys[GLFW_KEY_LEFT_CONTROL] || m_keys[GLFW_KEY_RIGHT_CONTROL] ? 5.0f : 1.0f;
    float adjustedDeltaTime = deltaTime * speedMultiplier;

    // Camera movement - only when cursor is captured
    if (m_mouseCaptured) {
        if (m_keys[GLFW_KEY_W]) m_camera.processKeyboard(CameraMovement::FORWARD, adjustedDeltaTime);
        if (m_keys[GLFW_KEY_S]) m_camera.processKeyboard(CameraMovement::BACKWARD, adjustedDeltaTime);
        if (m_keys[GLFW_KEY_A]) m_camera.processKeyboard(CameraMovement::LEFT, adjustedDeltaTime);
        if (m_keys[GLFW_KEY_D]) m_camera.processKeyboard(CameraMovement::RIGHT, adjustedDeltaTime);
        if (m_keys[GLFW_KEY_SPACE]) m_camera.processKeyboard(CameraMovement::UP, adjustedDeltaTime);
        if (m_keys[GLFW_KEY_LEFT_SHIFT]) m_camera.processKeyboard(CameraMovement::DOWN, adjustedDeltaTime);
    }

    // Hotbar selection
    for (int i = GLFW_KEY_1; i <= GLFW_KEY_8; i++) {
        if (m_keys[i]) {
            int slot = i - GLFW_KEY_1;
            if (slot < HOTBAR_SLOTS && slot != m_selectedHotbarSlot) {
                std::cout << "Hotbar selection changed from " << m_selectedHotbarSlot << " to " << slot
                          << " (key " << (i - GLFW_KEY_1 + 1) << " pressed)" << std::endl;
                m_selectedHotbarSlot = slot;
            }
        }
    }

    // Debug window toggle (F1 like in the original Application)
    if (m_keys[GLFW_KEY_F1]) {
        m_showDebugWindow = !m_showDebugWindow;
        m_keys[GLFW_KEY_F1] = false; // Prevent rapid toggling
    }

    // Toggle cursor capture with F2
    if (m_keys[GLFW_KEY_F2]) {
        m_mouseCaptured = !m_mouseCaptured;
        glfwSetInputMode(m_window, GLFW_CURSOR, m_mouseCaptured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
        m_firstMouse = true; // Reset mouse to prevent jump
        m_keys[GLFW_KEY_F2] = false;
    }

    // Toggle wireframe mode with F3
    if (m_keys[GLFW_KEY_F3]) {
        m_wireframeMode = !m_wireframeMode;
        recreateGraphicsPipeline();
        m_keys[GLFW_KEY_F3] = false;
    }

    // Toggle chunk boundaries with F4
    if (m_keys[GLFW_KEY_F4]) {
        m_showChunkBoundaries = !m_showChunkBoundaries;
        m_keys[GLFW_KEY_F4] = false;
    }

    // ESC key handling
    if (m_keys[GLFW_KEY_ESCAPE]) {
        if (m_gameState == GameState::IN_GAME) {
            // Pause the game and show pause menu
            m_gameState = GameState::PAUSED;
            glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            m_mouseCaptured = false;
            std::cout << "Game paused" << std::endl;
        } else if (m_gameState == GameState::PAUSED) {
            // Unpause the game
            m_gameState = GameState::IN_GAME;
            glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            m_mouseCaptured = true;
            m_firstMouse = true;
            std::cout << "Game resumed" << std::endl;
        } else {
            // In menu states, quit the game
            // Force save before quitting if we have an integrated server
            if (m_integratedServer) {
                std::cout << "Saving world before exit..." << std::endl;
                m_integratedServer->saveWorld();
            }

            glfwSetWindowShouldClose(m_window, GLFW_TRUE);
        }
        m_keys[GLFW_KEY_ESCAPE] = false;
    }
}

void GameClient::render(float deltaTime) {
    updateTransforms(deltaTime);

    // Process any deferred mesh updates before starting the frame
    if (m_chunkManager) {
        m_chunkManager->updateDirtyMeshesSafe();
        // Also process any newly dirty meshes from incoming messages before rendering
        m_chunkManager->updateDirtyMeshes();
    }

    drawFrame();

    // No mesh updates after drawFrame() to prevent command buffer conflicts
}

void GameClient::onBlockPlace(const glm::ivec3& position, BlockType blockType) {
    if (!isConnected()) return;

    // Send block place message to server
    BlockPlaceMessage msg;
    msg.position = position;
    msg.blockType = blockType;
    msg.timestamp = static_cast<uint32_t>(glfwGetTime() * 1000);

    NetworkPacket packet(MessageType::BLOCK_PLACE, &msg, sizeof(msg));
    sendToServer(packet);
}

void GameClient::onBlockBreak(const glm::ivec3& position) {
    if (!isConnected()) return;

    // Send block break message to server
    BlockBreakMessage msg;
    msg.position = position;
    msg.timestamp = static_cast<uint32_t>(glfwGetTime() * 1000);

    NetworkPacket packet(MessageType::BLOCK_BREAK, &msg, sizeof(msg));
    sendToServer(packet);
}

void GameClient::onPlayerMove() {
    if (!isConnected() || m_localPlayerId == INVALID_PLAYER_ID) return;

    // Send player movement to server
    PlayerMoveMessage msg;
    msg.playerId = m_localPlayerId;
    msg.position = m_camera.getPosition();
    msg.yaw = m_camera.Yaw;
    msg.pitch = m_camera.Pitch;
    msg.timestamp = static_cast<uint32_t>(glfwGetTime() * 1000);

    NetworkPacket packet(MessageType::PLAYER_MOVE, &msg, sizeof(msg));
    sendToServer(packet);
}

void GameClient::sendToServer(const NetworkPacket& packet) {
    if (m_localServer) {
        // Direct call for local server
        m_localServer->processMessage(m_localPlayerId, packet);
    } else {
        // Queue for network transmission
        m_outgoingMessages.push(packet);
    }
}

void GameClient::processIncomingMessages() {
    NetworkPacket packet;
    while (m_incomingMessages.pop(packet)) {
        processServerMessage(packet);
    }
}

void GameClient::processServerMessage(const NetworkPacket& packet) {
    switch (packet.header.type) {
        case MessageType::SERVER_HELLO: {
            if (packet.payload.size() >= sizeof(ServerHelloMessage)) {
                ServerHelloMessage msg;
                std::memcpy(&msg, packet.payload.data(), sizeof(msg));
                handleServerHello(msg);
            }
            break;
        }

        case MessageType::PLAYER_UPDATE: {
            if (packet.payload.size() >= sizeof(PlayerUpdateMessage)) {
                PlayerUpdateMessage msg;
                std::memcpy(&msg, packet.payload.data(), sizeof(msg));
                handlePlayerUpdate(msg);
            }
            break;
        }

        case MessageType::BLOCK_UPDATE: {
            if (packet.payload.size() >= sizeof(BlockUpdateMessage)) {
                BlockUpdateMessage msg;
                std::memcpy(&msg, packet.payload.data(), sizeof(msg));
                handleBlockUpdate(msg);
            }
            break;
        }

        case MessageType::CHUNK_DATA: {
            if (packet.payload.size() >= sizeof(ChunkDataMessage)) {
                ChunkDataMessage msg;
                std::memcpy(&msg, packet.payload.data(), sizeof(msg));
                handleChunkData(msg, packet.payload);
            }
            break;
        }

        default:
            std::cout << "Received unknown message type: " << static_cast<int>(packet.header.type) << std::endl;
            break;
    }
}

void GameClient::handleServerHello(const ServerHelloMessage& msg) {
    if (msg.accepted) {
        m_localPlayerId = msg.playerId;
        m_camera.Position = msg.spawnPosition;
        m_camera.Pitch = -45.0f; // Look down at terrain
        m_camera.updateCameraVectors();
        m_state = ClientState::IN_GAME;
        std::cout << "Joined game as player " << msg.playerId << std::endl;
    } else {
        std::cout << "Connection rejected: " << msg.reason << std::endl;
        disconnect();
    }
}

void GameClient::handlePlayerUpdate(const PlayerUpdateMessage& msg) {
    std::lock_guard<std::mutex> lock(m_playerMutex);

    ClientPlayer& player = m_players[msg.playerId];
    player.id = msg.playerId;
    player.position = msg.position;
    player.yaw = msg.yaw;
    player.pitch = msg.pitch;
    player.isLocalPlayer = (msg.playerId == m_localPlayerId);
}

void GameClient::handleBlockUpdate(const BlockUpdateMessage& msg) {
    std::cout << "Client received block update: " << static_cast<int>(msg.blockType)
              << " at (" << msg.position.x << ", " << msg.position.y << ", " << msg.position.z << ")" << std::endl;

    // Update block in client chunk manager
    if (m_chunkManager) {
        m_chunkManager->updateBlock(msg.position, msg.blockType);
        std::cout << "Block update sent to chunk manager" << std::endl;
    } else {
        std::cout << "ERROR: No chunk manager available for block update!" << std::endl;
    }
}

void GameClient::handleChunkData(const ChunkDataMessage& msg, const std::vector<uint8_t>& packetPayload) {
    // std::cout << "Received chunk data for chunk (" << msg.chunkPos.x << ", " << msg.chunkPos.y << ", " << msg.chunkPos.z
    //           << ") empty: " << (msg.isEmpty ? "yes" : "no") << std::endl;

    if (!m_chunkManager) {
        std::cerr << "Cannot process chunk data: chunk manager not initialized" << std::endl;
        return;
    }

    // Reconstruct chunk data from message
    ChunkData chunkData;
    chunkData.position = msg.chunkPos;

    if (!msg.isEmpty) {
        // Verify we have enough data for voxels
        size_t headerSize = sizeof(ChunkDataMessage);
        size_t expectedSize = headerSize + sizeof(chunkData.voxels);

        if (packetPayload.size() >= expectedSize) {
            // Copy voxel data from packet payload (after the header)
            std::memcpy(chunkData.voxels, packetPayload.data() + headerSize, sizeof(chunkData.voxels));
            // std::cout << "Loaded voxel data for chunk (" << msg.chunkPos.x << ", " << msg.chunkPos.y << ", " << msg.chunkPos.z << ")" << std::endl;
        } else {
            std::cerr << "Chunk data packet too small: expected " << expectedSize
                      << " bytes, got " << packetPayload.size() << " bytes" << std::endl;
            return;
        }
    } else {
        // Empty chunk - initialize with air blocks
        std::memset(chunkData.voxels, static_cast<int>(BlockType::AIR), sizeof(chunkData.voxels));
        std::cout << "Chunk is empty, filled with air blocks" << std::endl;
    }

    // Set chunk data in client chunk manager
    m_chunkManager->setChunkData(msg.chunkPos, chunkData);
    // std::cout << "Successfully processed chunk (" << msg.chunkPos.x << ", " << msg.chunkPos.y << ", " << msg.chunkPos.z << ")" << std::endl;
}

// Window and Vulkan initialization (similar to Application)
void GameClient::initWindow() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    m_window = glfwCreateWindow(m_config.windowWidth, m_config.windowHeight,
                               "Tidal Engine Client", nullptr, nullptr);

    if (!m_window) {
        throw std::runtime_error("Failed to create GLFW window");
    }

    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, framebufferResizeCallback);
    glfwSetKeyCallback(m_window, keyCallback);
    glfwSetCursorPosCallback(m_window, mouseCallback);
    glfwSetMouseButtonCallback(m_window, mouseButtonCallback);
    glfwSetScrollCallback(m_window, scrollCallback);

    // Cursor mode depends on game state
    if (m_gameState == GameState::MAIN_MENU) {
        // Menu mode: cursor should be visible and free
        glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        m_mouseCaptured = false;
    } else {
        // Game mode: capture cursor for FPS-style camera
        glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        m_mouseCaptured = true;
    }
}

void GameClient::initVulkan() {
    m_device = std::make_unique<VulkanDevice>(m_window);
    m_renderer = std::make_unique<VulkanRenderer>(m_window, *m_device);
    createRenderPass();
    createDescriptorSetLayout();
    createGraphicsPipeline();
    createWireframePipeline();
    createCommandPool();
    createFramebuffers();

    // Initialize texture system (after device, before descriptor sets)
    m_userDataManager = std::make_unique<UserDataManager>();
    m_textureManager = std::make_unique<TextureManager>(*m_device, m_userDataManager.get());
    m_textureManager->loadActiveTexturepack();

    createVertexBuffer();
    createIndexBuffer();
    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();
    createCommandBuffers();
}

void GameClient::initImGui() {
    // Create descriptor pool for ImGui
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = std::size(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;

    if (vkCreateDescriptorPool(m_device->getDevice(), &pool_info, nullptr, &m_imguiDescriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create ImGui descriptor pool!");
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForVulkan(m_window, true);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = m_device->getInstance();
    init_info.PhysicalDevice = m_device->getPhysicalDevice();
    init_info.Device = m_device->getDevice();
    init_info.QueueFamily = m_device->findPhysicalQueueFamilies().graphicsFamily.value();
    init_info.Queue = m_device->getGraphicsQueue();
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = m_imguiDescriptorPool;
    init_info.Subpass = 0;
    init_info.MinImageCount = 2;
    init_info.ImageCount = 2;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.Allocator = nullptr;
    init_info.CheckVkResultFn = nullptr;
    ImGui_ImplVulkan_Init(&init_info, m_renderer->getSwapChainRenderPass());

    // Upload fonts
    VkCommandBuffer command_buffer = m_device->beginSingleTimeCommands();
    ImGui_ImplVulkan_CreateFontsTexture();
    m_device->endSingleTimeCommands(command_buffer);
    vkDeviceWaitIdle(m_device->getDevice());
    ImGui_ImplVulkan_DestroyFontsTexture();

    // Create atlas texture descriptor for ImGui (will be set when texture manager is ready)
    m_atlasTextureDescriptor = VK_NULL_HANDLE;
}

// Placeholder implementations for complex Vulkan methods
void GameClient::createRenderPass() {
    // Use the renderer's render pass for now
    m_renderPass = m_renderer->getSwapChainRenderPass();
}

void GameClient::createDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.pImmutableSamplers = nullptr;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutBinding samplerLayoutBinding{};
    samplerLayoutBinding.binding = 1;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.pImmutableSamplers = nullptr;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    std::array<VkDescriptorSetLayoutBinding, 2> bindings = {uboLayoutBinding, samplerLayoutBinding};
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(m_device->getDevice(), &layoutInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout!");
    }
}

static std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file: " + filename);
    }

    size_t fileSize = (size_t) file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

void GameClient::createGraphicsPipeline() {
    auto vertShaderCode = readFile("shaders/compiled/vertex.vert.spv");
    auto fragShaderCode = readFile("shaders/compiled/fragment.frag.spv");

    VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    auto bindingDescription = ChunkVertex::getBindingDescription();
    auto attributeDescriptions = ChunkVertex::getAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = m_wireframeMode ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;  // Temporarily disable culling until all vertex winding is validated
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(PushConstants);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(m_device->getDevice(), &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create pipeline layout!");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_pipelineLayout;
    pipelineInfo.renderPass = m_renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    if (vkCreateGraphicsPipelines(m_device->getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_graphicsPipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create graphics pipeline!");
    }

    vkDestroyShaderModule(m_device->getDevice(), fragShaderModule, nullptr);
    vkDestroyShaderModule(m_device->getDevice(), vertShaderModule, nullptr);
}

void GameClient::createWireframePipeline() {
    auto vertShaderCode = readFile("shaders/compiled/vertex.vert.spv");
    auto fragShaderCode = readFile("shaders/compiled/fragment.frag.spv");

    VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    auto bindingDescription = ChunkVertex::getBindingDescription();
    auto attributeDescriptions = ChunkVertex::getAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    // KEY DIFFERENCE: Use line topology instead of triangle topology
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL; // Not used for lines
    rasterizer.lineWidth = 2.0f; // Thicker lines for better visibility
    rasterizer.cullMode = VK_CULL_MODE_NONE;  // Temporarily disable culling for wireframe too
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_FALSE; // Don't write to depth buffer for wireframes
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL; // Allow wireframes to show through
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_pipelineLayout; // Reuse same layout
    pipelineInfo.renderPass = m_renderPass; // Reuse same render pass
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    if (vkCreateGraphicsPipelines(m_device->getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_wireframePipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create wireframe pipeline!");
    }

    vkDestroyShaderModule(m_device->getDevice(), fragShaderModule, nullptr);
    vkDestroyShaderModule(m_device->getDevice(), vertShaderModule, nullptr);
}

void GameClient::recreateGraphicsPipeline() {
    vkDeviceWaitIdle(m_device->getDevice());
    vkDestroyPipeline(m_device->getDevice(), m_graphicsPipeline, nullptr);
    vkDestroyPipeline(m_device->getDevice(), m_wireframePipeline, nullptr);
    createGraphicsPipeline();
    createWireframePipeline();
}

void GameClient::createFramebuffers() {
    // Framebuffers are created by the renderer
}

void GameClient::createCommandPool() {
    QueueFamilyIndices queueFamilyIndices = m_device->findPhysicalQueueFamilies();

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

    if (vkCreateCommandPool(m_device->getDevice(), &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create command pool!");
    }
}

void GameClient::createVertexBuffer() {
    VkDeviceSize bufferSize = sizeof(m_vertices[0]) * m_vertices.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    m_device->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(m_device->getDevice(), stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, m_vertices.data(), (size_t) bufferSize);
    vkUnmapMemory(m_device->getDevice(), stagingBufferMemory);

    m_vertexBuffer = std::make_unique<VulkanBuffer>(
        *m_device,
        sizeof(m_vertices[0]),
        static_cast<uint32_t>(m_vertices.size()),
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    m_device->copyBuffer(stagingBuffer, m_vertexBuffer->getBuffer(), bufferSize);

    vkDestroyBuffer(m_device->getDevice(), stagingBuffer, nullptr);
    vkFreeMemory(m_device->getDevice(), stagingBufferMemory, nullptr);
}

void GameClient::createIndexBuffer() {
    VkDeviceSize bufferSize = sizeof(m_indices[0]) * m_indices.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    m_device->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(m_device->getDevice(), stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, m_indices.data(), (size_t) bufferSize);
    vkUnmapMemory(m_device->getDevice(), stagingBufferMemory);

    m_indexBuffer = std::make_unique<VulkanBuffer>(
        *m_device,
        sizeof(m_indices[0]),
        static_cast<uint32_t>(m_indices.size()),
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    m_device->copyBuffer(stagingBuffer, m_indexBuffer->getBuffer(), bufferSize);

    vkDestroyBuffer(m_device->getDevice(), stagingBuffer, nullptr);
    vkFreeMemory(m_device->getDevice(), stagingBufferMemory, nullptr);
}

void GameClient::createUniformBuffers() {
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);

    m_uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        m_uniformBuffers[i] = std::make_unique<VulkanBuffer>(
            *m_device,
            bufferSize,
            1,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        m_uniformBuffers[i]->map();
    }
}

void GameClient::createDescriptorPool() {
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

    if (vkCreateDescriptorPool(m_device->getDevice(), &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool!");
    }
}

void GameClient::createDescriptorSets() {
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, m_descriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    allocInfo.pSetLayouts = layouts.data();

    m_descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    if (vkAllocateDescriptorSets(m_device->getDevice(), &allocInfo, m_descriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = m_uniformBuffers[i]->getBuffer();
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);

        // Bind texture atlas from TextureManager
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = m_textureManager->getAtlasImageView();
        imageInfo.sampler = m_textureManager->getAtlasSampler();

        std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = m_descriptorSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &bufferInfo;

        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = m_descriptorSets[i];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(m_device->getDevice(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }
}

void GameClient::createCommandBuffers() {
    m_commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_commandPool;
    allocInfo.commandBufferCount = static_cast<uint32_t>(m_commandBuffers.size());

    if (vkAllocateCommandBuffers(m_device->getDevice(), &allocInfo, m_commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers!");
    }
}

void GameClient::createAtlasTextureDescriptor() {
    if (!m_textureManager || m_imguiDescriptorPool == VK_NULL_HANDLE) {
        std::cerr << "Cannot create atlas texture descriptor: texture manager or ImGui descriptor pool not ready" << std::endl;
        return;
    }

    // Create descriptor set for the texture atlas to use with ImGui
    // Note: ImGui manages its own descriptor set layouts

    // Use ImGui_ImplVulkan_AddTexture to create the descriptor set
    m_atlasTextureDescriptor = ImGui_ImplVulkan_AddTexture(
        m_textureManager->getAtlasSampler(),
        m_textureManager->getAtlasImageView(),
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );

    if (m_atlasTextureDescriptor == VK_NULL_HANDLE) {
        std::cerr << "Failed to create atlas texture descriptor for ImGui" << std::endl;
    } else {
        std::cout << "Successfully created atlas texture descriptor for ImGui" << std::endl;
    }
}

VkShaderModule GameClient::createShaderModule(const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(m_device->getDevice(), &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("failed to create shader module!");
    }

    return shaderModule;
}

void GameClient::drawFrame() {
    if (!m_renderer) return;

    // Begin frame
    VkCommandBuffer commandBuffer = m_renderer->beginFrame();
    if (commandBuffer == VK_NULL_HANDLE) {
        // Frame couldn't be started (e.g., during resize), ensure frame flag is cleared
        if (m_chunkManager) {
            m_chunkManager->setFrameInProgress(false);
        }
        return; // Skip frame
    }

    // Mark frame as in progress to defer mesh updates
    if (m_chunkManager) {
        m_chunkManager->setFrameInProgress(true);
    }

    // Begin render pass with a clear screen
    m_renderer->beginSwapchainRenderPass(commandBuffer);

    // Render world geometry if in game mode
    if (m_gameState == GameState::IN_GAME && m_chunkManager) {
        // Update uniform buffer for this frame
        int frameIndex = m_renderer->getFrameIndex();
        updateUniformBuffer(frameIndex);

        // Bind the graphics pipeline and descriptor sets
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSets[frameIndex], 0, nullptr);

        // Set up lighting push constants
        PushConstants pushConstants{
            .lightPos = glm::vec3(50.0f, 100.0f, 50.0f),
            .lightColor = glm::vec3(1.0f, 1.0f, 1.0f),
            .viewPos = m_camera.getPosition(),
            .voxelPosition = glm::vec3(0.0f),
            .voxelColor = glm::vec3(1.0f)
        };
        vkCmdPushConstants(commandBuffer, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pushConstants);

        // Calculate frustum for culling (moved here to avoid recalculating in chunk manager)
        glm::mat4 viewMatrix = m_camera.getViewMatrix();
        glm::mat4 projMatrix = glm::perspective(glm::radians(m_camera.getZoom()), m_renderer->getAspectRatio(), 0.1f, 1000.0f);
        projMatrix[1][1] *= -1; // Flip Y coordinate for Vulkan
        Frustum frustum = m_camera.calculateFrustum(projMatrix, viewMatrix);

        // Render the voxel world with frustum culling
        size_t renderedChunks = m_chunkManager->renderVisibleChunks(commandBuffer, frustum);

        // Update debug info with rendering statistics (only when needed, cached)
        static int debugUpdateCounter = 0;
        if (m_showDebugWindow || m_showRenderingHUD) {
            if (++debugUpdateCounter % 10 == 0) { // Only update every 10 frames
                size_t totalLoadedChunks = m_chunkManager->getLoadedChunkCount();
                size_t totalFaces = m_chunkManager->getTotalFaceCount();
                size_t totalVoxels = totalLoadedChunks * CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;
                size_t culledChunks = totalLoadedChunks - renderedChunks;

                m_debugInfo.drawCalls = static_cast<uint32_t>(renderedChunks); // Actual rendered chunks
                m_debugInfo.totalVoxels = static_cast<uint32_t>(totalVoxels);
                m_debugInfo.renderedVoxels = static_cast<uint32_t>(totalFaces);
                m_debugInfo.culledVoxels = static_cast<uint32_t>(culledChunks * CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE);
            }
        } else {
            // Reset counter when debug UI is hidden for immediate update when re-enabled
            debugUpdateCounter = 0;
        }

        // Render chunk boundaries if debug mode is enabled
        if (m_showChunkBoundaries) {
            m_chunkManager->renderChunkBoundaries(commandBuffer, m_wireframePipeline, m_camera.getPosition(), m_pipelineLayout, m_descriptorSets[frameIndex]);
        }
    } else {
        // For menu mode, just clear to dark gray background
        // The clear happens automatically in beginSwapchainRenderPass
    }

    // Render ImGui
    renderImGui(commandBuffer);

    // End render pass and frame
    m_renderer->endSwapchainRenderPass(commandBuffer);
    m_renderer->endFrame();

    // Mark frame as complete - safe to process deferred mesh updates
    if (m_chunkManager) {
        m_chunkManager->setFrameInProgress(false);
    }
}

void GameClient::updateUniformBuffer(uint32_t currentImage) {
    UniformBufferObject ubo{};
    ubo.model = glm::mat4(1.0f);
    ubo.view = m_camera.getViewMatrix();
    ubo.proj = glm::perspective(glm::radians(m_camera.getZoom()), m_renderer->getAspectRatio(), 0.1f, 1000.0f);
    ubo.proj[1][1] *= -1; // Flip Y coordinate for Vulkan
    m_uniformBuffers[currentImage]->writeToBuffer(&ubo);
}

void GameClient::updateTransforms(float deltaTime) {
    // Send player movement periodically
    static float lastMoveUpdate = 0.0f;
    lastMoveUpdate += deltaTime;
    if (lastMoveUpdate > 0.05f) { // 20 updates per second
        onPlayerMove();
        lastMoveUpdate = 0.0f;
    }

    // Load chunks around player position
    if ((m_gameState == GameState::IN_GAME || m_gameState == GameState::LOADING) && m_chunkManager) {
        static float lastChunkUpdate = 0.0f;
        lastChunkUpdate += deltaTime;
        if (lastChunkUpdate > 1.0f) { // Check for new chunks every second
            loadChunksAroundPlayer();
            lastChunkUpdate = 0.0f;
        }
    }
}

void GameClient::handleBlockInteraction(bool isBreaking) {
    std::cout << "handleBlockInteraction called: " << (isBreaking ? "breaking" : "placing") << std::endl;

    // Ray casting to find target block
    Ray ray = m_camera.getRay();
    auto hitResult = raycastVoxelsClient(ray, m_chunkManager.get(), 10.0f);

    std::cout << "Raycast result: hit=" << hitResult.hit << std::endl;
    if (hitResult.hit) {
        std::cout << "Hit position: (" << hitResult.blockPosition.x << ", " << hitResult.blockPosition.y << ", " << hitResult.blockPosition.z << ")" << std::endl;

        if (isBreaking) {
            std::cout << "Breaking block at position" << std::endl;
            onBlockBreak(hitResult.blockPosition);
        } else {
            // Place block adjacent to hit surface
            glm::ivec3 placePos = hitResult.blockPosition + hitResult.normal;
            BlockType blockType = m_hotbarBlocks[m_selectedHotbarSlot];
            std::cout << "Placing block type " << static_cast<int>(blockType) << " at ("
                      << placePos.x << ", " << placePos.y << ", " << placePos.z << ")"
                      << " (normal: " << hitResult.normal.x << "," << hitResult.normal.y << "," << hitResult.normal.z << ")" << std::endl;
            onBlockPlace(placePos, blockType);
        }
    } else {
        std::cout << "No block hit by raycast" << std::endl;
    }
}

void GameClient::renderImGui(VkCommandBuffer commandBuffer) {
    // Start the Dear ImGui frame
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Render UI based on game state
    if (m_gameState == GameState::MAIN_MENU) {
        renderMainMenu();
    } else if (m_gameState == GameState::WORLD_SELECTION) {
        renderWorldSelection();
    } else if (m_gameState == GameState::LOADING) {
        renderLoadingScreen();
        // Don't render other UI during loading, but continue to frame end
    } else if (m_gameState == GameState::PAUSED) {
        renderPauseMenu();
    } else {
        // Render debug window if enabled and in game
        if (m_showDebugWindow) {
            renderDebugWindow();
        }

        // Render modular HUD boxes
        if (m_showPerformanceHUD) {
            renderPerformanceHUD();
        }
        if (m_showRenderingHUD) {
            renderRenderingHUD();
        }
        if (m_showCameraHUD) {
            renderCameraHUD();
        }

        // Render crosshair overlay
        renderCrosshair();

        // Render hotbar
        renderHotbar();
    }

    // Render ImGui
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
}

void GameClient::renderMainMenu() {
    // Center the main menu window
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x + viewport->Size.x * 0.5f, viewport->Pos.y + viewport->Size.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));

    ImGui::Begin("Tidal Engine", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::Text("Welcome to Tidal Engine!");
    ImGui::Separator();

    // Single Player button
    if (ImGui::Button("Single Player", ImVec2(200, 40))) {
        std::cout << "Opening world selection..." << std::endl;
        m_gameState = GameState::WORLD_SELECTION;
    }

    // Multiplayer button
    if (ImGui::Button("Multiplayer", ImVec2(200, 40))) {
        std::cout << "Multiplayer not implemented yet!" << std::endl;
        // Show a simple popup for now
        ImGui::OpenPopup("Multiplayer Info");
    }

    // Multiplayer info popup
    if (ImGui::BeginPopupModal("Multiplayer Info", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Multiplayer is not implemented yet!");
        ImGui::Text("For now, use command line options:");
        ImGui::Text("  ./TidalEngine --server");
        ImGui::Text("  ./TidalEngine --client <host>");
        ImGui::Separator();
        if (ImGui::Button("OK", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // Settings button
    if (ImGui::Button("Settings", ImVec2(200, 40))) {
        std::cout << "Opening settings..." << std::endl;
        // TODO: Open settings menu
        m_gameState = GameState::SETTINGS;
    }

    // Quit button
    if (ImGui::Button("Quit", ImVec2(200, 40))) {
        std::cout << "Quitting..." << std::endl;
        m_running = false;
    }

    ImGui::End();
}

void GameClient::renderWorldSelection() {
    // Center the world selection window
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x + viewport->Size.x * 0.5f, viewport->Pos.y + viewport->Size.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));

    ImGui::Begin("Select World", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::Text("Select or Create a World");
    ImGui::Separator();

    ImGui::Text("Existing Worlds:");

    if (m_availableWorlds.empty()) {
        ImGui::Text("No worlds found. Create a new world to get started!");
    } else {
        for (size_t i = 0; i < m_availableWorlds.size(); i++) {
            char buttonLabel[128];
            snprintf(buttonLabel, sizeof(buttonLabel), "%s##%zu", m_availableWorlds[i].c_str(), i);

            if (ImGui::Button(buttonLabel, ImVec2(300, 30))) {
                std::cout << "Loading world: " << m_availableWorlds[i] << std::endl;
                startSingleplayerGame(m_availableWorlds[i]);
            }
        }
    }

    ImGui::Separator();

    // Create New World button
    if (ImGui::Button("Create New World", ImVec2(300, 40))) {
        m_showCreateWorldDialog = true;
        std::strcpy(m_newWorldName, ""); // Clear the input
    }

    // Create New World Dialog
    if (m_showCreateWorldDialog) {
        ImGui::OpenPopup("Create New World");
    }

    if (ImGui::BeginPopupModal("Create New World", &m_showCreateWorldDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Enter a name for your new world:");
        ImGui::InputText("World Name", m_newWorldName, sizeof(m_newWorldName));

        ImGui::Separator();

        bool canCreate = strlen(m_newWorldName) > 0;

        if (!canCreate) {
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
        }

        if (ImGui::Button("Create", ImVec2(120, 0)) && canCreate) {
            if (createNewWorld(m_newWorldName)) {
                startSingleplayerGame(m_newWorldName);
                m_showCreateWorldDialog = false;
            }
        }

        if (!canCreate) {
            ImGui::PopStyleVar();
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_showCreateWorldDialog = false;
        }

        ImGui::EndPopup();
    }

    ImGui::Separator();

    // Back button
    if (ImGui::Button("Back to Main Menu", ImVec2(300, 30))) {
        m_gameState = GameState::MAIN_MENU;
    }

    ImGui::End();
}

void GameClient::renderPauseMenu() {
    // Center the pause menu window
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x + viewport->Size.x * 0.5f, viewport->Pos.y + viewport->Size.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));

    ImGui::Begin("Game Paused", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::Text("Game Paused");
    ImGui::Separator();

    // Back to Game button
    if (ImGui::Button("Back to Game", ImVec2(200, 40))) {
        m_gameState = GameState::IN_GAME;
        glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        m_mouseCaptured = true;
        m_firstMouse = true;
    }

    // Open to LAN button (placeholder)
    if (ImGui::Button("Open to LAN", ImVec2(200, 40))) {
        std::cout << "Open to LAN not implemented yet!" << std::endl;
        // TODO: Implement LAN functionality
    }

    // Options button
    if (ImGui::Button("Options", ImVec2(200, 40))) {
        std::cout << "Opening options..." << std::endl;
        m_gameState = GameState::SETTINGS;
    }

    // Save and Quit to Title button
    if (ImGui::Button("Save and Quit to Title", ImVec2(200, 40))) {
        std::cout << "Saving and returning to main menu..." << std::endl;

        // Save the world
        if (m_integratedServer) {
            m_integratedServer->saveWorld();
        }

        // Disconnect from server
        disconnect();

        // Reset to main menu
        m_gameState = GameState::MAIN_MENU;
        glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        m_mouseCaptured = false;
    }

    ImGui::End();
}

void GameClient::renderLoadingScreen() {
    // Create a full-screen overlay
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);

    ImGui::Begin("LoadingOverlay", nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoInputs);

    // Get the draw list for custom drawing
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Fill the entire screen with a semi-transparent dark background
    drawList->AddRectFilled(
        ImVec2(0, 0),
        io.DisplaySize,
        IM_COL32(0, 0, 0, 200)  // Semi-transparent black
    );

    // Center the loading text and animation
    float centerX = io.DisplaySize.x * 0.5f;
    float centerY = io.DisplaySize.y * 0.5f;

    // Draw "Loading..." text
    const char* loadingText = "Loading...";
    float fontSize = ImGui::GetFontSize() * 2.0f;  // 2x size
    ImVec2 textSize = ImGui::CalcTextSize(loadingText);
    textSize.x *= 2.0f;  // Account for larger font size
    textSize.y *= 2.0f;

    // Position text above center with proper spacing
    ImVec2 textPos = ImVec2(
        centerX - textSize.x * 0.5f,
        centerY - 80.0f  // Move text higher above the animation
    );

    drawList->AddText(
        ImGui::GetFont(),
        fontSize,
        textPos,
        IM_COL32(255, 255, 255, 255),  // White text
        loadingText
    );

    // Add a simple spinning animation
    static float rotation = 0.0f;
    rotation += io.DeltaTime * 2.0f;  // Rotate speed
    if (rotation > 6.28f) rotation -= 6.28f;  // Keep within 2π

    // Draw a simple rotating circle - positioned below the text
    float circleRadius = 30.0f;
    ImVec2 circleCenter = ImVec2(centerX, centerY + 20.0f);  // Closer to center

    for (int i = 0; i < 8; i++) {
        float angle = rotation + (i * 6.28f / 8.0f);
        float alpha = (sin(angle) + 1.0f) * 0.5f;  // Fade effect
        ImVec2 dotPos = ImVec2(
            circleCenter.x + cos(angle) * circleRadius,
            circleCenter.y + sin(angle) * circleRadius
        );
        drawList->AddCircleFilled(
            dotPos, 4.0f,
            IM_COL32(255, 255, 255, (uint8_t)(alpha * 255))
        );
    }

    ImGui::End();
}

void GameClient::renderDebugWindow() {
    ImGui::Begin("Debug Control Panel", &m_showDebugWindow);

    ImGui::Text("Tidal Engine Client Debug Control");
    ImGui::Separator();

    // HUD toggles
    ImGui::Text("HUD Overlays:");
    ImGui::Checkbox("Performance Stats", &m_showPerformanceHUD);
    ImGui::Checkbox("Rendering Stats", &m_showRenderingHUD);
    ImGui::Checkbox("Camera Info", &m_showCameraHUD);
    ImGui::Separator();

    ImGui::Separator();

    // Client state
    ImGui::Text("Client Status:");
    ImGui::Text("  Connection: %s", isConnected() ? "Connected" : "Disconnected");
    ImGui::Text("  Game State: %s", m_gameState == GameState::IN_GAME ? "In Game" : "Menu");
    if (m_chunkManager) {
        ImGui::Text("  Loaded Chunks: %zu", m_chunkManager->getLoadedChunkCount());
    }
    ImGui::Separator();

    ImGui::Text("Controls:");
    ImGui::Text("  F1: Toggle this debug window");
    ImGui::Text("  F2: Toggle cursor capture (%s)", m_mouseCaptured ? "CAPTURED" : "FREE");
    ImGui::Text("  F3: Toggle wireframe mode (%s)", m_wireframeMode ? "ON" : "OFF");
    ImGui::Text("  F4: Toggle chunk boundaries (%s)", m_showChunkBoundaries ? "ON" : "OFF");
    ImGui::Text("  ESC: Quit game");
    ImGui::Text("  WASD + Space/Shift: Move camera");
    ImGui::Text("  Ctrl + WASD: Fast movement (5x speed)");

    ImGui::End();
}

void GameClient::renderCrosshair() {
    // Only show crosshair when in game
    if (m_gameState != GameState::IN_GAME) return;

    // Get the viewport size
    ImGuiIO& io = ImGui::GetIO();
    float centerX = io.DisplaySize.x * 0.5f;
    float centerY = io.DisplaySize.y * 0.5f;

    // Create an invisible window that covers the entire screen
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("CrosshairOverlay", nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoBackground);

    // Get the draw list
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Draw crosshair lines
    const float crosshairSize = 10.0f;
    const float thickness = 2.0f;
    const ImU32 color = IM_COL32(255, 255, 255, 200); // Semi-transparent white

    // Horizontal line
    drawList->AddLine(
        ImVec2(centerX - crosshairSize, centerY),
        ImVec2(centerX + crosshairSize, centerY),
        color, thickness);

    // Vertical line
    drawList->AddLine(
        ImVec2(centerX, centerY - crosshairSize),
        ImVec2(centerX, centerY + crosshairSize),
        color, thickness);

    ImGui::End();
}

void GameClient::renderHotbar() {
    // Get the viewport size
    ImGuiIO& io = ImGui::GetIO();
    float screenWidth = io.DisplaySize.x;
    float screenHeight = io.DisplaySize.y;

    // Hotbar dimensions
    const float slotSize = 50.0f;
    const float slotSpacing = 5.0f;
    const float windowPadding = 32.0f; // Account for ImGui window padding (increased)
    const float hotbarWidth = HOTBAR_SLOTS * slotSize + (HOTBAR_SLOTS - 1) * slotSpacing + windowPadding;
    const float hotbarHeight = slotSize + 20.0f; // Extra space for slot numbers

    // Position hotbar at bottom center of screen
    float hotbarX = (screenWidth - hotbarWidth) * 0.5f;
    float hotbarY = screenHeight - hotbarHeight - 20.0f; // 20px from bottom

    // Create hotbar window
    ImGui::SetNextWindowPos(ImVec2(hotbarX, hotbarY));
    ImGui::SetNextWindowSize(ImVec2(hotbarWidth, hotbarHeight));
    ImGui::Begin("Hotbar", nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBackground);

    // Render hotbar slots
    for (int i = 0; i < HOTBAR_SLOTS; i++) {
        if (i > 0) ImGui::SameLine();

        // Push style for selected slot
        bool isSelected = (i == m_selectedHotbarSlot);
        if (isSelected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.8f, 0.2f, 0.8f)); // Yellow highlight
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.9f, 0.3f, 0.8f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 0.8f)); // Dark gray
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5f, 0.5f, 0.5f, 0.8f));
        }

        // Get texture info for this block type
        BlockType blockType = m_hotbarBlocks[i];
        TextureInfo texInfo = m_textureManager->getBlockTexture(blockType, 0); // Use face 0

        // Create image button with UV coordinates from atlas
        ImVec2 uv0(texInfo.uvMin.x, texInfo.uvMin.y);
        ImVec2 uv1(texInfo.uvMax.x, texInfo.uvMax.y);

        std::string buttonId = "##hotbar_" + std::to_string(i);
        if (ImGui::ImageButton(buttonId.c_str(), (ImTextureID)m_atlasTextureDescriptor, ImVec2(slotSize, slotSize), uv0, uv1)) {
            std::cout << "Hotbar clicked: changed from " << m_selectedHotbarSlot << " to " << i << std::endl;
            m_selectedHotbarSlot = i;
        }

        // Pop style
        ImGui::PopStyleColor(2);

        // Show tooltip with block name on hover
        if (ImGui::IsItemHovered()) {
            BlockType blockType = m_hotbarBlocks[i];
            const char* blockName = "Unknown";

            switch (blockType) {
                case BlockType::STONE: blockName = "Stone"; break;
                case BlockType::DIRT: blockName = "Dirt"; break;
                case BlockType::GRASS: blockName = "Grass"; break;
                case BlockType::WOOD: blockName = "Wood"; break;
                case BlockType::SAND: blockName = "Sand"; break;
                case BlockType::BRICK: blockName = "Brick"; break;
                case BlockType::COBBLESTONE: blockName = "Cobblestone"; break;
                case BlockType::SNOW: blockName = "Snow"; break;
                default: break;
            }

            ImGui::SetTooltip("%s", blockName);
        }
    }

    ImGui::End();
}

BlockHitResult GameClient::raycastVoxelsClient(const Ray& ray, ClientChunkManager* chunkManager, float maxDistance) {
    BlockHitResult result;

    if (!chunkManager) {
        return result;
    }

    // DDA (Digital Differential Analyzer) ray casting algorithm
    glm::vec3 currentPos = ray.origin;
    glm::vec3 rayStep = ray.direction * 0.1f; // Step size

    float distance = 0.0f;

    while (distance < maxDistance) {
        // Convert world position to voxel coordinates
        glm::ivec3 voxelPos = glm::ivec3(glm::floor(currentPos));

        // Check if this voxel is solid
        if (chunkManager->isVoxelSolidAtWorldPosition(voxelPos.x, voxelPos.y, voxelPos.z)) {
            result.hit = true;
            result.blockPosition = voxelPos;
            result.distance = distance;
            result.hitPoint = currentPos;

            // Calculate face normal by checking which face was hit
            glm::vec3 voxelCenter = glm::vec3(voxelPos) + glm::vec3(0.5f);
            glm::vec3 diff = currentPos - voxelCenter;

            // Find the axis with the largest absolute difference to determine face
            float absX = glm::abs(diff.x);
            float absY = glm::abs(diff.y);
            float absZ = glm::abs(diff.z);

            if (absX > absY && absX > absZ) {
                result.normal = glm::ivec3(diff.x > 0 ? 1 : -1, 0, 0);
            } else if (absY > absZ) {
                result.normal = glm::ivec3(0, diff.y > 0 ? 1 : -1, 0);
            } else {
                result.normal = glm::ivec3(0, 0, diff.z > 0 ? 1 : -1);
            }

            return result;
        }

        currentPos += rayStep;
        distance += glm::length(rayStep);
    }

    return result;
}

void GameClient::loadChunksAroundPlayer() {
    // Get player position in chunk coordinates
    glm::vec3 playerPos = m_camera.getPosition();
    int playerChunkX = static_cast<int>(std::floor(playerPos.x / CHUNK_SIZE));
    int playerChunkY = static_cast<int>(std::floor(playerPos.y / CHUNK_SIZE));
    int playerChunkZ = static_cast<int>(std::floor(playerPos.z / CHUNK_SIZE));

    // Define render distances (optimize for performance)
    const int horizontalRenderDistance = 4;  // Keep horizontal the same
    const int verticalRenderDistance = 2;    // Reduce vertical significantly for better performance

    bool needsLoading = false;
    std::vector<ChunkPos> chunksToRequest;

    // First pass: collect chunks that need loading in 3D with distance culling
    for (int dx = -horizontalRenderDistance; dx <= horizontalRenderDistance; dx++) {
        for (int dy = -verticalRenderDistance; dy <= verticalRenderDistance; dy++) {
            for (int dz = -horizontalRenderDistance; dz <= horizontalRenderDistance; dz++) {
                // Apply distance-based culling to avoid loading chunks too far away
                float horizontalDist = std::sqrt(dx * dx + dz * dz);
                if (horizontalDist > horizontalRenderDistance) {
                    continue; // Skip chunks outside circular horizontal render distance
                }

                ChunkPos chunkPos = {playerChunkX + dx, playerChunkY + dy, playerChunkZ + dz};

                // Check if chunk is already loaded
                if (!m_chunkManager->hasChunk(chunkPos)) {
                    chunksToRequest.push_back(chunkPos);
                    needsLoading = true;
                }
            }
        }
    }

    // Second pass: unload chunks that are too far away
    const float unloadDistance = horizontalRenderDistance + 1.5f; // Give some buffer
    std::vector<ChunkPos> chunksToUnload;

    // Check all loaded chunks for unloading
    std::vector<ChunkPos> loadedChunkPositions = m_chunkManager->getLoadedChunkPositions();
    for (const auto& pos : loadedChunkPositions) {
        float horizontalDist = std::sqrt((pos.x - playerChunkX) * (pos.x - playerChunkX) +
                                       (pos.z - playerChunkZ) * (pos.z - playerChunkZ));
        float verticalDist = std::abs(pos.y - playerChunkY);

        if (horizontalDist > unloadDistance || verticalDist > verticalRenderDistance + 1) {
            chunksToUnload.push_back(pos);
        }
    }

    // Unload chunks that are too far away (re-enabled after fixing Vulkan sync issues)
    if (!chunksToUnload.empty()) {
        std::cout << "Unloading " << chunksToUnload.size() << " distant chunks" << std::endl;
        for (const auto& chunkPos : chunksToUnload) {
            m_chunkManager->removeChunk(chunkPos);
        }
    }

    // Only print and request if there are chunks to load
    if (needsLoading) {
        std::cout << "Loading chunks around player at chunk (" << playerChunkX << ", " << playerChunkY << ", " << playerChunkZ
                  << ") - requesting " << chunksToRequest.size() << " chunks" << std::endl;

        for (const auto& chunkPos : chunksToRequest) {
            // std::cout << "Requesting chunk (" << chunkPos.x << ", " << chunkPos.y << ", " << chunkPos.z << ")" << std::endl;

            // Send chunk request to server
            ChunkRequestMessage request;
            request.chunkPos = chunkPos;
            NetworkPacket packet(MessageType::CHUNK_REQUEST, &request, sizeof(ChunkRequestMessage));
            sendToServer(packet);
        }
    }
}

void GameClient::startSingleplayerGame(const std::string& worldName) {
    std::cout << "Transitioning to singleplayer mode with world: " << worldName << std::endl;

    // Show loading screen immediately
    m_gameState = GameState::LOADING;

    // Initialize game systems that were skipped in menu mode
    if (!m_chunkManager) {
        m_chunkManager = std::make_unique<ClientChunkManager>(*m_device, m_textureManager.get());
    }
    if (!m_networkClient) {
        m_networkClient = std::make_unique<NetworkClient>();
        m_networkClient->setGameClient(this);
    }

    // Create integrated server for singleplayer
    ServerConfig serverConfig;
    serverConfig.worldName = worldName;
    serverConfig.serverName = "Local Server";
    serverConfig.maxPlayers = 1;
    serverConfig.enableRemoteAccess = false;  // Disable network server for integrated mode
    serverConfig.savesDirectory = m_saveDirectory;  // Use our managed save directory

    m_integratedServer = std::make_unique<GameServer>(serverConfig);

    // Initialize and start the server
    if (!m_integratedServer->initialize()) {
        std::cerr << "Failed to initialize integrated server!" << std::endl;
        return;
    }

    // Connect to local server
    connectToLocalServer(m_integratedServer.get());

    // Stay in loading state - will transition to IN_GAME when chunks are loaded
    // The transition happens in the update loop when sufficient chunks are loaded

    std::cout << "Server connected, loading initial chunks..." << std::endl;

    // Immediately request initial chunks around spawn
    if (m_chunkManager) {
        loadChunksAroundPlayer();
    }
}

void GameClient::initializeSaveDirectory() {
    // Get home directory
    const char* homeDir = std::getenv("HOME");
    if (!homeDir) {
        std::cerr << "Could not get HOME directory!" << std::endl;
        m_saveDirectory = "./saves";  // Fallback to local directory
    } else {
        m_saveDirectory = std::string(homeDir) + "/.tidal-engine/saves";
    }

    // Create the directory structure
    try {
        std::filesystem::create_directories(m_saveDirectory);
        std::cout << "Save directory: " << m_saveDirectory << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Failed to create save directory: " << e.what() << std::endl;
        m_saveDirectory = "./saves";  // Fallback
        std::filesystem::create_directories(m_saveDirectory);
    }
}

void GameClient::scanForWorlds() {
    m_availableWorlds.clear();

    try {
        if (std::filesystem::exists(m_saveDirectory)) {
            for (const auto& entry : std::filesystem::directory_iterator(m_saveDirectory)) {
                if (entry.is_directory()) {
                    std::string worldPath = entry.path().string();
                    std::string worldName = entry.path().filename().string();

                    // Only include worlds with proper structure
                    if (isValidWorld(worldPath)) {
                        m_availableWorlds.push_back(worldName);
                        std::cout << "Found valid world: " << worldName << std::endl;
                    } else {
                        std::cout << "Skipping invalid world directory: " << worldName << std::endl;
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error scanning for worlds: " << e.what() << std::endl;
    }

    if (m_availableWorlds.empty()) {
        std::cout << "No valid worlds found." << std::endl;
    }
}

bool GameClient::createNewWorld(const std::string& worldName) {
    if (worldName.empty()) {
        std::cerr << "World name cannot be empty!" << std::endl;
        return false;
    }

    try {
        // Use SaveSystem to properly create the world with data structures
        SaveSystem saveSystem(m_saveDirectory);

        if (saveSystem.createWorld(worldName)) {
            std::cout << "Created new world: " << worldName << " with proper data structures" << std::endl;

            // Rescan worlds to include the new one
            scanForWorlds();
            return true;
        } else {
            std::cerr << "SaveSystem failed to create world: " << worldName << std::endl;
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to create world '" << worldName << "': " << e.what() << std::endl;
        return false;
    }
}

bool GameClient::isValidWorld(const std::string& worldPath) const {
    // Check for required world files/directories
    std::string levelDat = worldPath + "/level.dat";
    std::string chunksDir = worldPath + "/chunks";

    bool hasLevelDat = std::filesystem::exists(levelDat);
    bool hasChunksDir = std::filesystem::exists(chunksDir) && std::filesystem::is_directory(chunksDir);

    return hasLevelDat && hasChunksDir;
}

std::string GameClient::getSaveDirectory() const {
    return m_saveDirectory;
}

void GameClient::cleanupImGui() {
    // Reset atlas texture descriptor (ImGui manages its cleanup)
    m_atlasTextureDescriptor = VK_NULL_HANDLE;

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    // Safely destroy ImGui descriptor pool
    if (m_device && m_imguiDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_device->getDevice(), m_imguiDescriptorPool, nullptr);
        m_imguiDescriptorPool = VK_NULL_HANDLE;
    }
}

void GameClient::cleanupVulkan() {
    std::cout << "Starting Vulkan cleanup..." << std::endl;

    // Cleanup Vulkan resources
    if (!m_device) {
        std::cout << "No device to cleanup, exiting." << std::endl;
        return; // Nothing to cleanup
    }

    // Store device handle before we potentially reset the device wrapper
    VkDevice device = m_device->getDevice();

    if (device != VK_NULL_HANDLE) {
        std::cout << "Waiting for device idle..." << std::endl;
        vkDeviceWaitIdle(device);
        std::cout << "Device idle." << std::endl;
    }

    // Clear buffers first
    std::cout << "Clearing buffers..." << std::endl;
    m_uniformBuffers.clear();
    m_indexBuffer.reset();
    m_vertexBuffer.reset();
    std::cout << "Buffers cleared." << std::endl;

    // Destroy Vulkan handles with proper null checking and error handling
    if (device != VK_NULL_HANDLE) {
        try {
            if (m_commandPool != VK_NULL_HANDLE) {
                std::cout << "Destroying command pool..." << std::endl;
                vkDestroyCommandPool(device, m_commandPool, nullptr);
                m_commandPool = VK_NULL_HANDLE;
                std::cout << "Command pool destroyed." << std::endl;
            }
        } catch (...) {
            std::cout << "Error destroying command pool" << std::endl;
        }

        try {
            if (m_descriptorPool != VK_NULL_HANDLE) {
                std::cout << "Destroying descriptor pool..." << std::endl;
                vkDestroyDescriptorPool(device, m_descriptorPool, nullptr);
                m_descriptorPool = VK_NULL_HANDLE;
                std::cout << "Descriptor pool destroyed." << std::endl;
            }
        } catch (...) {
            std::cout << "Error destroying descriptor pool" << std::endl;
        }

        try {
            if (m_descriptorSetLayout != VK_NULL_HANDLE) {
                std::cout << "Destroying descriptor set layout..." << std::endl;
                vkDestroyDescriptorSetLayout(device, m_descriptorSetLayout, nullptr);
                m_descriptorSetLayout = VK_NULL_HANDLE;
                std::cout << "Descriptor set layout destroyed." << std::endl;
            }
        } catch (...) {
            std::cout << "Error destroying descriptor set layout" << std::endl;
        }

        try {
            if (m_graphicsPipeline != VK_NULL_HANDLE) {
                std::cout << "Destroying graphics pipeline..." << std::endl;
                vkDestroyPipeline(device, m_graphicsPipeline, nullptr);
                m_graphicsPipeline = VK_NULL_HANDLE;
                std::cout << "Graphics pipeline destroyed." << std::endl;
            }
        } catch (...) {
            std::cout << "Error destroying graphics pipeline" << std::endl;
        }

        try {
            if (m_wireframePipeline != VK_NULL_HANDLE) {
                std::cout << "Destroying wireframe pipeline..." << std::endl;
                vkDestroyPipeline(device, m_wireframePipeline, nullptr);
                m_wireframePipeline = VK_NULL_HANDLE;
                std::cout << "Wireframe pipeline destroyed." << std::endl;
            }
        } catch (...) {
            std::cout << "Error destroying wireframe pipeline" << std::endl;
        }

        try {
            if (m_pipelineLayout != VK_NULL_HANDLE) {
                std::cout << "Destroying pipeline layout..." << std::endl;
                vkDestroyPipelineLayout(device, m_pipelineLayout, nullptr);
                m_pipelineLayout = VK_NULL_HANDLE;
                std::cout << "Pipeline layout destroyed." << std::endl;
            }
        } catch (...) {
            std::cout << "Error destroying pipeline layout" << std::endl;
        }

        // Skip render pass destruction entirely - it's likely already destroyed by VulkanRenderer
        if (m_renderPass != VK_NULL_HANDLE) {
            std::cout << "Render pass handle present, but skipping destruction (handled by renderer)." << std::endl;
            m_renderPass = VK_NULL_HANDLE;
        }
    }

    // Reset texture manager before device
    std::cout << "Resetting texture manager and device..." << std::endl;
    m_textureManager.reset();
    m_device.reset();
    std::cout << "Vulkan cleanup complete." << std::endl;
}

// Static callback implementations
void GameClient::framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    (void)width; (void)height; // Suppress unused parameter warnings
    auto client = reinterpret_cast<GameClient*>(glfwGetWindowUserPointer(window));
    if (client && client->m_renderer) {
        // Signal renderer to recreate swapchain on next frame
        client->m_renderer->recreateSwapChain();
    }
}

void GameClient::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    (void)scancode; // Suppress unused parameter warning
    auto client = reinterpret_cast<GameClient*>(glfwGetWindowUserPointer(window));
    client->handleKeyInput(key, action, mods);
}

void GameClient::mouseCallback(GLFWwindow* window, double xpos, double ypos) {
    auto client = reinterpret_cast<GameClient*>(glfwGetWindowUserPointer(window));
    client->handleMouseInput(xpos, ypos);
}

void GameClient::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    auto client = reinterpret_cast<GameClient*>(glfwGetWindowUserPointer(window));
    client->handleMouseButton(button, action, mods);
}

void GameClient::scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    (void)xoffset; (void)yoffset; // Suppress unused parameter warnings
    auto client = reinterpret_cast<GameClient*>(glfwGetWindowUserPointer(window));
    (void)client; // Suppress unused variable warning
    // Handle scroll
}

void GameClient::handleMouseInput(double xpos, double ypos) {
    if (!m_mouseCaptured) return;

    if (m_firstMouse) {
        m_lastX = xpos;
        m_lastY = ypos;
        m_firstMouse = false;
    }

    float xoffset = xpos - m_lastX;
    float yoffset = m_lastY - ypos; // Reversed since y-coordinates go from bottom to top

    m_lastX = xpos;
    m_lastY = ypos;

    m_camera.processMouseMovement(xoffset, yoffset);
}

void GameClient::handleKeyInput(int key, int action, int mods) {
    (void)mods; // Suppress unused parameter warning
    if (key >= 0 && key < 1024) {
        m_keys[key] = (action == GLFW_PRESS || action == GLFW_REPEAT);
    }
}

void GameClient::handleMouseButton(int button, int action, int mods) {
    (void)mods; // Suppress unused parameter warning

    std::cout << "Mouse button event: button=" << button << ", action=" << action << std::endl;

    // Don't handle mouse input if ImGui wants it
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) {
        std::cout << "ImGui wants mouse input, ignoring game input" << std::endl;
        return;
    }

    // Only handle input in game state
    if (m_gameState != GameState::IN_GAME) {
        std::cout << "Not in game state, ignoring mouse input" << std::endl;
        return;
    }

    // Only handle block interaction if cursor is captured
    if (!m_mouseCaptured) {
        std::cout << "Cursor not captured, ignoring block interaction" << std::endl;
        return;
    }

    if (action == GLFW_PRESS) {
        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            std::cout << "Left mouse button pressed - breaking block" << std::endl;
            handleBlockInteraction(true); // Break block
        } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
            std::cout << "Right mouse button pressed - placing block" << std::endl;
            handleBlockInteraction(false); // Place block
        }
    } else if (action == GLFW_RELEASE) {
        std::cout << "Mouse button released (no action)" << std::endl;
    }
}

// ClientChunkManager implementation
ClientChunkManager::ClientChunkManager(VulkanDevice& device, TextureManager* textureManager)
    : m_device(device), m_textureManager(textureManager), m_isFrameInProgress(false) {
}

size_t ClientChunkManager::renderVisibleChunks(VkCommandBuffer commandBuffer, const Frustum& frustum) {
    // Use a shared lock for reading chunks (or remove lock entirely if single-threaded)
    std::lock_guard<std::mutex> lock(m_chunkMutex);

    size_t renderedChunks = 0;

    for (const auto& [pos, chunk] : m_chunks) {
        if (chunk && !chunk->isEmpty()) {
            // Calculate chunk bounding box (could be cached per chunk)
            AABB chunkAABB;
            chunkAABB.min = glm::vec3(pos.x * CHUNK_SIZE, pos.y * CHUNK_SIZE, pos.z * CHUNK_SIZE);
            chunkAABB.max = chunkAABB.min + glm::vec3(CHUNK_SIZE, CHUNK_SIZE, CHUNK_SIZE);

            // Only render if the chunk is visible in the frustum and not completely occluded
            if (isAABBInFrustum(chunkAABB, frustum) && !isChunkCompletelyOccluded(pos)) {
                chunk->render(commandBuffer);
                renderedChunks++;
            }
        }
    }

    return renderedChunks;
}

void ClientChunkManager::renderChunkBoundaries(VkCommandBuffer commandBuffer, VkPipeline wireframePipeline, const glm::vec3& playerPosition, VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet) {
    // Only regenerate wireframe when chunks are loaded/unloaded, not for player movement
    if (m_wireframeDirty) {
        generateWireframeMesh(playerPosition);
        createWireframeBuffers();
        m_wireframeDirty = false;
    }

    if (m_wireframeVertexBuffer && !m_wireframeVertices.empty()) {
        // Bind the wireframe pipeline
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, wireframePipeline);

        // Bind descriptor sets for the wireframe pipeline
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

        VkBuffer vertexBuffers[] = {m_wireframeVertexBuffer->getBuffer()};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(commandBuffer, m_wireframeIndexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(m_wireframeIndices.size()), 1, 0, 0, 0);
    }
}

void ClientChunkManager::setChunkData(const ChunkPos& pos, const ChunkData& data) {
    std::lock_guard<std::mutex> lock(m_chunkMutex);

    auto chunk = std::make_unique<ClientChunk>(pos, m_device, m_textureManager, this);
    chunk->setChunkData(data);

    // Use deferred mesh generation for new chunks too to prevent queue conflicts
    if (m_isFrameInProgress) {
        // Queue for deferred update
        if (std::find(m_deferredMeshUpdates.begin(), m_deferredMeshUpdates.end(), pos) == m_deferredMeshUpdates.end()) {
            m_deferredMeshUpdates.push_back(pos);
        }
    } else {
        chunk->regenerateMesh(); // Generate mesh immediately only when safe
    }

    m_chunks[pos] = std::move(chunk);

    // Mark wireframe as dirty since we added a new chunk
    m_wireframeDirty = true;
}

void ClientChunkManager::removeChunk(const ChunkPos& pos) {
    // Ensure device is idle before removing chunks with GPU resources
    vkDeviceWaitIdle(m_device.getDevice());

    std::lock_guard<std::mutex> lock(m_chunkMutex);
    m_chunks.erase(pos);

    // Mark wireframe as dirty since we removed a chunk
    m_wireframeDirty = true;
}

void ClientChunkManager::updateBlock(const glm::ivec3& worldPos, BlockType blockType) {
    // Convert world position to chunk coordinates (handle negative coordinates correctly)
    ChunkPos chunkPos(
        worldPos.x >= 0 ? worldPos.x / CHUNK_SIZE : (worldPos.x - CHUNK_SIZE + 1) / CHUNK_SIZE,
        worldPos.y >= 0 ? worldPos.y / CHUNK_SIZE : (worldPos.y - CHUNK_SIZE + 1) / CHUNK_SIZE,
        worldPos.z >= 0 ? worldPos.z / CHUNK_SIZE : (worldPos.z - CHUNK_SIZE + 1) / CHUNK_SIZE
    );

    std::cout << "ClientChunkManager: Updating block at world (" << worldPos.x << ", " << worldPos.y << ", " << worldPos.z
              << ") -> chunk (" << chunkPos.x << ", " << chunkPos.y << ", " << chunkPos.z << ")" << std::endl;

    std::lock_guard<std::mutex> lock(m_chunkMutex);
    auto it = m_chunks.find(chunkPos);
    if (it != m_chunks.end()) {
        int localX = worldPos.x - (chunkPos.x * CHUNK_SIZE);
        int localY = worldPos.y - (chunkPos.y * CHUNK_SIZE);
        int localZ = worldPos.z - (chunkPos.z * CHUNK_SIZE);
        std::cout << "Found chunk, updating local position (" << localX << ", " << localY << ", " << localZ << ")" << std::endl;
        it->second->updateBlock(localX, localY, localZ, blockType);

        // Invalidate neighboring chunks if this block is at a chunk boundary
        // This ensures faces between chunks are properly updated
        invalidateNeighboringChunks(chunkPos, localX, localY, localZ);
    } else {
        std::cout << "ERROR: Chunk (" << chunkPos.x << ", " << chunkPos.y << ", " << chunkPos.z << ") not found!" << std::endl;
        std::cout << "Available chunks:" << std::endl;
        for (const auto& [pos, chunk] : m_chunks) {
            std::cout << "  (" << pos.x << ", " << pos.y << ", " << pos.z << ")" << std::endl;
        }
    }
}

size_t ClientChunkManager::getLoadedChunkCount() const {
    std::lock_guard<std::mutex> lock(m_chunkMutex);
    return m_chunks.size();
}

size_t ClientChunkManager::getTotalVertexCount() const {
    std::lock_guard<std::mutex> lock(m_chunkMutex);
    size_t total = 0;
    for (const auto& [pos, chunk] : m_chunks) {
        if (chunk) {
            total += chunk->getVertexCount();
        }
    }
    return total;
}

size_t ClientChunkManager::getTotalFaceCount() const {
    std::lock_guard<std::mutex> lock(m_chunkMutex);
    size_t total = 0;
    for (const auto& [pos, chunk] : m_chunks) {
        if (chunk) {
            total += chunk->getIndexCount() / 6; // 6 indices per quad (2 triangles)
        }
    }
    return total;
}

bool ClientChunkManager::hasChunk(const ChunkPos& pos) const {
    std::lock_guard<std::mutex> lock(m_chunkMutex);
    return m_chunks.find(pos) != m_chunks.end();
}

std::vector<ChunkPos> ClientChunkManager::getLoadedChunkPositions() const {
    std::lock_guard<std::mutex> lock(m_chunkMutex);
    std::vector<ChunkPos> positions;
    positions.reserve(m_chunks.size());

    for (const auto& [pos, chunk] : m_chunks) {
        positions.push_back(pos);
    }

    return positions;
}

void ClientChunkManager::updateDirtyMeshes() {
    std::lock_guard<std::mutex> lock(m_chunkMutex);

    // Update a limited number of dirty meshes per frame to avoid stuttering
    const int MAX_UPDATES_PER_FRAME = 3;
    int updatesThisFrame = 0;

    for (const auto& [pos, chunk] : m_chunks) {
        if (chunk && chunk->isMeshDirty() && updatesThisFrame < MAX_UPDATES_PER_FRAME) {
            // If a frame is in progress, defer the mesh update to avoid queue conflicts
            if (m_isFrameInProgress) {
                // Check if this chunk is already queued for deferred update
                if (std::find(m_deferredMeshUpdates.begin(), m_deferredMeshUpdates.end(), pos) == m_deferredMeshUpdates.end()) {
                    m_deferredMeshUpdates.push_back(pos);
                }
            } else {
                chunk->regenerateMesh();
                updatesThisFrame++;
            }
        }
    }
}

void ClientChunkManager::updateDirtyMeshesSafe() {
    std::lock_guard<std::mutex> lock(m_chunkMutex);

    // Process all deferred mesh updates safely
    if (m_deferredMeshUpdates.empty()) {
        return; // Nothing to process
    }

    std::cout << "Processing " << m_deferredMeshUpdates.size() << " deferred client mesh updates" << std::endl;

    for (const ChunkPos& pos : m_deferredMeshUpdates) {
        auto it = m_chunks.find(pos);
        if (it != m_chunks.end() && it->second->isMeshDirty()) {
            try {
                std::cout << "Processing deferred client mesh update for chunk (" << pos.x << ", " << pos.y << ", " << pos.z << ")" << std::endl;
                it->second->regenerateMesh();
            } catch (const std::exception& e) {
                std::cerr << "Error processing deferred client mesh update for chunk (" << pos.x << ", " << pos.y << ", " << pos.z << "): " << e.what() << std::endl;
                // Continue processing other chunks
            }
        }
    }
    m_deferredMeshUpdates.clear();
}

void ClientChunkManager::generateWireframeMesh(const glm::vec3& playerPosition) {
    std::lock_guard<std::mutex> lock(m_chunkMutex);
    m_wireframeVertices.clear();
    m_wireframeIndices.clear();

    // Note: playerPosition parameter kept for API consistency but not used in simplified version
    (void)playerPosition; // Suppress unused parameter warning

    // Generate wireframe boxes for each loaded chunk
    for (const auto& [pos, chunk] : m_chunks) {
        // Calculate chunk world position
        glm::vec3 chunkWorldPos = glm::vec3(
            static_cast<float>(pos.x * CHUNK_SIZE),
            static_cast<float>(pos.y * CHUNK_SIZE),
            static_cast<float>(pos.z * CHUNK_SIZE)
        );

        // Define the 8 corners of the chunk bounding box
        glm::vec3 corners[8] = {
            chunkWorldPos,                                                     // min corner
            chunkWorldPos + glm::vec3(CHUNK_SIZE, 0, 0),                     // +X
            chunkWorldPos + glm::vec3(0, CHUNK_SIZE, 0),                     // +Y
            chunkWorldPos + glm::vec3(0, 0, CHUNK_SIZE),                     // +Z
            chunkWorldPos + glm::vec3(CHUNK_SIZE, CHUNK_SIZE, 0),            // +X+Y
            chunkWorldPos + glm::vec3(CHUNK_SIZE, 0, CHUNK_SIZE),            // +X+Z
            chunkWorldPos + glm::vec3(0, CHUNK_SIZE, CHUNK_SIZE),            // +Y+Z
            chunkWorldPos + glm::vec3(CHUNK_SIZE, CHUNK_SIZE, CHUNK_SIZE)    // max corner
        };

        // Simple consistent color for all wireframes - bright enough to see but not distracting
        glm::vec3 color = glm::vec3(0.6f, 0.6f, 0.6f); // Light gray for all chunks

        uint32_t startIndex = static_cast<uint32_t>(m_wireframeVertices.size());

        // Add vertices
        for (int i = 0; i < 8; i++) {
            m_wireframeVertices.push_back({
                corners[i],                     // position
                glm::vec3(0.0f, 1.0f, 0.0f),   // normal (not used for wireframe)
                glm::vec2(0.0f, 0.0f),         // texCoord (not used for wireframe)
                glm::vec3(0.0f),               // worldOffset (already in world coords)
                color                          // color
            });
        }

        // Define the 12 edges of a cube
        uint32_t edges[12][2] = {
            // Bottom face edges
            {0, 1}, {1, 4}, {4, 2}, {2, 0},
            // Top face edges
            {3, 5}, {5, 7}, {7, 6}, {6, 3},
            // Vertical edges
            {0, 3}, {1, 5}, {4, 7}, {2, 6}
        };

        // Add indices for edges (lines)
        for (int i = 0; i < 12; i++) {
            m_wireframeIndices.push_back(startIndex + edges[i][0]);
            m_wireframeIndices.push_back(startIndex + edges[i][1]);
        }
    }
}

void ClientChunkManager::createWireframeBuffers() {
    if (m_wireframeVertices.empty()) return;

    // Create vertex buffer
    VkDeviceSize vertexBufferSize = sizeof(m_wireframeVertices[0]) * m_wireframeVertices.size();

    VkBuffer vertexStagingBuffer;
    VkDeviceMemory vertexStagingBufferMemory;
    m_device.createBuffer(vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                         vertexStagingBuffer, vertexStagingBufferMemory);

    void* vertexData;
    vkMapMemory(m_device.getDevice(), vertexStagingBufferMemory, 0, vertexBufferSize, 0, &vertexData);
    memcpy(vertexData, m_wireframeVertices.data(), static_cast<size_t>(vertexBufferSize));
    vkUnmapMemory(m_device.getDevice(), vertexStagingBufferMemory);

    m_wireframeVertexBuffer = std::make_unique<VulkanBuffer>(
        m_device,
        sizeof(m_wireframeVertices[0]),
        static_cast<uint32_t>(m_wireframeVertices.size()),
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    m_device.copyBuffer(vertexStagingBuffer, m_wireframeVertexBuffer->getBuffer(), vertexBufferSize);

    vkDestroyBuffer(m_device.getDevice(), vertexStagingBuffer, nullptr);
    vkFreeMemory(m_device.getDevice(), vertexStagingBufferMemory, nullptr);

    // Create index buffer
    VkDeviceSize indexBufferSize = sizeof(m_wireframeIndices[0]) * m_wireframeIndices.size();

    VkBuffer indexStagingBuffer;
    VkDeviceMemory indexStagingBufferMemory;
    m_device.createBuffer(indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                         indexStagingBuffer, indexStagingBufferMemory);

    void* indexData;
    vkMapMemory(m_device.getDevice(), indexStagingBufferMemory, 0, indexBufferSize, 0, &indexData);
    memcpy(indexData, m_wireframeIndices.data(), static_cast<size_t>(indexBufferSize));
    vkUnmapMemory(m_device.getDevice(), indexStagingBufferMemory);

    m_wireframeIndexBuffer = std::make_unique<VulkanBuffer>(
        m_device,
        sizeof(m_wireframeIndices[0]),
        static_cast<uint32_t>(m_wireframeIndices.size()),
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    m_device.copyBuffer(indexStagingBuffer, m_wireframeIndexBuffer->getBuffer(), indexBufferSize);

    vkDestroyBuffer(m_device.getDevice(), indexStagingBuffer, nullptr);
    vkFreeMemory(m_device.getDevice(), indexStagingBufferMemory, nullptr);
}

void ClientChunkManager::invalidateNeighboringChunks(const ChunkPos& chunkPos, int localX, int localY, int localZ) {
    // Check if the block is at a chunk boundary and invalidate neighboring chunks
    // This ensures that faces between chunks are properly updated when blocks change

    std::vector<ChunkPos> neighborsToUpdate;

    // Check each axis for boundary conditions
    if (localX == 0) {
        // Block is at the -X boundary, invalidate chunk to the left (-X)
        neighborsToUpdate.push_back({chunkPos.x - 1, chunkPos.y, chunkPos.z});
    }
    if (localX == CHUNK_SIZE - 1) {
        // Block is at the +X boundary, invalidate chunk to the right (+X)
        neighborsToUpdate.push_back({chunkPos.x + 1, chunkPos.y, chunkPos.z});
    }

    if (localY == 0) {
        // Block is at the -Y boundary, invalidate chunk below (-Y)
        neighborsToUpdate.push_back({chunkPos.x, chunkPos.y - 1, chunkPos.z});
    }
    if (localY == CHUNK_SIZE - 1) {
        // Block is at the +Y boundary, invalidate chunk above (+Y)
        neighborsToUpdate.push_back({chunkPos.x, chunkPos.y + 1, chunkPos.z});
    }

    if (localZ == 0) {
        // Block is at the -Z boundary, invalidate chunk behind (-Z)
        neighborsToUpdate.push_back({chunkPos.x, chunkPos.y, chunkPos.z - 1});
    }
    if (localZ == CHUNK_SIZE - 1) {
        // Block is at the +Z boundary, invalidate chunk in front (+Z)
        neighborsToUpdate.push_back({chunkPos.x, chunkPos.y, chunkPos.z + 1});
    }

    // Mark neighboring chunks as dirty so they regenerate their meshes
    for (const auto& neighborPos : neighborsToUpdate) {
        auto neighborIt = m_chunks.find(neighborPos);
        if (neighborIt != m_chunks.end()) {
            neighborIt->second->markAsModified(); // Mark as modified for occlusion culling

            // Use deferred mesh regeneration to avoid potential deadlocks
            if (m_isFrameInProgress) {
                // Queue for deferred update if frame is in progress
                if (std::find(m_deferredMeshUpdates.begin(), m_deferredMeshUpdates.end(), neighborPos) == m_deferredMeshUpdates.end()) {
                    m_deferredMeshUpdates.push_back(neighborPos);
                }
            } else {
                // Safe to regenerate immediately if no frame in progress
                neighborIt->second->regenerateMesh();
            }
        }
    }
}

bool ClientChunkManager::isAABBInFrustum(const AABB& aabb, const Frustum& frustum) const {
    // For each plane, check if the AABB is completely on the outside
    for (const auto& plane : frustum.planes) {
        // Find the positive vertex (the vertex of the AABB that is furthest along the plane normal)
        glm::vec3 positiveVertex = aabb.min;
        if (plane.normal.x >= 0) positiveVertex.x = aabb.max.x;
        if (plane.normal.y >= 0) positiveVertex.y = aabb.max.y;
        if (plane.normal.z >= 0) positiveVertex.z = aabb.max.z;

        // If the positive vertex is on the negative side of the plane, the AABB is outside
        if (plane.distanceToPoint(positiveVertex) < 0) {
            return false;
        }
    }

    // AABB is either intersecting or inside the frustum
    return true;
}

bool ClientChunkManager::isChunkCompletelyOccluded(const ChunkPos& pos) const {
    // Calculate world Y range for this chunk
    int chunkWorldYMax = pos.y * CHUNK_SIZE + CHUNK_SIZE - 1;

    // For flat world terrain (stone up to Y=10, air above):
    // Underground chunks (Y < 0) are occluded if:
    // 1. They're completely below ground level (Y max < 0)
    // 2. There are solid chunks above them (surface chunks exist)
    // 3. Neither the underground chunk nor surface chunk has been modified

    if (chunkWorldYMax < 0) {
        // This is an underground chunk - first check if it's been modified
        auto undergroundIt = m_chunks.find(pos);
        if (undergroundIt != m_chunks.end() && undergroundIt->second && undergroundIt->second->isModified()) {
            return false; // Underground chunk has been modified (dug into) - must render
        }

        // Check if surface chunk above exists and hasn't been modified
        ChunkPos surfaceChunk = {pos.x, 0, pos.z}; // Surface level chunk
        auto surfaceIt = m_chunks.find(surfaceChunk);
        if (surfaceIt != m_chunks.end() && surfaceIt->second && !surfaceIt->second->isEmpty()) {
            if (surfaceIt->second->isModified()) {
                return false; // Surface chunk modified (dug through) - underground is potentially visible
            }

            // Also check neighboring surface chunks for modifications that might expose this underground chunk
            std::vector<ChunkPos> neighbors = {
                {pos.x - 1, 0, pos.z}, {pos.x + 1, 0, pos.z},  // Left/Right
                {pos.x, 0, pos.z - 1}, {pos.x, 0, pos.z + 1}   // Front/Back
            };

            for (const auto& neighbor : neighbors) {
                auto neighborIt = m_chunks.find(neighbor);
                if (neighborIt != m_chunks.end() && neighborIt->second && neighborIt->second->isModified()) {
                    return false; // Neighboring surface chunk modified - might expose underground
                }
            }

            // Surface chunk exists and is unmodified, no modified neighbors - underground chunk is occluded
            return true;
        }
    }

    // Chunks at or above ground level are not occluded by simple depth test
    return false;
}

bool ClientChunkManager::isVoxelSolidAtWorldPosition(int worldX, int worldY, int worldZ) const {
    // Convert world coordinates to chunk position and local coordinates
    ChunkPos chunkPos(worldX / CHUNK_SIZE, worldY / CHUNK_SIZE, worldZ / CHUNK_SIZE);

    // Handle negative coordinates properly (floor division)
    if (worldX < 0 && worldX % CHUNK_SIZE != 0) chunkPos.x--;
    if (worldY < 0 && worldY % CHUNK_SIZE != 0) chunkPos.y--;
    if (worldZ < 0 && worldZ % CHUNK_SIZE != 0) chunkPos.z--;

    // Get local coordinates within the chunk
    int localX = worldX - (chunkPos.x * CHUNK_SIZE);
    int localY = worldY - (chunkPos.y * CHUNK_SIZE);
    int localZ = worldZ - (chunkPos.z * CHUNK_SIZE);

    // Ensure local coordinates are in valid range
    if (localX < 0) localX += CHUNK_SIZE;
    if (localY < 0) localY += CHUNK_SIZE;
    if (localZ < 0) localZ += CHUNK_SIZE;

    // Check if chunk is loaded
    std::lock_guard<std::mutex> lock(m_chunkMutex);
    auto it = m_chunks.find(chunkPos);
    if (it == m_chunks.end()) {
        return false; // Unloaded chunks are considered empty
    }

    // Check if the voxel is solid in the chunk
    return it->second->isVoxelSolid(localX, localY, localZ);
}

bool ClientChunkManager::isVoxelSolidAtWorldPositionUnsafe(int worldX, int worldY, int worldZ) const {
    // Convert world coordinates to chunk position and local coordinates (handle negative properly)
    ChunkPos chunkPos(worldX / CHUNK_SIZE, worldY / CHUNK_SIZE, worldZ / CHUNK_SIZE);

    // Handle negative coordinates properly (floor division)
    if (worldX < 0 && worldX % CHUNK_SIZE != 0) chunkPos.x--;
    if (worldY < 0 && worldY % CHUNK_SIZE != 0) chunkPos.y--;
    if (worldZ < 0 && worldZ % CHUNK_SIZE != 0) chunkPos.z--;

    // Get local coordinates within the chunk
    int localX = worldX - (chunkPos.x * CHUNK_SIZE);
    int localY = worldY - (chunkPos.y * CHUNK_SIZE);
    int localZ = worldZ - (chunkPos.z * CHUNK_SIZE);

    // Ensure local coordinates are in valid range
    if (localX < 0) localX += CHUNK_SIZE;
    if (localY < 0) localY += CHUNK_SIZE;
    if (localZ < 0) localZ += CHUNK_SIZE;

    // Check if chunk is loaded (NO LOCK - assumes caller has lock)
    auto it = m_chunks.find(chunkPos);
    if (it == m_chunks.end()) {
        return false; // Unloaded chunks are considered empty
    }

    // Check if the voxel is solid in the chunk
    return it->second->isVoxelSolid(localX, localY, localZ);
}

// ClientChunk implementation
ClientChunk::ClientChunk(const ChunkPos& position, VulkanDevice& device, TextureManager* textureManager, ClientChunkManager* chunkManager)
    : m_position(position), m_device(device), m_textureManager(textureManager), m_chunkManager(chunkManager) {
    std::memset(m_voxels, 0, sizeof(m_voxels));
}

bool ClientChunk::isVoxelSolid(int x, int y, int z) const {
    // Check bounds
    if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE) {
        return false;  // Out of bounds is considered air
    }
    return m_voxels[x][y][z] != BlockType::AIR;
}

BlockType ClientChunk::getVoxelType(int x, int y, int z) const {
    // Check bounds
    if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE) {
        return BlockType::AIR;  // Out of bounds is considered air
    }
    return m_voxels[x][y][z];
}

void ClientChunk::render(VkCommandBuffer commandBuffer) {
    // REMOVED: Automatic mesh regeneration during render to prevent queue conflicts
    // Mesh regeneration now handled by the deferred update system

    if (!m_vertexBuffer || m_vertices.empty()) {
        return;
    }

    VkBuffer vertexBuffers[] = {m_vertexBuffer->getBuffer()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

    if (m_indexBuffer) {
        vkCmdBindIndexBuffer(commandBuffer, m_indexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(m_indices.size()), 1, 0, 0, 0);
    }
}

void ClientChunk::setChunkData(const ChunkData& data) {
    std::memcpy(m_voxels, data.voxels, sizeof(m_voxels));
    m_meshDirty = true;
}

void ClientChunk::updateBlock(int x, int y, int z, BlockType blockType) {
    if (x >= 0 && x < CHUNK_SIZE && y >= 0 && y < CHUNK_SIZE && z >= 0 && z < CHUNK_SIZE) {
        m_voxels[x][y][z] = blockType;
        m_meshDirty = true;
        m_modified = true;  // Mark chunk as modified when any block changes
    }
}

void ClientChunk::regenerateMesh() {
    // Ensure device is idle before modifying buffers to prevent Vulkan queue conflicts
    vkDeviceWaitIdle(m_device.getDevice());

    size_t oldVertexCount = m_vertices.size();
    generateMesh();
    createBuffers();
    m_meshDirty = false;

    // Only log when vertex count changes significantly (for block placement)
    if (m_vertices.size() != oldVertexCount) {
        std::cout << "Regenerating mesh for chunk (" << m_position.x << ", " << m_position.y << ", " << m_position.z
                  << ") - vertices: " << oldVertexCount << " -> " << m_vertices.size() << std::endl;
    }
}

void ClientChunk::generateMesh() {
    m_vertices.clear();
    m_indices.clear();

    // Simple mesh generation (similar to Chunk class)
    for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int y = 0; y < CHUNK_SIZE; y++) {
            for (int z = 0; z < CHUNK_SIZE; z++) {
                BlockType blockType = m_voxels[x][y][z];
                if (blockType != BlockType::AIR) {
                    // Generate faces for visible sides
                    for (int face = 0; face < 6; face++) {
                        if (shouldRenderFace(x, y, z, face)) {
                            addVoxelFace(x, y, z, face, blockType);
                        }
                    }
                }
            }
        }
    }
}

void ClientChunk::addVoxelFace(int x, int y, int z, int faceDir, BlockType blockType) {
    // Calculate voxel position in world coordinates
    glm::vec3 voxelWorldPos = glm::vec3(m_position.x * CHUNK_SIZE + x, m_position.y * CHUNK_SIZE + y, m_position.z * CHUNK_SIZE + z);

    // Get texture coordinates for this block type from texture atlas
    glm::vec2 texOffset, texSize;
    if (m_textureManager) {
        TextureInfo texInfo = m_textureManager->getBlockTexture(blockType, faceDir);
        texOffset = texInfo.uvMin;
        texSize = texInfo.uvMax - texInfo.uvMin;
    } else {
        // Fallback to full atlas if no texture manager
        texOffset = glm::vec2(0.0f, 0.0f);
        texSize = glm::vec2(1.0f, 1.0f);
    }

    // Get color based on block type
    glm::vec3 color;
    switch (blockType) {
        case BlockType::GRASS: color = glm::vec3(0.2f, 0.8f, 0.2f); break;
        case BlockType::DIRT: color = glm::vec3(0.6f, 0.4f, 0.2f); break;
        case BlockType::STONE: color = glm::vec3(0.5f, 0.5f, 0.5f); break;
        case BlockType::WOOD: color = glm::vec3(0.4f, 0.3f, 0.1f); break;
        case BlockType::SAND: color = glm::vec3(0.9f, 0.8f, 0.6f); break;
        default: color = glm::vec3(1.0f, 1.0f, 1.0f); break;
    }

    uint32_t startIndex = static_cast<uint32_t>(m_vertices.size());

    // Define face vertices based on direction
    // Face directions: 0=+X, 1=-X, 2=+Y, 3=-Y, 4=+Z, 5=-Z
    switch (faceDir) {
        case 0: // +X face (right) - Counter-clockwise when viewed from outside
            m_vertices.push_back({{ 1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {texOffset.x, texOffset.y}, voxelWorldPos, color});
            m_vertices.push_back({{ 1.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {texOffset.x, texOffset.y + texSize.y}, voxelWorldPos, color});
            m_vertices.push_back({{ 1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {texOffset.x + texSize.x, texOffset.y + texSize.y}, voxelWorldPos, color});
            m_vertices.push_back({{ 1.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {texOffset.x + texSize.x, texOffset.y}, voxelWorldPos, color});
            break;
        case 1: // -X face (left) - Counter-clockwise when viewed from outside
            m_vertices.push_back({{ 0.0f, 0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {texOffset.x, texOffset.y}, voxelWorldPos, color});
            m_vertices.push_back({{ 0.0f, 1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {texOffset.x, texOffset.y + texSize.y}, voxelWorldPos, color});
            m_vertices.push_back({{ 0.0f, 1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, {texOffset.x + texSize.x, texOffset.y + texSize.y}, voxelWorldPos, color});
            m_vertices.push_back({{ 0.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, {texOffset.x + texSize.x, texOffset.y}, voxelWorldPos, color});
            break;
        case 2: // +Y face (top) - Counter-clockwise when viewed from outside
            m_vertices.push_back({{ 0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {texOffset.x, texOffset.y}, voxelWorldPos, color});
            m_vertices.push_back({{ 0.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {texOffset.x, texOffset.y + texSize.y}, voxelWorldPos, color});
            m_vertices.push_back({{ 1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {texOffset.x + texSize.x, texOffset.y + texSize.y}, voxelWorldPos, color});
            m_vertices.push_back({{ 1.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {texOffset.x + texSize.x, texOffset.y}, voxelWorldPos, color});
            break;
        case 3: // -Y face (bottom) - Counter-clockwise when viewed from outside
            m_vertices.push_back({{ 0.0f, 0.0f, 1.0f}, {0.0f, -1.0f, 0.0f}, {texOffset.x, texOffset.y}, voxelWorldPos, color});
            m_vertices.push_back({{ 0.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, {texOffset.x, texOffset.y + texSize.y}, voxelWorldPos, color});
            m_vertices.push_back({{ 1.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, {texOffset.x + texSize.x, texOffset.y + texSize.y}, voxelWorldPos, color});
            m_vertices.push_back({{ 1.0f, 0.0f, 1.0f}, {0.0f, -1.0f, 0.0f}, {texOffset.x + texSize.x, texOffset.y}, voxelWorldPos, color});
            break;
        case 4: // +Z face (front) - Counter-clockwise when viewed from outside
            m_vertices.push_back({{ 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {texOffset.x, texOffset.y}, voxelWorldPos, color});
            m_vertices.push_back({{ 0.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {texOffset.x, texOffset.y + texSize.y}, voxelWorldPos, color});
            m_vertices.push_back({{ 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {texOffset.x + texSize.x, texOffset.y + texSize.y}, voxelWorldPos, color});
            m_vertices.push_back({{ 1.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {texOffset.x + texSize.x, texOffset.y}, voxelWorldPos, color});
            break;
        case 5: // -Z face (back) - Counter-clockwise when viewed from outside
            m_vertices.push_back({{ 1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {texOffset.x, texOffset.y}, voxelWorldPos, color});
            m_vertices.push_back({{ 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {texOffset.x + texSize.x, texOffset.y}, voxelWorldPos, color});
            m_vertices.push_back({{ 0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {texOffset.x + texSize.x, texOffset.y + texSize.y}, voxelWorldPos, color});
            m_vertices.push_back({{ 1.0f, 1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {texOffset.x, texOffset.y + texSize.y}, voxelWorldPos, color});
            break;
    }

    // Add indices for the quad (2 triangles)
    m_indices.push_back(startIndex + 0);
    m_indices.push_back(startIndex + 1);
    m_indices.push_back(startIndex + 2);
    m_indices.push_back(startIndex + 2);
    m_indices.push_back(startIndex + 3);
    m_indices.push_back(startIndex + 0);
}

void GameClient::renderPerformanceHUD() {
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.8f);

    ImGui::Begin("Performance", &m_showPerformanceHUD,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);

    ImGui::Text("FPS: %.1f", m_debugInfo.fps);
    ImGui::Text("Frame Time: %.2f ms", m_debugInfo.frameTime);

    ImGui::End();
}

void GameClient::renderRenderingHUD() {
    ImGui::SetNextWindowPos(ImVec2(10, 100), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.8f);

    ImGui::Begin("Rendering", &m_showRenderingHUD,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);

    ImGui::Text("Draw Calls: %u", m_debugInfo.drawCalls);
    ImGui::Text("Total Voxels: %u", m_debugInfo.totalVoxels);
    ImGui::Text("Rendered Voxels: %u", m_debugInfo.renderedVoxels);
    ImGui::Text("Culled Voxels: %u", m_debugInfo.culledVoxels);

    float cullingEfficiency = m_debugInfo.totalVoxels > 0 ?
        (float)m_debugInfo.culledVoxels / m_debugInfo.totalVoxels * 100.0f : 0.0f;
    ImGui::Text("Culling Efficiency: %.1f%%", cullingEfficiency);

    ImGui::End();
}

void GameClient::renderCameraHUD() {
    ImGui::SetNextWindowPos(ImVec2(10, 220), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.8f);

    ImGui::Begin("Camera", &m_showCameraHUD,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);

    glm::vec3 pos = m_camera.getPosition();
    ImGui::Text("Position: (%.1f, %.1f, %.1f)", pos.x, pos.y, pos.z);
    ImGui::Text("Yaw: %.1f", m_camera.Yaw);
    ImGui::Text("Pitch: %.1f", m_camera.Pitch);

    ImGui::End();
}

bool ClientChunk::shouldRenderFace(int x, int y, int z, int faceDir) const {
    // Calculate adjacent block position
    int adjX = x, adjY = y, adjZ = z;

    switch (faceDir) {
        case 0: adjX++; break; // +X
        case 1: adjX--; break; // -X
        case 2: adjY++; break; // +Y
        case 3: adjY--; break; // -Y
        case 4: adjZ++; break; // +Z
        case 5: adjZ--; break; // -Z
    }

    // Check if adjacent block is within this chunk
    if (adjX >= 0 && adjX < CHUNK_SIZE &&
        adjY >= 0 && adjY < CHUNK_SIZE &&
        adjZ >= 0 && adjZ < CHUNK_SIZE) {
        // Adjacent block is within this chunk - check if it's solid
        return m_voxels[adjX][adjY][adjZ] == BlockType::AIR;
    }

    // Adjacent block is in a neighboring chunk - use world coordinates for cross-chunk culling
    if (m_chunkManager) {
        // Convert local position to world coordinates
        int worldX = m_position.x * CHUNK_SIZE + adjX;
        int worldY = m_position.y * CHUNK_SIZE + adjY;
        int worldZ = m_position.z * CHUNK_SIZE + adjZ;

        // Check if adjacent block in neighboring chunk is solid
        // Use unsafe version since setChunkData already holds the mutex
        return !m_chunkManager->isVoxelSolidAtWorldPositionUnsafe(worldX, worldY, worldZ);
    }

    // Fallback: render the face if no chunk manager available
    return true;
}

void ClientChunk::createBuffers() {
    if (m_vertices.empty()) {
        return;
    }

    // Create vertex buffer
    m_vertexBuffer = std::make_unique<VulkanBuffer>(
        m_device,
        sizeof(m_vertices[0]),              // instanceSize
        static_cast<uint32_t>(m_vertices.size()), // instanceCount
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    m_vertexBuffer->map();
    m_vertexBuffer->writeToBuffer(m_vertices.data());

    // Create index buffer if we have indices
    if (!m_indices.empty()) {
        m_indexBuffer = std::make_unique<VulkanBuffer>(
            m_device,
            sizeof(m_indices[0]),              // instanceSize
            static_cast<uint32_t>(m_indices.size()), // instanceCount
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );

        m_indexBuffer->map();
        m_indexBuffer->writeToBuffer(m_indices.data());
    }
}

