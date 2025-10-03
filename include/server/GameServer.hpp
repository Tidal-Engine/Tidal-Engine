#pragma once

#include <enet/enet.h>
#include <memory>
#include <atomic>
#include <cstdint>

namespace engine {

// Forward declarations
class World;

/**
 * @brief Main game server class
 *
 * Manages the server tick loop, networking, and world state.
 * Runs at a fixed tick rate (20 TPS by default) for deterministic simulation.
 */
class GameServer {
public:
    /**
     * @brief Construct a new game server
     * @param port Port to listen on (default: 25565)
     * @param tickRate Server tick rate in ticks per second (default: 20)
     */
    GameServer(uint16_t port = 25565, double tickRate = 20.0);
    ~GameServer();

    // Delete copy/move operations (server is unique)
    GameServer(const GameServer&) = delete;
    GameServer& operator=(const GameServer&) = delete;
    GameServer(GameServer&&) = delete;
    GameServer& operator=(GameServer&&) = delete;

    /**
     * @brief Start the server main loop
     *
     * Runs until stop() is called. This is a blocking call.
     */
    void run();

    /**
     * @brief Stop the server
     *
     * Signals the server to shut down gracefully.
     */
    void stop();

    /**
     * @brief Get current tick number
     */
    uint64_t getCurrentTick() const { return currentTick; }

private:
    ENetHost* server = nullptr;
    std::unique_ptr<World> world;

    uint16_t port;
    double tickRate;
    double tickDuration; // Calculated as 1.0 / tickRate

    uint64_t currentTick = 0;
    std::atomic<bool> running{false};

    /**
     * @brief Initialize ENet networking
     */
    void initNetworking();

    /**
     * @brief Process one server tick
     *
     * Updates world state, processes entities, and sends updates to clients.
     */
    void tick();

    /**
     * @brief Process incoming network events
     */
    void processNetworkEvents();

    /**
     * @brief Handle client connection
     */
    void onClientConnect(ENetPeer* peer);

    /**
     * @brief Handle client disconnection
     */
    void onClientDisconnect(ENetPeer* peer);

    /**
     * @brief Handle packet from client
     */
    void onClientPacket(ENetPeer* peer, ENetPacket* packet);

    /**
     * @brief Cleanup networking resources
     */
    void cleanupNetworking();
};

} // namespace engine
