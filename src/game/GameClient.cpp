#include "game/GameClient.h"
#include "game/GameClientRenderer.h"
#include "game/GameClientInput.h"
#include "game/GameClientNetwork.h"
#include "game/GameClientUI.h"
#include "game/GameServer.h"
#include "network/NetworkManager.h"
#include "network/NetworkProtocol.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <cstdlib>
#include <array>
#include <cmath>


GameClient::GameClient(const ClientConfig& config)
    : m_config(config)
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

        // Initialize renderer after Vulkan device is ready
        m_renderer = std::make_unique<GameClientRenderer>(*this, m_window, *m_device);
        if (!m_renderer->initialize()) {
            throw std::runtime_error("Failed to initialize GameClientRenderer");
        }

        // Initialize input system
        m_input = std::make_unique<GameClientInput>(*this, m_window, m_camera);

        // Initialize network system
        m_network = std::make_unique<GameClientNetwork>(*this);
        if (!m_network->initialize()) {
            throw std::runtime_error("Failed to initialize GameClientNetwork");
        }

        // Only initialize game systems if not in menu mode
        if (m_gameState != GameState::MAIN_MENU) {
            // Initialize chunk manager for rendering
            m_chunkManager = std::make_unique<ClientChunkManager>(*m_device);
        }

        // Initialize UI system
        m_ui = std::make_unique<GameClientUI>(*this, m_window, *m_device);
        if (!m_ui->initialize()) {
            throw std::runtime_error("Failed to initialize UI system");
        }

        // Create atlas texture descriptor for UI after both texture manager and UI are ready
        if (m_textureManager) {
            m_ui->createAtlasTextureDescriptor();
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

        if (m_input) {
            m_input->processInput(m_deltaTime);
        }

        // Only process game messages when not in menu
        if (m_gameState != GameState::MAIN_MENU && m_network) {
            m_network->processIncomingMessages();
        }

        // Check if we should transition from LOADING to IN_GAME
        if (m_gameState == GameState::LOADING && m_chunkManager) {
            // Wait until we have some initial chunks loaded around spawn
            size_t loadedChunks = m_chunkManager->getLoadedChunkCount();
            if (loadedChunks >= 1) {  // At least spawn chunk loaded
                std::cout << "Initial chunks loaded (" << loadedChunks << "), starting game!" << std::endl;
                m_gameState = GameState::IN_GAME;
                if (m_input) {
                    m_input->setMouseCaptured(true);
                }
            }
        }

        // Delegate rendering to renderer
        if (m_renderer) {
            updateTransforms(m_deltaTime);
            m_renderer->drawFrame();
        }
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
    m_ui.reset();
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


void GameClient::connectToLocalServer(GameServer* localServer) {
    if (!localServer) return;

    std::cout << "Connecting to local server..." << std::endl;
    m_localServer = localServer;
    m_state = ClientState::CONNECTED;

    // Set up integrated server callback for message passing
    m_localServer->setIntegratedClientCallback([this](PlayerId playerId, const NetworkPacket& packet) {
        // Only process messages for our local player
        if (playerId == m_localPlayerId) {
            // Process message through network system
            m_network->processServerMessage(packet);
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

    // Delegate to network system
    if (m_network) {
        m_network->disconnect();
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

    // Input state reset is handled by GameClientInput

    // Reset debug/UI state through UI system
    if (m_ui) {
        m_ui->setShowDebugWindow(false);
        m_ui->setShowPerformanceHUD(false);
        m_ui->setShowRenderingHUD(false);
        m_ui->setShowCameraHUD(false);
        m_ui->setShowChunkBoundaries(false);
        m_ui->setShowCreateWorldDialog(false);
    }

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



void GameClient::onBlockPlace(const glm::ivec3& position, BlockType blockType) {
    if (!isConnected()) return;

    // Send block place message to server
    BlockPlaceMessage msg;
    msg.position = position;
    msg.blockType = blockType;
    msg.timestamp = static_cast<uint32_t>(glfwGetTime() * 1000);

    NetworkPacket packet(MessageType::BLOCK_PLACE, &msg, sizeof(msg));
    if (m_network) {
        m_network->sendToServer(packet);
    }
}

void GameClient::onBlockBreak(const glm::ivec3& position) {
    if (!isConnected()) return;

    // Send block break message to server
    BlockBreakMessage msg;
    msg.position = position;
    msg.timestamp = static_cast<uint32_t>(glfwGetTime() * 1000);

    NetworkPacket packet(MessageType::BLOCK_BREAK, &msg, sizeof(msg));
    if (m_network) {
        m_network->sendToServer(packet);
    }
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
    if (m_network) {
        m_network->sendToServer(packet);
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

void GameClient::processServerMessage(const NetworkPacket& packet) {
    // Delegate to network system
    if (m_network) {
        m_network->processServerMessage(packet);
    }
}

void GameClient::handleChunkData(const ChunkDataMessage& msg, const std::vector<uint8_t>& compressedData) {
    if (!m_chunkManager) {
        std::cerr << "Cannot process chunk data: chunk manager not initialized" << std::endl;
        return;
    }

    // Reconstruct chunk data from message
    ChunkData chunkData;
    chunkData.position = msg.chunkPos;

    if (!msg.isEmpty) {
        // The compressedData now contains only the voxel data (no header)
        if (compressedData.size() >= sizeof(chunkData.voxels)) {
            // Copy voxel data directly from compressed data
            std::memcpy(chunkData.voxels, compressedData.data(), sizeof(chunkData.voxels));
        } else {
            std::cerr << "Compressed chunk data too small: expected " << sizeof(chunkData.voxels)
                      << " bytes, got " << compressedData.size() << " bytes" << std::endl;
            return;
        }
    } else {
        // Empty chunk - initialize with air blocks
        std::memset(chunkData.voxels, static_cast<int>(BlockType::AIR), sizeof(chunkData.voxels));
    }

    // Set chunk data in client chunk manager
    m_chunkManager->setChunkData(msg.chunkPos, chunkData);
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
    glfwSetKeyCallback(m_window, GameClientInput::keyCallback);
    glfwSetCursorPosCallback(m_window, GameClientInput::mouseCallback);
    glfwSetMouseButtonCallback(m_window, GameClientInput::mouseButtonCallback);
    glfwSetScrollCallback(m_window, GameClientInput::scrollCallback);

    // Cursor mode will be managed by GameClientInput once it's initialized
}

void GameClient::initVulkan() {
    m_device = std::make_unique<VulkanDevice>(m_window);

    // Initialize texture system (after device)
    m_userDataManager = std::make_unique<UserDataManager>();
    m_textureManager = std::make_unique<TextureManager>(*m_device, m_userDataManager.get());
    m_textureManager->loadActiveTexturepack();
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






void GameClient::saveAndQuitToTitle() {
    // Save the world
    if (m_integratedServer) {
        m_integratedServer->saveWorld();
    }

    // Disconnect from server
    disconnect();

    // Reset to main menu
    m_gameState = GameState::MAIN_MENU;
    if (m_input) {
        m_input->setMouseCaptured(false);
    }
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
            if (m_network) {
                m_network->sendToServer(packet);
            }
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
    // Network client is managed by GameClientNetwork

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

    // Cleanup renderer first (handles all Vulkan rendering resources)
    if (m_renderer) {
        m_renderer->shutdown();
        m_renderer.reset();
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

