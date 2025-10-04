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
    playerData.position = glm::vec3(0.0f, 5.0f, 0.0f);
    players[peer] = playerData;

    LOG_INFO("========================================");
    LOG_INFO(">>> PLAYER JOINED <<<");
    LOG_INFO("Address: {}:{}", peer->address.host, peer->address.port);
    LOG_INFO("Spawn position: ({:.1f}, {:.1f}, {:.1f})",
             playerData.position.x, playerData.position.y, playerData.position.z);
    LOG_INFO("Players online: {}", players.size());
    LOG_INFO("========================================");

    // Send chunks in radius around spawn point
    sendChunksAroundPlayer(peer, playerData.position);
    players[peer].lastChunkUpdatePos = playerData.position;
}

void GameServer::onClientDisconnect(ENetPeer* peer) {
    // Remove player from tracking
    players.erase(peer);

    LOG_INFO("========================================");
    LOG_INFO("<<< PLAYER LEFT >>>");
    LOG_INFO("Address: {}:{}", peer->address.host, peer->address.port);
    LOG_INFO("Players remaining: {}", players.size());
    LOG_INFO("========================================");
}

void GameServer::onClientPacket(ENetPeer* peer, ENetPacket* packet) {
    if (packet->dataLength < sizeof(protocol::MessageHeader)) {
        LOG_WARN("Received malformed packet from client");
        return;
    }

    // Read message header
    protocol::MessageHeader header;
    std::memcpy(&header, packet->data, sizeof(protocol::MessageHeader));

    LOG_TRACE("Received {} message from client ({} bytes)",
             static_cast<int>(header.type), packet->dataLength);

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

        case protocol::MessageType::BlockPlace:
            // TODO: Implement block placement
            break;

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

} // namespace engine
