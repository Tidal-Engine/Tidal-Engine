#pragma once

#include "network/NetworkProtocol.h"
#include "core/ThreadPool.h"
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include <queue>
#include <unordered_map>
#include <functional>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    using socket_t = SOCKET;
    constexpr socket_t INVALID_SOCKET_VALUE = INVALID_SOCKET;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    using socket_t = int;
    constexpr socket_t INVALID_SOCKET_VALUE = -1;
#endif

// Forward declarations
class GameServer;
class GameClient;

// Network connection representing a client connection to the server
class NetworkConnection {
public:
    NetworkConnection(socket_t socket, const std::string& address);
    ~NetworkConnection();

    // Connection info
    socket_t getSocket() const { return m_socket; }
    const std::string& getAddress() const { return m_address; }
    PlayerId getPlayerId() const { return m_playerId; }
    void setPlayerId(PlayerId id) { m_playerId = id; }

    // Connection state
    bool isConnected() const { return m_connected; }
    void disconnect();

    // Message handling
    bool sendPacket(const NetworkPacket& packet);
    bool receivePackets(std::queue<NetworkPacket>& outPackets);

    // Statistics
    uint64_t getBytesSent() const { return m_bytesSent; }
    uint64_t getBytesReceived() const { return m_bytesReceived; }
    uint32_t getPacketsSent() const { return m_packetsSent; }
    uint32_t getPacketsReceived() const { return m_packetsReceived; }

    // Static utility methods
    static void setSocketNonBlocking(socket_t socket);

private:
    socket_t m_socket;
    std::string m_address;
    PlayerId m_playerId = INVALID_PLAYER_ID;
    std::atomic<bool> m_connected{true};

    // Send/receive buffers
    std::vector<uint8_t> m_sendBuffer;
    std::vector<uint8_t> m_receiveBuffer;
    size_t m_receiveBufferOffset = 0;

    // Statistics
    std::atomic<uint64_t> m_bytesSent{0};
    std::atomic<uint64_t> m_bytesReceived{0};
    std::atomic<uint32_t> m_packetsSent{0};
    std::atomic<uint32_t> m_packetsReceived{0};

    mutable std::mutex m_sendMutex;
    mutable std::mutex m_receiveMutex;

    // Helper methods
    bool sendData(const void* data, size_t size);
    bool receiveData(void* data, size_t size);
};

// Server-side network manager
class NetworkServer {
public:
    NetworkServer();
    ~NetworkServer();

    // Server lifecycle
    bool start(uint16_t port, const std::string& bindAddress = "0.0.0.0");
    void stop();
    bool isRunning() const { return m_running; }

    // Connection management
    void setGameServer(GameServer* gameServer) { m_gameServer = gameServer; }
    size_t getConnectionCount() const;
    void disconnectPlayer(PlayerId playerId);
    void disconnectAll();

    // Message handling
    void sendToPlayer(PlayerId playerId, const NetworkPacket& packet);
    void sendToAll(const NetworkPacket& packet);
    void sendToAllExcept(PlayerId excludePlayerId, const NetworkPacket& packet);

    // Configuration
    void setMaxConnections(size_t maxConnections) { m_maxConnections = maxConnections; }
    size_t getMaxConnections() const { return m_maxConnections; }

private:
    GameServer* m_gameServer = nullptr;
    socket_t m_listenSocket = INVALID_SOCKET_VALUE;
    std::atomic<bool> m_running{false};
    uint16_t m_port = 0;
    size_t m_maxConnections = 100;

    // Connection management
    std::unordered_map<PlayerId, std::unique_ptr<NetworkConnection>> m_connections;
    mutable std::mutex m_connectionsMutex;
    PlayerId m_nextPlayerId = 1;

    // Threading
    std::unique_ptr<std::thread> m_acceptThread;
    std::unique_ptr<std::thread> m_networkThread;
    std::unique_ptr<ThreadPool> m_threadPool;

    // Network processing
    void acceptLoop();
    void networkLoop();
    void handleNewConnection(socket_t clientSocket, const std::string& clientAddress);
    void processConnection(PlayerId playerId);
    void removeConnection(PlayerId playerId);

    // Socket utilities
    bool initializeSocket();
    void cleanupSocket();
    std::string getSocketAddress(socket_t socket);

#ifdef _WIN32
    bool m_wsaInitialized = false;
#endif
};

// Client-side network manager
class NetworkClient {
public:
    NetworkClient();
    ~NetworkClient();

    // Connection lifecycle
    bool connect(const std::string& serverAddress, uint16_t port, const std::string& playerName);
    void disconnect();
    bool isConnected() const { return m_connected; }

    // Message handling
    void setGameClient(GameClient* gameClient) { m_gameClient = gameClient; }
    bool sendPacket(const NetworkPacket& packet);
    void processIncomingMessages();

    // Connection info
    PlayerId getPlayerId() const { return m_playerId; }
    const std::string& getServerAddress() const { return m_serverAddress; }

private:
    GameClient* m_gameClient = nullptr;
    std::unique_ptr<NetworkConnection> m_connection;
    std::atomic<bool> m_connected{false};
    PlayerId m_playerId = INVALID_PLAYER_ID;
    std::string m_serverAddress;
    std::string m_playerName;

    // Threading
    std::unique_ptr<std::thread> m_networkThread;

    // Message processing
    void networkLoop();
    void handleMessage(const NetworkPacket& packet);
    bool performHandshake();

#ifdef _WIN32
    bool m_wsaInitialized = false;
#endif
};