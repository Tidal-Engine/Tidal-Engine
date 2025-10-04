#pragma once

#include <enet/enet.h>
#include <memory>
#include <atomic>
#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <glm/glm.hpp>
#include "shared/ChunkCoord.hpp"
#include "shared/Item.hpp"

namespace engine {

// Forward declarations
class World;

/**
 * @brief Main game server class
 *
 * Manages the server tick loop, networking, and world state.
 * Runs at a fixed tick rate (40 TPS by default) for deterministic simulation.
 */
class GameServer {
public:
    /**
     * @brief Construct a new game server
     * @param port Port to listen on (default: 25565)
     * @param tickRate Server tick rate in ticks per second (default: 40)
     */
    GameServer(uint16_t port = 25565, double tickRate = 40.0);
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
     * @brief Start playit.gg tunnel
     * @param secretKey The playit.gg secret key (optional, will prompt if not provided)
     * @return true if tunnel started successfully
     */
    bool startTunnel(const std::string& secretKey = "");

    /**
     * @brief Stop playit.gg tunnel
     */
    void stopTunnel();

    /**
     * @brief Check if tunnel is running
     */
    bool isTunnelRunning() const { return tunnelRunning; }

    /**
     * @brief Get current tick number
     */
    uint64_t getCurrentTick() const { return currentTick; }

    /**
     * @brief Get world instance
     */
    World* getWorld() const { return world.get(); }

private:
    // Player tracking
    struct PlayerData {
        uint32_t playerId = 0;                 ///< Unique player ID
        std::string playerName;                ///< Player's display name
        glm::vec3 position{0.0f, 5.0f, 0.0f};  ///< Player world position (spawn at Y=5)
        float yaw = -90.0f;                    ///< Camera yaw angle in degrees
        float pitch = -20.0f;                  ///< Camera pitch angle in degrees
        glm::vec3 lastChunkUpdatePos{0.0f, 5.0f, 0.0f};  ///< Last position where chunks were sent
        std::unordered_set<ChunkCoord> loadedChunks;  ///< Chunks this player has loaded
        std::array<ItemStack, 9> hotbar;       ///< Player hotbar inventory (9 slots)
        size_t selectedHotbarSlot = 0;         ///< Currently selected hotbar slot (0-8)
    };

    std::unordered_map<ENetPeer*, PlayerData> players;  ///< Track all connected players

    static constexpr int32_t CHUNK_LOAD_RADIUS = 10;  ///< Radius to load chunks around player (10 chunks = 160 blocks)

private:
    ENetHost* server = nullptr;
    std::unique_ptr<World> world;

    uint16_t port;
    double tickRate;
    double tickDuration; // Calculated as 1.0 / tickRate

    uint64_t currentTick = 0;
    std::atomic<bool> running{false};

    // Player ID generation
    uint32_t nextPlayerId = 1;  ///< Next player ID to assign

    // Tunnel management
    std::atomic<bool> tunnelRunning{false};
    int tunnelPid = -1;  ///< Process ID of playit agent

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

    /**
     * @brief Send chunks in radius around player
     * @param peer Player to send chunks to
     * @param position Player position
     */
    void sendChunksAroundPlayer(ENetPeer* peer, const glm::vec3& position);

    /**
     * @brief Update chunk loading for all players (called periodically)
     */
    void updatePlayerChunks();

    /**
     * @brief Save player data to disk
     * @param playerData Player data to save
     * @return true if saved successfully
     */
    bool savePlayerData(const PlayerData& playerData);

    /**
     * @brief Load player data from disk
     * @param playerName Name of player to load
     * @param outPlayerData Output player data
     * @return true if loaded successfully
     */
    bool loadPlayerData(const std::string& playerName, PlayerData& outPlayerData);
};

} // namespace engine
