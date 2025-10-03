#include "server/GameServer.hpp"
#include "server/World.hpp"
#include "shared/Protocol.hpp"
#include "shared/ChunkSerializer.hpp"
#include "core/Logger.hpp"

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
    LOG_INFO("Client connected from {}:{}",
             peer->address.host, peer->address.port);

    // Create player data (spawn at origin, Y=5)
    PlayerData playerData;
    playerData.position = glm::vec3(0.0f, 5.0f, 0.0f);
    players[peer] = playerData;

    // Send chunks in radius around spawn point
    sendChunksAroundPlayer(peer, playerData.position);

    LOG_INFO("Player spawned at ({}, {}, {})",
             playerData.position.x, playerData.position.y, playerData.position.z);
}

void GameServer::onClientDisconnect(ENetPeer* peer) {
    LOG_INFO("Client disconnected from {}:{}",
             peer->address.host, peer->address.port);

    // Remove player from tracking
    players.erase(peer);

    LOG_INFO("Player removed, {} players remaining", players.size());
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

        case protocol::MessageType::PlayerMove:
            // TODO: Update player position, validate movement
            break;

        case protocol::MessageType::BlockPlace:
        case protocol::MessageType::BlockBreak:
            // TODO: Validate and apply block changes
            break;

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

    // Find chunks that need to be sent (not already loaded by player)
    for (const auto& coord : chunksNeeded) {
        if (playerData.loadedChunks.find(coord) == playerData.loadedChunks.end()) {
            chunksToSend.insert(coord);
        }
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
