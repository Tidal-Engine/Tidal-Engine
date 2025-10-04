#include "server/GameServer.hpp"
#include "server/World.hpp"
#include "shared/Protocol.hpp"
#include "shared/ChunkSerializer.hpp"
#include "core/Logger.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <chrono>
#include <thread>
#include <stdexcept>
#include <cstring>
#include <fstream>
#include <sstream>
#include <iostream>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <filesystem>

namespace engine {

GameServer::GameServer(uint16_t port, double tickRate)
    : port(port), tickRate(tickRate), tickDuration(1.0 / tickRate) {

    LOG_INFO("Initializing game server on port {} at {} TPS", port, tickRate);

    // Initialize ENet
    if (enet_initialize() != 0) {
        LOG_ERROR("Failed to initialize ENet");
        throw std::runtime_error("Failed to initialize ENet");
    }

    // Create world
    world = std::make_unique<World>();

    // Try to load existing world
    size_t loadedChunks = world->loadWorld("world");
    if (loadedChunks == 0) {
        LOG_INFO("No existing world found, generating new world");
        world->generateInitialChunks();
    }

    LOG_INFO("Game server initialized successfully");
}

GameServer::~GameServer() {
    if (running) {
        stop();
    }

    // Stop tunnel if running
    if (tunnelRunning) {
        stopTunnel();
    }

    // Save world before shutting down
    LOG_INFO("Saving world before shutdown...");
    world->saveWorld("world");

    cleanupNetworking();
    enet_deinitialize();
}

void GameServer::run() {
    LOG_INFO("Starting server main loop...");
    running = true;

    initNetworking();

    auto lastTick = std::chrono::steady_clock::now();

    while (running) {
        auto now = std::chrono::steady_clock::now();
        double deltaTime = std::chrono::duration<double>(now - lastTick).count();

        if (deltaTime >= tickDuration) {
            // Process one server tick
            tick();

            lastTick = now;
            currentTick++;

            // Log every 200 ticks (~5 seconds at 40 TPS)
            if (currentTick % 200 == 0) {
                LOG_TRACE("Server tick: {} | Loaded chunks: {}",
                         currentTick, world->getLoadedChunkCount());
            }

            // Autosave every 12000 ticks (5 minutes at 40 TPS)
            if (currentTick % 12000 == 0) {
                LOG_INFO("Autosaving world...");
                size_t saved = world->saveWorld("world");
                if (saved > 0) {
                    LOG_INFO("Autosave complete: {} chunks saved", saved);
                }
            }
        } else {
            // Sleep to avoid busy-waiting (1ms granularity)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    LOG_INFO("Server main loop ended");
}

void GameServer::stop() {
    LOG_INFO("Stopping server...");
    running = false;
}

void GameServer::initNetworking() {
    LOG_INFO("Initializing server networking on port {}...", port);

    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = port;

    // Create server host
    // Parameters: address, max clients, channels, incoming bandwidth, outgoing bandwidth
    server = enet_host_create(&address, 32, 2, 0, 0);

    if (server == nullptr) {
        LOG_ERROR("Failed to create ENet server host");
        throw std::runtime_error("Failed to create ENet server host");
    }

    LOG_INFO("Server listening on port {}", port);
}

void GameServer::tick() {
    // 1. Process network events
    processNetworkEvents();

    // 2. Update world state
    world->update();

    // 3. Update player chunks periodically (once per second at 40 TPS)
    if (currentTick % 40 == 0) {
        updatePlayerChunks();
    }

    // 4. TODO: Update entities, physics, etc.

    // 5. TODO: Send state updates to clients
}

void GameServer::processNetworkEvents() {
    ENetEvent event;

    // Process all pending events (non-blocking with 0ms timeout)
    while (enet_host_service(server, &event, 0) > 0) {
        switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT:
                onClientConnect(event.peer);
                break;

            case ENET_EVENT_TYPE_DISCONNECT:
                onClientDisconnect(event.peer);
                break;

            case ENET_EVENT_TYPE_RECEIVE:
                onClientPacket(event.peer, event.packet);
                enet_packet_destroy(event.packet);
                break;

            default:
                break;
        }
    }
}

void GameServer::onClientConnect(ENetPeer* peer) {
    // Create player data (spawn at origin, Y=5)
    PlayerData playerData;
    playerData.playerId = nextPlayerId++;
    playerData.position = glm::vec3(0.0f, 5.0f, 0.0f);
    players[peer] = playerData;

    LOG_INFO("========================================");
    LOG_INFO(">>> PLAYER JOINED <<<");
    LOG_INFO("Player ID: {}", playerData.playerId);
    LOG_INFO("Address: {}:{}", peer->address.host, peer->address.port);
    LOG_INFO("Spawn position: ({:.1f}, {:.1f}, {:.1f})",
             playerData.position.x, playerData.position.y, playerData.position.z);
    LOG_INFO("Players online: {}", players.size());
    LOG_INFO("========================================");

    // Send all existing players to the new player
    for (const auto& [otherPeer, otherPlayer] : players) {
        if (otherPeer != peer) {
            protocol::PlayerSpawnMessage spawnMsg;
            spawnMsg.playerId = otherPlayer.playerId;
            spawnMsg.spawnPosition = otherPlayer.position;
            std::snprintf(spawnMsg.playerName, sizeof(spawnMsg.playerName), "Player %u", otherPlayer.playerId);

            size_t totalSize = sizeof(protocol::MessageHeader) + sizeof(protocol::PlayerSpawnMessage);
            ENetPacket* packet = enet_packet_create(nullptr, totalSize, ENET_PACKET_FLAG_RELIABLE);

            protocol::MessageHeader header;
            header.type = protocol::MessageType::PlayerSpawn;
            header.payloadSize = sizeof(protocol::PlayerSpawnMessage);
            std::memcpy(packet->data, &header, sizeof(protocol::MessageHeader));
            std::memcpy(packet->data + sizeof(protocol::MessageHeader), &spawnMsg, sizeof(spawnMsg));

            enet_peer_send(peer, 0, packet);
        }
    }

    // Broadcast new player spawn to all existing players
    protocol::PlayerSpawnMessage spawnMsg;
    spawnMsg.playerId = playerData.playerId;
    spawnMsg.spawnPosition = playerData.position;
    std::snprintf(spawnMsg.playerName, sizeof(spawnMsg.playerName), "Player %u", playerData.playerId);

    size_t totalSize = sizeof(protocol::MessageHeader) + sizeof(protocol::PlayerSpawnMessage);
    ENetPacket* packet = enet_packet_create(nullptr, totalSize, ENET_PACKET_FLAG_RELIABLE);

    protocol::MessageHeader header;
    header.type = protocol::MessageType::PlayerSpawn;
    header.payloadSize = sizeof(protocol::PlayerSpawnMessage);
    std::memcpy(packet->data, &header, sizeof(protocol::MessageHeader));
    std::memcpy(packet->data + sizeof(protocol::MessageHeader), &spawnMsg, sizeof(spawnMsg));

    for (const auto& [otherPeer, otherPlayer] : players) {
        if (otherPeer != peer) {
            enet_peer_send(otherPeer, 0, enet_packet_create(packet->data, packet->dataLength, ENET_PACKET_FLAG_RELIABLE));
        }
    }
    enet_packet_destroy(packet);

    // Send chunks in radius around spawn point
    sendChunksAroundPlayer(peer, playerData.position);
    players[peer].lastChunkUpdatePos = playerData.position;
}

void GameServer::onClientDisconnect(ENetPeer* peer) {
    // Find the player ID before removing
    auto it = players.find(peer);
    if (it != players.end()) {
        uint32_t disconnectedPlayerId = it->second.playerId;

        // Broadcast player removal to all other clients
        protocol::PlayerRemoveMessage removeMsg;
        removeMsg.playerId = disconnectedPlayerId;

        std::vector<uint8_t> packet;
        packet.resize(sizeof(protocol::MessageHeader) + sizeof(protocol::PlayerRemoveMessage));

        protocol::MessageHeader header;
        header.type = protocol::MessageType::PlayerRemove;
        header.payloadSize = sizeof(protocol::PlayerRemoveMessage);

        std::memcpy(packet.data(), &header, sizeof(protocol::MessageHeader));
        std::memcpy(packet.data() + sizeof(protocol::MessageHeader), &removeMsg, sizeof(protocol::PlayerRemoveMessage));

        // Send to all OTHER clients
        for (const auto& [otherPeer, playerData] : players) {
            if (otherPeer != peer) {
                ENetPacket* enetPacket = enet_packet_create(packet.data(), packet.size(), ENET_PACKET_FLAG_RELIABLE);
                enet_peer_send(otherPeer, 0, enetPacket);
            }
        }

        // Remove player from tracking
        players.erase(it);

        LOG_INFO("========================================");
        LOG_INFO("<<< PLAYER LEFT >>>");
        LOG_INFO("Player ID: {}", disconnectedPlayerId);
        LOG_INFO("Address: {}:{}", peer->address.host, peer->address.port);
        LOG_INFO("Players remaining: {}", players.size());
        LOG_INFO("========================================");
    }
}

void GameServer::onClientPacket(ENetPeer* peer, ENetPacket* packet) {
    if (packet->dataLength < sizeof(protocol::MessageHeader)) {
        LOG_WARN("Received malformed packet from client");
        return;
    }

    // Read message header
    protocol::MessageHeader header;
    std::memcpy(&header, packet->data, sizeof(protocol::MessageHeader));

    // Handle different message types
    switch (header.type) {
        case protocol::MessageType::ClientJoin:
            LOG_INFO("Client join request received");
            break;

        case protocol::MessageType::PlayerMove: {
            if (packet->dataLength < sizeof(protocol::MessageHeader) + sizeof(protocol::PlayerMoveMessage)) {
                LOG_WARN("Received invalid PlayerMove message (too small)");
                break;
            }

            const auto* moveMsg = reinterpret_cast<const protocol::PlayerMoveMessage*>(
                static_cast<const uint8_t*>(packet->data) + sizeof(protocol::MessageHeader)
            );

            // Update player position
            auto& playerData = players[peer];
            playerData.position = moveMsg->position;

            // Broadcast position update to all other players
            protocol::PlayerPositionUpdateMessage posUpdate;
            posUpdate.playerId = playerData.playerId;
            posUpdate.position = moveMsg->position;
            posUpdate.yaw = moveMsg->yaw;
            posUpdate.pitch = moveMsg->pitch;

            size_t totalSize = sizeof(protocol::MessageHeader) + sizeof(protocol::PlayerPositionUpdateMessage);
            ENetPacket* updatePacket = enet_packet_create(nullptr, totalSize, 0);  // Unreliable for frequent updates

            protocol::MessageHeader updateHeader;
            updateHeader.type = protocol::MessageType::PlayerPositionUpdate;
            updateHeader.payloadSize = sizeof(protocol::PlayerPositionUpdateMessage);
            std::memcpy(updatePacket->data, &updateHeader, sizeof(protocol::MessageHeader));
            std::memcpy(updatePacket->data + sizeof(protocol::MessageHeader), &posUpdate, sizeof(posUpdate));

            // Send to all other players
            for (const auto& [otherPeer, otherPlayer] : players) {
                if (otherPeer != peer) {
                    enet_peer_send(otherPeer, 0, enet_packet_create(updatePacket->data, updatePacket->dataLength, 0));
                }
            }
            enet_packet_destroy(updatePacket);

            // Check distance from last chunk update position
            float distanceFromLastUpdate = glm::distance(playerData.lastChunkUpdatePos, playerData.position);

            // Only send new chunks if player moved significantly (1 chunk = 16 blocks)
            if (distanceFromLastUpdate > 16.0f) {  // 1 chunk
                LOG_DEBUG("Player moved {:.1f} blocks from last chunk update, sending new chunks around ({:.1f}, {:.1f}, {:.1f}) | Currently loaded: {} chunks",
                         distanceFromLastUpdate, playerData.position.x, playerData.position.y, playerData.position.z, playerData.loadedChunks.size());
                sendChunksAroundPlayer(peer, playerData.position);
                playerData.lastChunkUpdatePos = playerData.position;
            }
            break;
        }

        case protocol::MessageType::BlockPlace: {
            LOG_INFO("SERVER: Received BlockPlace message");

            if (packet->dataLength < sizeof(protocol::MessageHeader) + sizeof(protocol::BlockPlaceMessage)) {
                LOG_WARN("SERVER: Invalid BlockPlace message (too small)");
                break;
            }

            const auto* placeMsg = reinterpret_cast<const protocol::BlockPlaceMessage*>(
                static_cast<const uint8_t*>(packet->data) + sizeof(protocol::MessageHeader)
            );

            LOG_INFO("SERVER: Processing block place at ({}, {}, {}) | Type: {}",
                     placeMsg->x, placeMsg->y, placeMsg->z, placeMsg->blockType);

            // Validate player is close enough (10 block reach + 5 block buffer)
            auto& playerData = players[peer];
            float distance = glm::distance(
                playerData.position,
                glm::vec3(placeMsg->x, placeMsg->y, placeMsg->z)
            );
            if (distance > 15.0f) {
                LOG_WARN("Player tried to place block too far away ({:.1f} blocks)", distance);
                break;
            }

            // Get chunk containing this block
            ChunkCoord chunkCoord = ChunkCoord::fromWorldPos(glm::vec3(placeMsg->x, placeMsg->y, placeMsg->z));
            Chunk* chunk = world->getChunk(chunkCoord);
            if (!chunk) {
                LOG_WARN("Player tried to place block in unloaded chunk ({}, {}, {})",
                         chunkCoord.x, chunkCoord.y, chunkCoord.z);
                break;
            }

            // Calculate local block position within chunk
            int localX = placeMsg->x - (chunkCoord.x * 32);
            int localY = placeMsg->y - (chunkCoord.y * 32);
            int localZ = placeMsg->z - (chunkCoord.z * 32);

            // Get current block type
            Block currentBlock = chunk->getBlock(localX, localY, localZ);
            if (currentBlock.type != BlockType::Air) {
                LOG_DEBUG("Player tried to place block in occupied space at ({}, {}, {})",
                         placeMsg->x, placeMsg->y, placeMsg->z);
                break;
            }

            // Place the block
            chunk->setBlock(localX, localY, localZ, Block{static_cast<BlockType>(placeMsg->blockType)});
            LOG_INFO("SERVER: Player placed block at ({}, {}, {}) | Type: {}",
                     placeMsg->x, placeMsg->y, placeMsg->z, placeMsg->blockType);

            // Broadcast block update to all clients
            LOG_INFO("SERVER: Broadcasting BlockUpdate to all clients");
            protocol::BlockUpdateMessage updateMsg;
            updateMsg.x = placeMsg->x;
            updateMsg.y = placeMsg->y;
            updateMsg.z = placeMsg->z;
            updateMsg.blockType = placeMsg->blockType;

            size_t totalSize = sizeof(protocol::MessageHeader) + sizeof(protocol::BlockUpdateMessage);
            ENetPacket* updatePacket = enet_packet_create(nullptr, totalSize, ENET_PACKET_FLAG_RELIABLE);

            protocol::MessageHeader updateHeader;
            updateHeader.type = protocol::MessageType::BlockUpdate;
            updateHeader.payloadSize = sizeof(protocol::BlockUpdateMessage);
            std::memcpy(updatePacket->data, &updateHeader, sizeof(protocol::MessageHeader));
            std::memcpy(updatePacket->data + sizeof(protocol::MessageHeader), &updateMsg, sizeof(updateMsg));

            enet_host_broadcast(server, 0, updatePacket);
            break;
        }

        case protocol::MessageType::BlockBreak: {
            LOG_INFO("SERVER: Received BlockBreak message");

            if (packet->dataLength < sizeof(protocol::MessageHeader) + sizeof(protocol::BlockBreakMessage)) {
                LOG_WARN("SERVER: Invalid BlockBreak message (too small)");
                break;
            }

            const auto* breakMsg = reinterpret_cast<const protocol::BlockBreakMessage*>(
                static_cast<const uint8_t*>(packet->data) + sizeof(protocol::MessageHeader)
            );

            LOG_INFO("SERVER: Processing block break at ({}, {}, {})", breakMsg->x, breakMsg->y, breakMsg->z);

            // Validate player is close enough (10 block reach + 5 block buffer)
            auto& playerData = players[peer];
            float distance = glm::distance(
                playerData.position,
                glm::vec3(breakMsg->x, breakMsg->y, breakMsg->z)
            );
            if (distance > 15.0f) {
                LOG_WARN("Player tried to break block too far away ({:.1f} blocks)", distance);
                break;
            }

            // Get chunk containing this block
            ChunkCoord chunkCoord = ChunkCoord::fromWorldPos(glm::vec3(breakMsg->x, breakMsg->y, breakMsg->z));
            Chunk* chunk = world->getChunk(chunkCoord);
            if (!chunk) {
                LOG_WARN("Player tried to break block in unloaded chunk ({}, {}, {})",
                         chunkCoord.x, chunkCoord.y, chunkCoord.z);
                break;
            }

            // Calculate local block position within chunk
            int localX = breakMsg->x - (chunkCoord.x * 32);
            int localY = breakMsg->y - (chunkCoord.y * 32);
            int localZ = breakMsg->z - (chunkCoord.z * 32);

            // Get current block type
            Block currentBlock = chunk->getBlock(localX, localY, localZ);
            if (currentBlock.type == BlockType::Air) {
                LOG_DEBUG("Player tried to break air block at ({}, {}, {})",
                         breakMsg->x, breakMsg->y, breakMsg->z);
                break;
            }

            // Break the block (set to air)
            chunk->setBlock(localX, localY, localZ, Block{BlockType::Air});
            LOG_INFO("SERVER: Player broke block at ({}, {}, {}) | Type: {}",
                     breakMsg->x, breakMsg->y, breakMsg->z, static_cast<int>(currentBlock.type));

            // Broadcast block update to all clients
            LOG_INFO("SERVER: Broadcasting BlockUpdate to all clients");
            protocol::BlockUpdateMessage updateMsg;
            updateMsg.x = breakMsg->x;
            updateMsg.y = breakMsg->y;
            updateMsg.z = breakMsg->z;
            updateMsg.blockType = static_cast<uint16_t>(BlockType::Air);

            size_t totalSize = sizeof(protocol::MessageHeader) + sizeof(protocol::BlockUpdateMessage);
            ENetPacket* updatePacket = enet_packet_create(nullptr, totalSize, ENET_PACKET_FLAG_RELIABLE);

            protocol::MessageHeader updateHeader;
            updateHeader.type = protocol::MessageType::BlockUpdate;
            updateHeader.payloadSize = sizeof(protocol::BlockUpdateMessage);
            std::memcpy(updatePacket->data, &updateHeader, sizeof(protocol::MessageHeader));
            std::memcpy(updatePacket->data + sizeof(protocol::MessageHeader), &updateMsg, sizeof(updateMsg));

            enet_host_broadcast(server, 0, updatePacket);
            break;
        }

        default:
            LOG_TRACE("Unhandled message type from client: {}", static_cast<int>(header.type));
            break;
    }
}

void GameServer::cleanupNetworking() {
    if (server != nullptr) {
        LOG_INFO("Shutting down server networking...");
        enet_host_destroy(server);
        server = nullptr;
    }
}

void GameServer::sendChunksAroundPlayer(ENetPeer* peer, const glm::vec3& position) {
    // Get chunks needed around this position
    std::vector<ChunkCoord> chunksNeeded = world->getChunksInRadius(position, CHUNK_LOAD_RADIUS);

    // Get player's loaded chunks
    auto& playerData = players[peer];
    std::unordered_set<ChunkCoord> chunksToSend;
    std::unordered_set<ChunkCoord> chunksToUnload;

    // Convert needed chunks to a set for fast lookup
    std::unordered_set<ChunkCoord> neededSet(chunksNeeded.begin(), chunksNeeded.end());

    // Find chunks that need to be sent (not already loaded by player)
    for (const auto& coord : chunksNeeded) {
        if (playerData.loadedChunks.find(coord) == playerData.loadedChunks.end()) {
            chunksToSend.insert(coord);
        }
    }

    // Find chunks that need to be unloaded (loaded but not needed anymore)
    for (const auto& coord : playerData.loadedChunks) {
        if (neededSet.find(coord) == neededSet.end()) {
            chunksToUnload.insert(coord);
        }
    }

    // Send unload messages
    for (const auto& coord : chunksToUnload) {
        protocol::ChunkUnloadMessage msg;
        msg.coord = coord;

        size_t totalSize = sizeof(protocol::MessageHeader) + sizeof(protocol::ChunkUnloadMessage);
        ENetPacket* packet = enet_packet_create(nullptr, totalSize, ENET_PACKET_FLAG_RELIABLE);

        protocol::MessageHeader header;
        header.type = protocol::MessageType::ChunkUnload;
        header.payloadSize = sizeof(protocol::ChunkUnloadMessage);
        std::memcpy(packet->data, &header, sizeof(protocol::MessageHeader));
        std::memcpy(packet->data + sizeof(protocol::MessageHeader), &msg, sizeof(msg));

        enet_peer_send(peer, 0, packet);
        playerData.loadedChunks.erase(coord);
        LOG_DEBUG("Sent unload for chunk ({}, {}, {}) - player at ({:.1f}, {:.1f}, {:.1f})",
                 coord.x, coord.y, coord.z, position.x, position.y, position.z);
    }

    if (!chunksToUnload.empty()) {
        LOG_DEBUG("Unloading {} chunks from player", chunksToUnload.size());
    }

    if (chunksToSend.empty()) {
        return;
    }

    LOG_DEBUG("Sending {} new chunks to player at ({:.1f}, {:.1f}, {:.1f})",
              chunksToSend.size(), position.x, position.y, position.z);

    size_t sentCount = 0;

    for (const auto& coord : chunksToSend) {
        // Load/generate chunk if needed
        Chunk& chunk = world->loadChunk(coord);

        // Serialize chunk
        std::vector<uint8_t> compressedData;
        size_t compressedSize = ChunkSerializer::serialize(chunk, compressedData);

        // Create packet: header + ChunkDataMessage + compressed data
        size_t totalSize = sizeof(protocol::MessageHeader) +
                          sizeof(protocol::ChunkDataMessage) +
                          compressedSize;

        ENetPacket* packet = enet_packet_create(nullptr, totalSize, ENET_PACKET_FLAG_RELIABLE);

        // Write message header
        protocol::MessageHeader header;
        header.type = protocol::MessageType::ChunkData;
        header.payloadSize = sizeof(protocol::ChunkDataMessage) + compressedSize;
        std::memcpy(packet->data, &header, sizeof(protocol::MessageHeader));

        // Write chunk data header
        protocol::ChunkDataMessage chunkHeader;
        chunkHeader.coord = coord;
        chunkHeader.compressedSize = compressedSize;
        std::memcpy(packet->data + sizeof(protocol::MessageHeader),
                   &chunkHeader, sizeof(protocol::ChunkDataMessage));

        // Write compressed chunk data
        std::memcpy(packet->data + sizeof(protocol::MessageHeader) + sizeof(protocol::ChunkDataMessage),
                   compressedData.data(), compressedSize);

        // Send packet
        enet_peer_send(peer, 0, packet);

        // Mark as loaded for this player
        playerData.loadedChunks.insert(coord);
        sentCount++;
    }

    // Flush packets immediately
    enet_host_flush(server);

    LOG_INFO("Sent {} chunks to player", sentCount);
}

void GameServer::updatePlayerChunks() {
    if (players.empty()) {
        // No players, unload all chunks
        size_t unloaded = world->unloadDistantChunks({}, CHUNK_LOAD_RADIUS);
        if (unloaded > 0) {
            LOG_DEBUG("No players online, unloaded all {} chunks", unloaded);
        }
        return;
    }

    // Collect all player positions
    std::vector<glm::vec3> playerPositions;
    playerPositions.reserve(players.size());

    for (const auto& [peer, playerData] : players) {
        playerPositions.push_back(playerData.position);
    }

    // Unload chunks that are far from all players
    world->unloadDistantChunks(playerPositions, CHUNK_LOAD_RADIUS + 2);  // +2 buffer for hysteresis
}

bool GameServer::startTunnel(const std::string& secretKey) {
    if (tunnelRunning) {
        LOG_WARN("Tunnel is already running");
        return false;
    }

    LOG_INFO("========================================");
    LOG_INFO("Starting playit.gg tunnel...");

    std::string key = secretKey;

    // If no secret key provided, try to load from .playit-secret file
    if (key.empty()) {
        std::ifstream secretFile(".playit-secret");
        if (secretFile.is_open()) {
            std::getline(secretFile, key);
            secretFile.close();

            // Trim whitespace
            key.erase(0, key.find_first_not_of(" \t\n\r"));
            key.erase(key.find_last_not_of(" \t\n\r") + 1);

            if (!key.empty()) {
                LOG_INFO("Loaded secret key from .playit-secret");
            }
        }
    }

    if (key.empty()) {
        LOG_ERROR("No secret key provided!");
        LOG_INFO("Please either:");
        LOG_INFO("  1. Create a .playit-secret file with your key");
        LOG_INFO("  2. Use: /tunnel start <your-secret-key>");
        LOG_INFO("Get your secret key at: https://playit.gg/account/agents/new-docker");
        LOG_INFO("========================================");
        return false;
    }

    // Fork process to run playit agent
    tunnelPid = fork();

    if (tunnelPid == -1) {
        LOG_ERROR("Failed to fork process for playit agent");
        LOG_INFO("========================================");
        return false;
    }

    if (tunnelPid == 0) {
        // Child process - run playit agent via Docker

        // Redirect output to log file
        std::freopen("logs/playit.log", "w", stdout);
        std::freopen("logs/playit.log", "w", stderr);

        // Try Docker first, fall back to native binary
        execlp("docker", "docker", "run", "--rm", "--net=host",
               "-e", ("SECRET_KEY=" + key).c_str(),
               "ghcr.io/playit-cloud/playit-agent:latest",
               nullptr);

        // If docker fails, try native playit binary
        execlp("playit", "playit", "--secret", key.c_str(), nullptr);

        // If we get here, both methods failed
        std::cerr << "Failed to start playit agent. Please install either:" << std::endl;
        std::cerr << "  - Docker: https://docs.docker.com/get-docker/" << std::endl;
        std::cerr << "  - playit binary: https://playit.gg/download" << std::endl;
        exit(1);
    }

    // Parent process
    tunnelRunning = true;

    // Give the agent a moment to start
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Check if process is still alive
    int status;
    pid_t result = waitpid(tunnelPid, &status, WNOHANG);

    if (result != 0) {
        // Process already exited
        LOG_ERROR("playit agent failed to start");
        LOG_INFO("Check logs/playit.log for details");
        tunnelRunning = false;
        tunnelPid = -1;
        LOG_INFO("========================================");
        return false;
    }

    LOG_INFO("playit.gg tunnel started successfully!");
    LOG_INFO("Check https://playit.gg/account to see your tunnel address");
    LOG_INFO("Output is being logged to logs/playit.log");
    LOG_INFO("========================================");

    return true;
}

void GameServer::stopTunnel() {
    if (!tunnelRunning || tunnelPid == -1) {
        LOG_INFO("No tunnel is running");
        return;
    }

    LOG_INFO("========================================");
    LOG_INFO("Stopping playit.gg tunnel...");

    // Send SIGTERM to gracefully stop the agent
    kill(tunnelPid, SIGTERM);

    // Wait up to 5 seconds for graceful shutdown
    int status;
    for (int i = 0; i < 50; i++) {
        pid_t result = waitpid(tunnelPid, &status, WNOHANG);
        if (result != 0) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Force kill if still running
    int finalStatus;
    pid_t result = waitpid(tunnelPid, &finalStatus, WNOHANG);
    if (result == 0) {
        LOG_WARN("playit agent didn't stop gracefully, forcing shutdown...");
        kill(tunnelPid, SIGKILL);
        waitpid(tunnelPid, &finalStatus, 0);
    }

    tunnelRunning = false;
    tunnelPid = -1;

    LOG_INFO("playit.gg tunnel stopped");
    LOG_INFO("========================================");
}

} // namespace engine
