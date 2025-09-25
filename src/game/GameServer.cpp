#include "game/GameServer.h"
#include "network/NetworkManager.h"
#include "network/NetworkProtocol.h"
#include <iostream>
#include <sstream>
#include <chrono>
#include <thread>
#include <cstring>
#include <cmath>


GameServer::GameServer(const ServerConfig& config)
    : m_config(config) {
}

GameServer::~GameServer() {
    if (m_running) {
        shutdown();
    }
}

bool GameServer::initialize() {
    if (m_initialized) {
        return true;
    }

    std::cout << "Initializing Tidal Engine Server..." << std::endl;
    std::cout << "Server Name: " << m_config.serverName << std::endl;
    std::cout << "World: " << m_config.worldName << std::endl;
    std::cout << "Port: " << m_config.port << std::endl;

    // Initialize save system
    m_saveSystem = std::make_unique<SaveSystem>(m_config.savesDirectory);

    // Load or create world
    if (!m_saveSystem->loadWorld(m_config.worldName)) {
        std::cout << "Creating new world: " << m_config.worldName << std::endl;
        if (!m_saveSystem->createWorld(m_config.worldName)) {
            std::cerr << "Failed to create world!" << std::endl;
            return false;
        }
    } else {
        std::cout << "Loaded existing world: " << m_config.worldName << std::endl;
    }

    // Initialize task manager (multi-threading)
    m_taskManager = std::make_unique<ServerTaskManager>();

    // Initialize chunk manager
    m_chunkManager = std::make_unique<ServerChunkManager>(m_saveSystem.get(), m_taskManager.get());

    // Initialize world generator (basic for now)
    // m_worldGenerator = std::make_unique<ServerWorldGenerator>();

    // Initialize network server (only for client builds with full networking)
    // Note: Integrated servers don't need network servers, they use direct callbacks
    #ifndef HEADLESS_SERVER
    // Don't create network server for integrated servers - they use integrated client callbacks
    if (!m_config.enableRemoteAccess) {
        std::cout << "Integrated server mode - using direct client callbacks" << std::endl;
    } else {
        m_networkServer = std::make_unique<NetworkServer>();
        m_networkServer->setGameServer(this);
        std::cout << "Network server mode - accepting remote connections" << std::endl;
    }
    #endif

    m_initialized = true;
    std::cout << "Server initialized successfully!" << std::endl;
    return true;
}

void GameServer::run() {
    if (!initialize()) {
        std::cerr << "Failed to initialize server!" << std::endl;
        return;
    }

    m_running = true;
    std::cout << "Starting server on port " << m_config.port << "..." << std::endl;
    std::cout << "Server is ready! Type 'help' for commands." << std::endl;

    // Start server thread
    m_serverThread = std::thread(&GameServer::serverLoop, this);

    // Console input loop (main thread)
    std::string input;
    while (m_running && std::getline(std::cin, input)) {
        if (!input.empty()) {
            processConsoleCommand(input);
        }
    }

    // Wait for server thread to finish
    if (m_serverThread.joinable()) {
        m_serverThread.join();
    }
}

void GameServer::shutdown() {
    if (!m_running) {
        return;
    }

    std::cout << "Shutting down server..." << std::endl;
    m_running = false;

    // Save world before shutdown
    saveWorld();

    // Disconnect all players
    std::lock_guard<std::mutex> lock(m_playerMutex);
    for (auto& [playerId, player] : m_players) {
        if (player.connected) {
            // Send disconnect message
            DisconnectMessage msg;
            std::strcpy(msg.reason, "Server shutting down");
            NetworkPacket packet(MessageType::DISCONNECT, &msg, sizeof(msg));
            sendToPlayer(playerId, packet);
        }
    }
    m_players.clear();

    std::cout << "Server shutdown complete." << std::endl;
}

void GameServer::serverLoop() {
    auto lastTime = std::chrono::high_resolution_clock::now();

    while (m_running) {
        auto currentTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
        lastTime = currentTime;

        processTick(deltaTime);

        // Sleep to maintain tick rate
        auto tickDuration = std::chrono::duration<float>(TICK_INTERVAL);
        std::this_thread::sleep_for(tickDuration);
    }
}

void GameServer::processTick(float deltaTime) {
    m_serverTime += deltaTime;

    // Process events
    processEvents();

    // Update players
    updatePlayers(deltaTime);

    // Check for autosave
    checkAutosave(deltaTime);

    // Call server tick hook
    if (m_hooks.onServerTick) {
        m_hooks.onServerTick(deltaTime);
    }
}

void GameServer::processEvents() {
    GameEvent event;
    while (m_eventQueue.pop(event)) {

        // Process event based on type
        switch (event.type) {
            case GameEvent::PLAYER_JOINED:
                std::cout << "Player " << event.playerId << " joined the game" << std::endl;
                break;

            case GameEvent::PLAYER_LEFT:
                std::cout << "Player " << event.playerId << " left the game" << std::endl;
                break;

            case GameEvent::PLAYER_MOVED:
                // Player movement updates are handled elsewhere
                break;

            case GameEvent::BLOCK_PLACED:
                broadcastBlockUpdate(event.blockPlace.position, event.blockPlace.blockType, event.playerId);
                break;

            case GameEvent::BLOCK_BROKEN:
                broadcastBlockUpdate(event.blockBreak.position, BlockType::AIR, event.playerId);
                break;

            case GameEvent::CHAT_SENT:
                std::cout << "[CHAT] Player " << event.playerId << ": " << event.chat.message << std::endl;
                break;
        }
    }
}

void GameServer::updatePlayers(float /* deltaTime */) {
    std::lock_guard<std::mutex> lock(m_playerMutex);

    for (auto& [playerId, player] : m_players) {
        if (player.connected) {
            // Load chunks around player
            m_chunkManager->loadChunksAroundPosition(player.position, m_config.renderDistance);
        }
    }
}

void GameServer::checkAutosave(float deltaTime) {
    m_lastAutosave += deltaTime;
    if (m_lastAutosave >= m_config.autosaveInterval) {
        std::cout << "Auto-saving world..." << std::endl;
        saveWorld();
        m_lastAutosave = 0.0f;
    }
}

void GameServer::saveWorld() {
    if (!m_saveSystem) return;

    // Submit world save as background task
    m_taskManager->submit_world_save([this]() {
        // Save all chunks
        m_chunkManager->saveAllChunks();

        // Save world metadata
        m_saveSystem->saveWorld();

        // Call save hook
        if (m_hooks.onWorldSave) {
            m_hooks.onWorldSave();
        }

        std::cout << "World saved successfully." << std::endl;
    });
}

bool GameServer::addPlayer(const std::string& name, PlayerId& outPlayerId) {
    std::lock_guard<std::mutex> lock(m_playerMutex);

    // Check server capacity
    if (m_players.size() >= m_config.maxPlayers) {
        return false;
    }

    // Check for duplicate names
    for (const auto& [id, player] : m_players) {
        if (player.name == name) {
            return false;
        }
    }

    // Create new player
    PlayerId playerId = m_nextPlayerId++;
    PlayerState& player = m_players[playerId];
    player.id = playerId;
    player.name = name;
    player.position = glm::vec3(0.0f, 18.0f, 0.0f);  // Default spawn
    player.yaw = 0.0f;
    player.pitch = 0.0f;
    player.lastUpdate = static_cast<uint32_t>(m_serverTime * 1000);
    player.connected = true;

    outPlayerId = playerId;

    // Add join event
    GameEvent event;
    event.type = GameEvent::PLAYER_JOINED;
    event.playerId = playerId;
    event.timestamp = player.lastUpdate;
    addEvent(event);

    // Call join hook
    if (m_hooks.onPlayerJoin && !m_hooks.onPlayerJoin(player)) {
        m_players.erase(playerId);
        return false;
    }

    return true;
}

void GameServer::removePlayer(PlayerId playerId) {
    std::lock_guard<std::mutex> lock(m_playerMutex);

    auto it = m_players.find(playerId);
    if (it != m_players.end()) {
        // Call leave hook
        if (m_hooks.onPlayerLeave) {
            m_hooks.onPlayerLeave(playerId);
        }

        // Add leave event
        GameEvent event;
        event.type = GameEvent::PLAYER_LEFT;
        event.playerId = playerId;
        event.timestamp = static_cast<uint32_t>(m_serverTime * 1000);
        addEvent(event);

        m_players.erase(it);
    }
}

PlayerState* GameServer::getPlayer(PlayerId playerId) {
    std::lock_guard<std::mutex> lock(m_playerMutex);
    auto it = m_players.find(playerId);
    return (it != m_players.end()) ? &it->second : nullptr;
}

std::vector<PlayerState> GameServer::getConnectedPlayers() const {
    std::lock_guard<std::mutex> lock(m_playerMutex);
    std::vector<PlayerState> players;
    for (const auto& [id, player] : m_players) {
        if (player.connected) {
            players.push_back(player);
        }
    }
    return players;
}

size_t GameServer::getPlayerCount() const {
    std::lock_guard<std::mutex> lock(m_playerMutex);
    return m_players.size();
}

bool GameServer::placeBlock(PlayerId playerId, const glm::ivec3& position, BlockType blockType) {
    // Check permissions with hook
    if (m_hooks.onBlockPlace && !m_hooks.onBlockPlace(playerId, position, blockType)) {
        return false;
    }

    // Place block in world
    m_chunkManager->setBlock(position, blockType);

    // Add event
    GameEvent event;
    event.type = GameEvent::BLOCK_PLACED;
    event.playerId = playerId;
    event.timestamp = static_cast<uint32_t>(m_serverTime * 1000);
    event.blockPlace.position = position;
    event.blockPlace.blockType = blockType;
    addEvent(event);

    return true;
}

bool GameServer::breakBlock(PlayerId playerId, const glm::ivec3& position) {
    // Check permissions with hook
    if (m_hooks.onBlockBreak && !m_hooks.onBlockBreak(playerId, position)) {
        return false;
    }

    // Break block in world
    m_chunkManager->setBlock(position, BlockType::AIR);

    // Add event
    GameEvent event;
    event.type = GameEvent::BLOCK_BROKEN;
    event.playerId = playerId;
    event.timestamp = static_cast<uint32_t>(m_serverTime * 1000);
    event.blockBreak.position = position;
    addEvent(event);

    return true;
}

BlockType GameServer::getBlock(const glm::ivec3& position) const {
    return m_chunkManager->getBlock(position);
}

void GameServer::addEvent(const GameEvent& event) {
    m_eventQueue.push(event);
}

void GameServer::broadcastPlayerUpdate(const PlayerState& player) {
    PlayerUpdateMessage msg;
    msg.playerId = player.id;
    msg.position = player.position;
    msg.yaw = player.yaw;
    msg.pitch = player.pitch;
    msg.flags = 0;

    NetworkPacket packet(MessageType::PLAYER_UPDATE, &msg, sizeof(msg));
    sendToAll(packet);
}

void GameServer::broadcastBlockUpdate(const glm::ivec3& position, BlockType blockType, PlayerId causedBy) {
    BlockUpdateMessage msg;
    msg.position = position;
    msg.blockType = blockType;
    msg.causedBy = causedBy;

    NetworkPacket packet(MessageType::BLOCK_UPDATE, &msg, sizeof(msg));
    sendToAll(packet);
}

// Console commands
void GameServer::processConsoleCommand(const std::string& command) {
    std::istringstream iss(command);
    std::string cmd;
    iss >> cmd;

    if (cmd == "help") {
        cmdHelp();
    } else if (cmd == "stop") {
        cmdStop();
    } else if (cmd == "save") {
        cmdSave();
    } else if (cmd == "list") {
        cmdList();
    } else if (cmd == "status") {
        cmdStatus();
    } else if (cmd == "kick") {
        std::string playerName, reason;
        iss >> playerName;
        std::getline(iss, reason);
        cmdKick(playerName, reason);
    } else if (cmd == "broadcast") {
        std::string message;
        std::getline(iss, message);
        cmdBroadcast(message);
    } else {
        std::cout << "Unknown command: " << cmd << ". Type 'help' for available commands." << std::endl;
    }
}

void GameServer::cmdHelp() {
    std::cout << "Available commands:" << std::endl;
    std::cout << "  help          - Show this help message" << std::endl;
    std::cout << "  stop          - Stop the server" << std::endl;
    std::cout << "  save          - Save the world" << std::endl;
    std::cout << "  list          - List connected players" << std::endl;
    std::cout << "  status        - Show server status" << std::endl;
    std::cout << "  kick <player> - Kick a player" << std::endl;
    std::cout << "  broadcast <msg> - Send message to all players" << std::endl;
}

void GameServer::cmdStop() {
    std::cout << "Stopping server..." << std::endl;
    m_running = false;
}

void GameServer::cmdSave() {
    saveWorld();
}

void GameServer::cmdList() {
    auto players = getConnectedPlayers();
    std::cout << "Connected players (" << players.size() << "/" << m_config.maxPlayers << "):" << std::endl;
    for (const auto& player : players) {
        std::cout << "  " << player.name << " (ID: " << player.id << ")" << std::endl;
    }
}

void GameServer::cmdStatus() {
    std::cout << "Server Status:" << std::endl;
    std::cout << "  Running: " << (m_running ? "Yes" : "No") << std::endl;
    std::cout << "  Players: " << getPlayerCount() << "/" << m_config.maxPlayers << std::endl;
    std::cout << "  World: " << m_config.worldName << std::endl;
    std::cout << "  Uptime: " << static_cast<int>(m_serverTime) << " seconds" << std::endl;
}

void GameServer::cmdKick(const std::string& /* playerName */, const std::string& /* reason */) {
    // Implementation would find player by name and kick them
    std::cout << "Kick command not fully implemented yet" << std::endl;
}

void GameServer::cmdBroadcast(const std::string& message) {
    std::cout << "[BROADCAST] " << message << std::endl;
    // Implementation would send to all connected players
}

void GameServer::processMessage(PlayerId playerId, const NetworkPacket& packet) {
    switch (packet.header.type) {
        case MessageType::CLIENT_HELLO: {
            if (packet.payload.size() == sizeof(ClientHelloMessage)) {
                ClientHelloMessage helloMsg;
                std::memcpy(&helloMsg, packet.payload.data(), sizeof(helloMsg));

                // Validate protocol version
                ServerHelloMessage response{};
                response.protocolVersion = NETWORK_PROTOCOL_VERSION;
                response.playerId = playerId;
                response.spawnPosition = glm::vec3(-32.0f, 50.0f, 0.0f); // Above center of loaded chunks

                if (helloMsg.protocolVersion == NETWORK_PROTOCOL_VERSION) {
                    // Add player to server
                    PlayerId actualPlayerId;
                    if (addPlayer(std::string(helloMsg.playerName), actualPlayerId)) {
                        response.playerId = actualPlayerId;
                        response.accepted = true;
                        strncpy(response.reason, "Welcome to the server!", sizeof(response.reason) - 1);
                        std::cout << "Player " << helloMsg.playerName << " joined (ID: " << actualPlayerId << ")" << std::endl;
                    } else {
                        response.accepted = false;
                        strncpy(response.reason, "Server full or invalid name", sizeof(response.reason) - 1);
                    }
                } else {
                    response.accepted = false;
                    strncpy(response.reason, "Protocol version mismatch", sizeof(response.reason) - 1);
                }

                NetworkPacket responsePacket(MessageType::SERVER_HELLO, &response, sizeof(response));
                sendToPlayer(playerId, responsePacket);
            }
            break;
        }

        case MessageType::PLAYER_LEAVE: {
            removePlayer(playerId);
            std::cout << "Player " << playerId << " left the server" << std::endl;
            break;
        }

        case MessageType::PLAYER_MOVE: {
            if (packet.payload.size() == sizeof(PlayerMoveMessage)) {
                PlayerMoveMessage moveMsg;
                std::memcpy(&moveMsg, packet.payload.data(), sizeof(moveMsg));

                PlayerState* player = getPlayer(playerId);
                if (player) {
                    player->position = moveMsg.position;
                    player->yaw = moveMsg.yaw;
                    player->pitch = moveMsg.pitch;
                    player->lastUpdate = moveMsg.timestamp;

                    // Broadcast to other players
                    PlayerUpdateMessage updateMsg{};
                    updateMsg.playerId = playerId;
                    updateMsg.position = moveMsg.position;
                    updateMsg.yaw = moveMsg.yaw;
                    updateMsg.pitch = moveMsg.pitch;
                    updateMsg.flags = 0;

                    NetworkPacket updatePacket(MessageType::PLAYER_UPDATE, &updateMsg, sizeof(updateMsg));
                    sendToAllExcept(playerId, updatePacket);
                }
            }
            break;
        }

        case MessageType::BLOCK_PLACE: {
            if (packet.payload.size() == sizeof(BlockPlaceMessage)) {
                BlockPlaceMessage placeMsg;
                std::memcpy(&placeMsg, packet.payload.data(), sizeof(placeMsg));

                std::cout << "Attempting to place block..." << std::endl;
                if (placeBlock(playerId, placeMsg.position, placeMsg.blockType)) {
                    std::cout << "placeBlock() succeeded, broadcasting update..." << std::endl;
                    // Broadcast block update to all players
                    BlockUpdateMessage updateMsg{};
                    updateMsg.position = placeMsg.position;
                    updateMsg.blockType = placeMsg.blockType;
                    updateMsg.causedBy = playerId;

                    NetworkPacket updatePacket(MessageType::BLOCK_UPDATE, &updateMsg, sizeof(updateMsg));
                    sendToAll(updatePacket);

                    std::cout << "Player " << playerId << " placed block at ("
                              << placeMsg.position.x << ", " << placeMsg.position.y << ", " << placeMsg.position.z << ")" << std::endl;
                } else {
                    std::cout << "placeBlock() FAILED for player " << playerId << std::endl;
                }
            }
            break;
        }

        case MessageType::BLOCK_BREAK: {
            if (packet.payload.size() == sizeof(BlockBreakMessage)) {
                BlockBreakMessage breakMsg;
                std::memcpy(&breakMsg, packet.payload.data(), sizeof(breakMsg));

                std::cout << "Attempting to break block..." << std::endl;
                if (breakBlock(playerId, breakMsg.position)) {
                    std::cout << "breakBlock() succeeded, broadcasting update..." << std::endl;
                    // Broadcast block update to all players
                    BlockUpdateMessage updateMsg{};
                    updateMsg.position = breakMsg.position;
                    updateMsg.blockType = BlockType::AIR;
                    updateMsg.causedBy = playerId;

                    NetworkPacket updatePacket(MessageType::BLOCK_UPDATE, &updateMsg, sizeof(updateMsg));
                    sendToAll(updatePacket);

                    std::cout << "Player " << playerId << " broke block at ("
                              << breakMsg.position.x << ", " << breakMsg.position.y << ", " << breakMsg.position.z << ")" << std::endl;
                } else {
                    std::cout << "breakBlock() FAILED for player " << playerId << std::endl;
                }
            }
            break;
        }

        case MessageType::CHUNK_REQUEST: {
            if (packet.payload.size() == sizeof(ChunkRequestMessage)) {
                ChunkRequestMessage chunkMsg;
                std::memcpy(&chunkMsg, packet.payload.data(), sizeof(chunkMsg));

                requestChunk(playerId, chunkMsg.chunkPos);
            }
            break;
        }

        case MessageType::CHAT_MESSAGE: {
            if (packet.payload.size() == sizeof(ChatMessage)) {
                ChatMessage chatMsg;
                std::memcpy(&chatMsg, packet.payload.data(), sizeof(chatMsg));

                PlayerState* player = getPlayer(playerId);
                if (player) {
                    std::cout << "[CHAT] " << player->name << ": " << chatMsg.message << std::endl;

                    // Broadcast to all players
                    sendToAll(packet);
                }
            }
            break;
        }

        case MessageType::PING: {
            if (packet.payload.size() == sizeof(PingMessage)) {
                PingMessage pingMsg;
                std::memcpy(&pingMsg, packet.payload.data(), sizeof(pingMsg));

                // Send PONG response
                NetworkPacket pongPacket(MessageType::PONG, &pingMsg, sizeof(pingMsg));
                sendToPlayer(playerId, pongPacket);
            }
            break;
        }

        case MessageType::DISCONNECT: {
            if (packet.payload.size() == sizeof(DisconnectMessage)) {
                DisconnectMessage disconnectMsg;
                std::memcpy(&disconnectMsg, packet.payload.data(), sizeof(disconnectMsg));

                std::cout << "Player " << playerId << " disconnected: " << disconnectMsg.reason << std::endl;
                removePlayer(playerId);
            }
            break;
        }

        default:
            std::cout << "Unknown message type " << static_cast<int>(packet.header.type)
                      << " received from player " << playerId << std::endl;
            break;
    }
}

// Networking methods
void GameServer::sendToPlayer(PlayerId playerId, const NetworkPacket& packet) {
    // Try integrated client callback first (for singleplayer)
    if (m_integratedClientCallback) {
        m_integratedClientCallback(playerId, packet);
        return;
    }

    #ifndef HEADLESS_SERVER
    // Try network server (for multiplayer)
    if (m_networkServer) {
        m_networkServer->sendToPlayer(playerId, packet);
        return;
    }
    #endif

    #ifdef HEADLESS_SERVER
    std::cout << "DEBUG: Would send packet type " << static_cast<int>(packet.header.type)
              << " to player " << playerId << std::endl;
    #else
    std::cout << "Warning: No way to send packet to player " << playerId << std::endl;
    #endif
}

void GameServer::sendToAll(const NetworkPacket& packet) {
    std::cout << "GameServer::sendToAll called with packet type " << static_cast<int>(packet.header.type) << std::endl;
    #ifndef HEADLESS_SERVER
    if (m_networkServer) {
        std::cout << "Using network server to send packet" << std::endl;
        m_networkServer->sendToAll(packet);
    } else if (m_integratedClientCallback) {
        std::cout << "Using integrated client callback, sending to " << m_players.size() << " players" << std::endl;
        // Send to integrated client for each player
        for (const auto& [playerId, player] : m_players) {
            std::cout << "Calling integrated callback for player " << playerId << std::endl;
            m_integratedClientCallback(playerId, packet);
        }
    } else {
        std::cout << "ERROR: No network server and no integrated client callback!" << std::endl;
    }
    #else
    std::cout << "DEBUG: Would broadcast packet type " << static_cast<int>(packet.header.type)
              << " to all players" << std::endl;
    #endif
}

void GameServer::sendToAllExcept(PlayerId excludePlayerId, const NetworkPacket& packet) {
    #ifndef HEADLESS_SERVER
    if (m_networkServer) {
        m_networkServer->sendToAllExcept(excludePlayerId, packet);
    } else if (m_integratedClientCallback) {
        // Send to integrated client for each player except excluded one
        for (const auto& [playerId, player] : m_players) {
            if (playerId != excludePlayerId) {
                m_integratedClientCallback(playerId, packet);
            }
        }
    }
    #else
    std::cout << "DEBUG: Would broadcast packet type " << static_cast<int>(packet.header.type)
              << " to all except player " << excludePlayerId << std::endl;
    #endif
}

bool GameServer::startNetworkServer(uint16_t port, const std::string& bindAddress) {
    #ifndef HEADLESS_SERVER
    if (!m_networkServer) {
        std::cerr << "Network server not initialized!" << std::endl;
        return false;
    }
    return m_networkServer->start(port, bindAddress);
    #else
    std::cout << "DEBUG: Would start network server on " << bindAddress << ":" << port << std::endl;
    std::cout << "Note: This is a placeholder implementation for the headless server" << std::endl;
    std::cout << "For full networking, build the TidalEngine client instead" << std::endl;
    return true; // Return true to allow server to continue
    #endif
}

void GameServer::stopNetworkServer() {
    #ifndef HEADLESS_SERVER
    if (m_networkServer) {
        m_networkServer->stop();
    }
    #else
    std::cout << "DEBUG: Would stop network server" << std::endl;
    #endif
}

bool GameServer::isNetworkServerRunning() const {
    #ifndef HEADLESS_SERVER
    return m_networkServer && m_networkServer->isRunning();
    #else
    return false; // Return false since this is just a placeholder
    #endif
}

// ServerChunkManager implementation
ServerChunkManager::ServerChunkManager(SaveSystem* saveSystem, ServerTaskManager* taskManager)
    : m_saveSystem(saveSystem), m_taskManager(taskManager) {
}

void ServerChunkManager::loadChunk(const ChunkPos& pos) {
    {
        std::lock_guard<std::mutex> lock(m_chunkMutex);
        if (m_loadedChunks.find(pos) != m_loadedChunks.end()) {
            return; // Already loaded
        }
    }

    // Submit chunk loading as async task to avoid blocking game thread
    if (m_taskManager) {
        m_taskManager->submit_chunk_generation([this, pos]() {
            auto chunk = std::make_unique<ServerChunk>(pos);

            // Try to load chunk from save first, then generate if not found
            ChunkData chunkData;
            if (m_saveSystem && m_saveSystem->loadChunk(pos, chunkData)) {
                chunk->setChunkData(chunkData);
                std::cout << "Loaded chunk (" << pos.x << ", " << pos.y << ", " << pos.z << ") from save" << std::endl;
            } else {
                // Generate new terrain (CPU-intensive operation)
                chunk->generateTerrain();
                std::cout << "Generated new chunk (" << pos.x << ", " << pos.y << ", " << pos.z << ")" << std::endl;
            }

            // Add to loaded chunks (thread-safe)
            {
                std::lock_guard<std::mutex> lock(m_chunkMutex);
                m_loadedChunks[pos] = std::move(chunk);
            }
        });
    } else {
        // Fallback to synchronous loading
        auto chunk = std::make_unique<ServerChunk>(pos);
        ChunkData chunkData;
        if (m_saveSystem && m_saveSystem->loadChunk(pos, chunkData)) {
            chunk->setChunkData(chunkData);
            std::cout << "Loaded chunk (" << pos.x << ", " << pos.y << ", " << pos.z << ") from save (sync)" << std::endl;
        } else {
            chunk->generateTerrain();
            std::cout << "Generated new chunk (" << pos.x << ", " << pos.y << ", " << pos.z << ") (sync)" << std::endl;
        }

        std::lock_guard<std::mutex> lock(m_chunkMutex);
        m_loadedChunks[pos] = std::move(chunk);
    }
}

bool ServerChunkManager::isChunkLoaded(const ChunkPos& pos) const {
    std::lock_guard<std::mutex> lock(m_chunkMutex);
    return m_loadedChunks.find(pos) != m_loadedChunks.end();
}

BlockType ServerChunkManager::getBlock(const glm::ivec3& worldPos) const {
    ChunkPos chunkPos = worldPositionToChunkPos(glm::vec3(worldPos));

    std::lock_guard<std::mutex> lock(m_chunkMutex);
    auto it = m_loadedChunks.find(chunkPos);
    if (it == m_loadedChunks.end()) {
        return BlockType::AIR; // Chunk not loaded
    }

    // Convert world position to local chunk coordinates
    int localX = worldPos.x - (chunkPos.x * CHUNK_SIZE);
    int localY = worldPos.y - (chunkPos.y * CHUNK_SIZE);
    int localZ = worldPos.z - (chunkPos.z * CHUNK_SIZE);

    // Ensure coordinates are within chunk bounds
    if (localX < 0 || localX >= CHUNK_SIZE ||
        localY < 0 || localY >= CHUNK_SIZE ||
        localZ < 0 || localZ >= CHUNK_SIZE) {
        return BlockType::AIR;
    }

    return it->second->getVoxel(localX, localY, localZ);
}

void ServerChunkManager::setBlock(const glm::ivec3& worldPos, BlockType blockType) {
    ChunkPos chunkPos = worldPositionToChunkPos(glm::vec3(worldPos));

    // Load chunk if not already loaded
    if (!isChunkLoaded(chunkPos)) {
        loadChunk(chunkPos);
    }

    std::lock_guard<std::mutex> lock(m_chunkMutex);
    auto it = m_loadedChunks.find(chunkPos);
    if (it == m_loadedChunks.end()) {
        return; // Failed to load chunk
    }

    // Convert world position to local chunk coordinates
    int localX = worldPos.x - (chunkPos.x * CHUNK_SIZE);
    int localY = worldPos.y - (chunkPos.y * CHUNK_SIZE);
    int localZ = worldPos.z - (chunkPos.z * CHUNK_SIZE);

    // Ensure coordinates are within chunk bounds
    if (localX < 0 || localX >= CHUNK_SIZE ||
        localY < 0 || localY >= CHUNK_SIZE ||
        localZ < 0 || localZ >= CHUNK_SIZE) {
        return;
    }

    it->second->setVoxel(localX, localY, localZ, blockType);
}

void ServerChunkManager::loadChunksAroundPosition(const glm::vec3& position, int radius) {
    ChunkPos centerChunk = worldPositionToChunkPos(position);

    // Load chunks in a radius around the player
    for (int x = centerChunk.x - radius; x <= centerChunk.x + radius; x++) {
        for (int z = centerChunk.z - radius; z <= centerChunk.z + radius; z++) {
            // Only load at ground level for now (y = 0)
            ChunkPos chunkPos(x, 0, z);
            if (!isChunkLoaded(chunkPos)) {
                loadChunk(chunkPos);
            }
        }
    }
}

void ServerChunkManager::unloadDistantChunks(const glm::vec3& position, int unloadDistance) {
    ChunkPos centerChunk = worldPositionToChunkPos(position);
    std::vector<ChunkPos> chunksToUnload;

    {
        std::lock_guard<std::mutex> lock(m_chunkMutex);
        for (const auto& [chunkPos, chunk] : m_loadedChunks) {
            // Calculate distance from center chunk
            int dx = chunkPos.x - centerChunk.x;
            int dz = chunkPos.z - centerChunk.z;
            int distance = std::max(std::abs(dx), std::abs(dz));

            if (distance > unloadDistance) {
                chunksToUnload.push_back(chunkPos);
            }
        }
    }

    // Unload distant chunks
    for (const ChunkPos& pos : chunksToUnload) {
        unloadChunk(pos);
    }
}

void ServerChunkManager::unloadChunk(const ChunkPos& pos) {
    std::lock_guard<std::mutex> lock(m_chunkMutex);

    auto it = m_loadedChunks.find(pos);
    if (it != m_loadedChunks.end()) {
        // Save chunk if dirty before unloading
        if (m_saveSystem && it->second->isDirty()) {
            ChunkData chunkData;
            it->second->getChunkData(chunkData);
            m_saveSystem->saveChunk(chunkData);
        }

        m_loadedChunks.erase(it);
    }
}

bool ServerChunkManager::getChunkData(const ChunkPos& pos, ChunkData& outData) const {
    std::lock_guard<std::mutex> lock(m_chunkMutex);

    auto it = m_loadedChunks.find(pos);
    if (it != m_loadedChunks.end()) {
        it->second->getChunkData(outData);
        return true;
    }
    return false;
}

ChunkPos ServerChunkManager::worldPositionToChunkPos(const glm::vec3& worldPos) const {
    int chunkX = static_cast<int>(std::floor(worldPos.x / CHUNK_SIZE));
    int chunkY = static_cast<int>(std::floor(worldPos.y / CHUNK_SIZE));
    int chunkZ = static_cast<int>(std::floor(worldPos.z / CHUNK_SIZE));
    return ChunkPos(chunkX, chunkY, chunkZ);
}

void ServerChunkManager::saveAllChunks() {
    if (!m_saveSystem) return;

    std::lock_guard<std::mutex> lock(m_chunkMutex);
    for (auto& [pos, chunk] : m_loadedChunks) {
        if (chunk->isDirty()) {
            ChunkData chunkData;
            chunk->getChunkData(chunkData);
            m_saveSystem->saveChunk(chunkData);
            chunk->markClean();
        }
    }
}

// ServerChunk implementation
ServerChunk::ServerChunk(const ChunkPos& position)
    : m_position(position) {
    // Initialize all voxels to air
    std::memset(m_voxels, 0, sizeof(m_voxels));
}

BlockType ServerChunk::getVoxel(int x, int y, int z) const {
    if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE) {
        return BlockType::AIR;
    }
    return m_voxels[x][y][z];
}

void ServerChunk::setVoxel(int x, int y, int z, BlockType blockType) {
    if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE) {
        return;
    }
    if (m_voxels[x][y][z] != blockType) {
        m_voxels[x][y][z] = blockType;
        m_dirty = true;
    }
}

void ServerChunk::getChunkData(ChunkData& outData) const {
    outData.position = m_position;
    std::memcpy(outData.voxels, m_voxels, sizeof(m_voxels));
}

void ServerChunk::setChunkData(const ChunkData& data) {
    std::memcpy(m_voxels, data.voxels, sizeof(m_voxels));
    m_generated = true;
    m_dirty = false;
}

void ServerChunk::generateTerrain() {
    // Generate completely flat world for maximum performance
    for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int z = 0; z < CHUNK_SIZE; z++) {
            for (int y = 0; y < CHUNK_SIZE; y++) {
                // Create flat terrain based on absolute world Y position
                int worldY = m_position.y * CHUNK_SIZE + y;

                if (worldY <= 10) {
                    // Flat stone ground up to Y=10
                    m_voxels[x][y][z] = BlockType::STONE;
                } else {
                    // Air above Y=10
                    m_voxels[x][y][z] = BlockType::AIR;
                }
            }
        }
    }

    m_generated = true;
    m_dirty = true;
}

// Missing GameServer method implementation
void GameServer::requestChunk(PlayerId playerId, const ChunkPos& chunkPos) {
    if (!m_chunkManager) {
        std::cerr << "Cannot request chunk: chunk manager not initialized" << std::endl;
        return;
    }

    // std::cout << "Player " << playerId << " requested chunk ("
    //           << chunkPos.x << ", " << chunkPos.y << ", " << chunkPos.z << ")" << std::endl;

    // Load chunk if not already loaded
    if (!m_chunkManager->isChunkLoaded(chunkPos)) {
        m_chunkManager->loadChunk(chunkPos);
    }

    // Get chunk data and send to player
    ChunkData chunkData;
    if (m_chunkManager->getChunkData(chunkPos, chunkData)) {
        // Create chunk data message with header
        ChunkDataMessage msg;
        msg.chunkPos = chunkPos;
        msg.isEmpty = chunkData.isEmpty();

        // Create packet with variable-size payload
        size_t messageSize = sizeof(ChunkDataMessage);
        size_t totalSize = messageSize;

        if (!msg.isEmpty) {
            // Add voxel data size
            totalSize += sizeof(chunkData.voxels);
        }

        // Create packet with proper size
        std::vector<uint8_t> packetData(totalSize);

        // Copy message header
        std::memcpy(packetData.data(), &msg, sizeof(ChunkDataMessage));

        // Copy voxel data if chunk is not empty
        if (!msg.isEmpty) {
            std::memcpy(packetData.data() + sizeof(ChunkDataMessage),
                       chunkData.voxels, sizeof(chunkData.voxels));
        }

        // Create and send packet
        NetworkPacket packet(MessageType::CHUNK_DATA, packetData.data(), totalSize);
        sendToPlayer(playerId, packet);

        // std::cout << "Sent chunk (" << chunkPos.x << ", " << chunkPos.y << ", " << chunkPos.z
        //           << ") to player " << playerId << " (empty: " << (msg.isEmpty ? "yes" : "no") << ")" << std::endl;
    } else {
        // std::cerr << "Failed to get chunk data for chunk (" << chunkPos.x << ", " << chunkPos.y << ", " << chunkPos.z << ")" << std::endl;
    }
}