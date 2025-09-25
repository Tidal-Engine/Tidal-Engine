#pragma once

#include "network/NetworkProtocol.h"
#include "game/SaveSystem.h"
#ifndef HEADLESS_SERVER
#include "game/Chunk.h"
#endif
#include "core/ThreadPool.h"
#include <memory>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <string>
#include <functional>

// Forward declarations
class NetworkServer;

// Forward declarations (no graphics dependencies)
class ServerChunkManager;
class ServerChunk;

// Server configuration
struct ServerConfig {
    std::string worldName = "default_world";
    std::string serverName = "Tidal Engine Server";
    std::string motd = "Welcome to Tidal Engine!";
    uint16_t port = 25565;
    uint32_t maxPlayers = 20;
    uint32_t renderDistance = 8;  // Chunks to send to clients
    bool pvpEnabled = true;
    bool creativeModeEnabled = false;
    float autosaveInterval = 30.0f;   // 30 seconds for testing
    std::string savesDirectory = "saves";
    bool enableServerCommands = true;
    bool enableRemoteAccess = true;

    // Future: mod/plugin directories, permission systems, etc.
};

// Hook system for mods/plugins
class ServerHooks {
public:
    // Event hooks
    std::function<bool(const PlayerState&)> onPlayerJoin;
    std::function<void(PlayerId)> onPlayerLeave;
    std::function<bool(PlayerId, const glm::ivec3&, BlockType)> onBlockPlace;
    std::function<bool(PlayerId, const glm::ivec3&)> onBlockBreak;
    std::function<bool(PlayerId, const std::string&)> onChatMessage;

    // Tick hooks
    std::function<void(float)> onServerTick;
    std::function<void()> onWorldSave;
};

class GameServer {
public:
    GameServer(const ServerConfig& config = ServerConfig{});
    ~GameServer();

    // Server lifecycle
    bool initialize();
    void run();
    void shutdown();

    // Server control (for console commands)
    void stop() { m_running = false; }
    bool isRunning() const { return m_running; }
    void saveWorld();
    void kickPlayer(PlayerId playerId, const std::string& reason);
    void broadcast(const std::string& message);

    // Player management
    bool addPlayer(const std::string& name, PlayerId& outPlayerId);
    void removePlayer(PlayerId playerId);
    PlayerState* getPlayer(PlayerId playerId);
    std::vector<PlayerState> getConnectedPlayers() const;
    size_t getPlayerCount() const;

    // World interaction
    bool placeBlock(PlayerId playerId, const glm::ivec3& position, BlockType blockType);
    bool breakBlock(PlayerId playerId, const glm::ivec3& position);
    BlockType getBlock(const glm::ivec3& position) const;

    // Chunk management
    void requestChunk(PlayerId playerId, const ChunkPos& chunkPos);
    bool isChunkLoaded(const ChunkPos& chunkPos) const;

    // Networking
    void sendToPlayer(PlayerId playerId, const NetworkPacket& packet);
    void sendToAll(const NetworkPacket& packet);
    void sendToAllExcept(PlayerId excludePlayerId, const NetworkPacket& packet);

    // Network server management
    bool startNetworkServer(uint16_t port, const std::string& bindAddress = "0.0.0.0");
    void stopNetworkServer();
    bool isNetworkServerRunning() const;

    // Message processing
    void processMessage(PlayerId playerId, const NetworkPacket& packet);

    // Configuration
    const ServerConfig& getConfig() const { return m_config; }
    void setConfig(const ServerConfig& config) { m_config = config; }

    // Hook system for mods/extensibility
    ServerHooks& getHooks() { return m_hooks; }

    // Integrated server support
    void setIntegratedClientCallback(std::function<void(PlayerId, const NetworkPacket&)> callback) {
        m_integratedClientCallback = callback;
    }

    // Console interface
    void processConsoleCommand(const std::string& command);

private:
    ServerConfig m_config;
    ServerHooks m_hooks;

    // Core systems (no graphics dependencies)
    std::unique_ptr<SaveSystem> m_saveSystem;
    std::unique_ptr<ServerChunkManager> m_chunkManager;
    std::unique_ptr<ServerTaskManager> m_taskManager;
    std::unique_ptr<NetworkServer> m_networkServer;

    // Server state
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_initialized{false};
    float m_lastAutosave = 0.0f;
    float m_serverTime = 0.0f;

    // Player management
    std::unordered_map<PlayerId, PlayerState> m_players;
    PlayerId m_nextPlayerId = 1;
    mutable std::mutex m_playerMutex;

    // Event processing (thread-safe)
    MessageQueue<GameEvent> m_eventQueue;

    // Integrated client callback (for singleplayer)
    std::function<void(PlayerId, const NetworkPacket&)> m_integratedClientCallback;

    // Threading
    std::thread m_serverThread;
    static constexpr float TICK_RATE = 20.0f;  // 20 TPS like Minecraft
    static constexpr float TICK_INTERVAL = 1.0f / TICK_RATE;

    // Internal methods
    void serverLoop();
    void processTick(float deltaTime);
    void processEvents();
    void updatePlayers(float deltaTime);
    void checkAutosave(float deltaTime);

    // Message handlers
    void handleClientHello(PlayerId playerId, const ClientHelloMessage& msg);
    void handlePlayerMove(PlayerId playerId, const PlayerMoveMessage& msg);
    void handleBlockPlace(PlayerId playerId, const BlockPlaceMessage& msg);
    void handleBlockBreak(PlayerId playerId, const BlockBreakMessage& msg);
    void handleChatMessage(PlayerId playerId, const ChatMessage& msg);
    void handleChunkRequest(PlayerId playerId, const ChunkRequestMessage& msg);
    void handlePing(PlayerId playerId, const PingMessage& msg);

    // Utility
    void addEvent(const GameEvent& event);
    void broadcastPlayerUpdate(const PlayerState& player);
    void broadcastBlockUpdate(const glm::ivec3& position, BlockType blockType, PlayerId causedBy);

    // Console commands
    void cmdHelp();
    void cmdStop();
    void cmdSave();
    void cmdList();
    void cmdKick(const std::string& playerName, const std::string& reason);
    void cmdBroadcast(const std::string& message);
    void cmdStatus();
};

// Server-only chunk manager (no rendering)
class ServerChunkManager {
public:
    ServerChunkManager(SaveSystem* saveSystem, ServerTaskManager* taskManager = nullptr);
    ~ServerChunkManager() = default;

    // Chunk loading/unloading
    void loadChunk(const ChunkPos& pos);
    void unloadChunk(const ChunkPos& pos);
    bool isChunkLoaded(const ChunkPos& pos) const;

    // Block access
    BlockType getBlock(const glm::ivec3& worldPos) const;
    void setBlock(const glm::ivec3& worldPos, BlockType blockType);

    // Chunk data for network transmission
    bool getChunkData(const ChunkPos& pos, ChunkData& outData) const;

    // Chunk management around players
    void loadChunksAroundPosition(const glm::vec3& position, int radius);
    void unloadDistantChunks(const glm::vec3& position, int unloadDistance);

    // Save/load integration
    void saveAllChunks();

private:
    SaveSystem* m_saveSystem;
    ServerTaskManager* m_taskManager;
    std::unordered_map<ChunkPos, std::unique_ptr<ServerChunk>> m_loadedChunks;
    mutable std::mutex m_chunkMutex;

    ChunkPos worldPositionToChunkPos(const glm::vec3& worldPos) const;
};

// Server-only chunk (no rendering data)
class ServerChunk {
public:
    ServerChunk(const ChunkPos& position);

    BlockType getVoxel(int x, int y, int z) const;
    void setVoxel(int x, int y, int z, BlockType blockType);

    const ChunkPos& getPosition() const { return m_position; }
    bool isDirty() const { return m_dirty; }
    void markClean() { m_dirty = false; }
    void markDirty() { m_dirty = true; }

    // Get data for network transmission
    void getChunkData(ChunkData& outData) const;
    void setChunkData(const ChunkData& data);

    // Generation
    void generateTerrain();

private:
    ChunkPos m_position;
#ifndef HEADLESS_SERVER
    BlockType m_voxels[Chunk::SIZE][Chunk::SIZE][Chunk::SIZE];
#else
    BlockType m_voxels[CHUNK_SIZE][CHUNK_SIZE][CHUNK_SIZE];
#endif
    bool m_dirty = true;
    bool m_generated = false;
};