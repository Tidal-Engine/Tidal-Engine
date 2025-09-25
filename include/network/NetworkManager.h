/**
 * @file NetworkManager.h
 * @brief Cross-platform network management for client-server multiplayer gaming
 * @author Tidal Engine Team
 * @version 1.0
 */

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
    using socket_t = SOCKET;                        ///< Windows socket type
    constexpr socket_t INVALID_SOCKET_VALUE = INVALID_SOCKET;  ///< Invalid socket value on Windows
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    using socket_t = int;                           ///< Unix socket type
    constexpr socket_t INVALID_SOCKET_VALUE = -1;   ///< Invalid socket value on Unix
#endif

// Forward declarations
class GameServer;
class GameClient;

/**
 * @brief Network connection representing a client-server TCP connection
 *
 * Manages a single TCP socket connection between client and server, providing
 * thread-safe packet transmission with automatic buffering and statistics tracking.
 * Handles cross-platform socket operations and non-blocking I/O.
 *
 * @note All send/receive operations are thread-safe using internal mutexes
 * @warning Socket must be properly closed to avoid resource leaks
 */
class NetworkConnection {
public:
    /**
     * @brief Construct connection from existing socket
     * @param socket Connected TCP socket
     * @param address Remote endpoint address string
     */
    NetworkConnection(socket_t socket, const std::string& address);

    /**
     * @brief Destructor - automatically closes socket and cleans up resources
     */
    ~NetworkConnection();

    /// @name Connection Information
    /// @{
    socket_t getSocket() const { return m_socket; }                ///< Get underlying socket handle
    const std::string& getAddress() const { return m_address; }    ///< Get remote IP address
    PlayerId getPlayerId() const { return m_playerId; }            ///< Get assigned player ID
    void setPlayerId(PlayerId id) { m_playerId = id; }             ///< Set player ID for this connection
    /// @}

    /// @name Connection State
    /// @{
    /**
     * @brief Check if connection is active
     * @return True if socket is connected and operational
     */
    bool isConnected() const { return m_connected; }

    /**
     * @brief Gracefully close the connection
     * @note Thread-safe, can be called multiple times
     */
    void disconnect();
    /// @}

    /// @name Message Handling
    /// @{
    /**
     * @brief Send a network packet to remote endpoint
     * @param packet Packet to transmit
     * @return True if packet was queued for sending successfully
     * @note Thread-safe, uses internal buffering for partial sends
     */
    bool sendPacket(const NetworkPacket& packet);

    /**
     * @brief Receive available packets from connection
     * @param outPackets Queue to store received complete packets
     * @return True if any packets were received
     * @note Non-blocking operation, handles partial packet reception
     */
    bool receivePackets(std::queue<NetworkPacket>& outPackets);
    /// @}

    /// @name Connection Statistics
    /// @{
    uint64_t getBytesSent() const { return m_bytesSent; }          ///< Total bytes sent over connection
    uint64_t getBytesReceived() const { return m_bytesReceived; }  ///< Total bytes received from connection
    uint32_t getPacketsSent() const { return m_packetsSent; }      ///< Total packets sent successfully
    uint32_t getPacketsReceived() const { return m_packetsReceived; } ///< Total complete packets received
    /// @}

    /// @name Socket Utilities
    /// @{
    /**
     * @brief Configure socket for non-blocking I/O operations
     * @param socket Socket handle to configure
     * @note Cross-platform implementation (Windows/Unix)
     */
    static void setSocketNonBlocking(socket_t socket);
    /// @}

private:
    socket_t m_socket;                              ///< Underlying TCP socket handle
    std::string m_address;                          ///< Remote endpoint IP address
    PlayerId m_playerId = INVALID_PLAYER_ID;       ///< Assigned player identifier
    std::atomic<bool> m_connected{true};            ///< Connection status flag

    /// @name Internal Buffers
    /// @{
    std::vector<uint8_t> m_sendBuffer;              ///< Buffered data waiting to be sent
    std::vector<uint8_t> m_receiveBuffer;           ///< Buffer for incoming partial packets
    size_t m_receiveBufferOffset = 0;               ///< Current position in receive buffer
    /// @}

    /// @name Connection Statistics (Thread-safe)
    /// @{
    std::atomic<uint64_t> m_bytesSent{0};           ///< Total bytes transmitted
    std::atomic<uint64_t> m_bytesReceived{0};       ///< Total bytes received
    std::atomic<uint32_t> m_packetsSent{0};         ///< Count of packets sent
    std::atomic<uint32_t> m_packetsReceived{0};     ///< Count of packets received
    /// @}

    /// @name Thread Synchronization
    /// @{
    mutable std::mutex m_sendMutex;                 ///< Protects send operations
    mutable std::mutex m_receiveMutex;              ///< Protects receive operations
    /// @}

    /// @name Internal Socket Operations
    /// @{
    /**
     * @brief Send raw data through socket with buffering
     * @param data Pointer to data buffer
     * @param size Number of bytes to send
     * @return True if all data was sent or buffered
     */
    bool sendData(const void* data, size_t size);

    /**
     * @brief Receive raw data from socket
     * @param data Buffer to store received data
     * @param size Number of bytes to receive
     * @return True if requested data was fully received
     */
    bool receiveData(void* data, size_t size);
    /// @}
};

/**
 * @brief Server-side network manager for multiplayer game hosting
 *
 * Manages TCP server socket, accepts client connections, and handles
 * message routing between connected clients. Uses thread pool for scalable
 * connection handling and provides comprehensive connection management.
 *
 * Features:
 * - Multi-threaded client connection handling
 * - Automatic player ID assignment
 * - Broadcast and selective messaging
 * - Connection limits and monitoring
 * - Cross-platform socket implementation
 *
 * @note Thread-safe for all public operations
 * @see NetworkClient for client-side networking
 * @see GameServer for game logic integration
 */
class NetworkServer {
public:
    /**
     * @brief Construct network server with default settings
     * @note Does not start listening - call start() explicitly
     */
    NetworkServer();

    /**
     * @brief Destructor - stops server and cleans up all connections
     */
    ~NetworkServer();

    /// @name Server Lifecycle
    /// @{
    /**
     * @brief Start server on specified port and address
     * @param port TCP port to bind to
     * @param bindAddress IP address to bind to ("0.0.0.0" for all interfaces)
     * @return True if server started successfully
     * @note Creates accept and network processing threads
     */
    bool start(uint16_t port, const std::string& bindAddress = "0.0.0.0");

    /**
     * @brief Stop server and disconnect all clients
     * @note Blocks until all threads are safely terminated
     */
    void stop();

    /**
     * @brief Check if server is currently running
     * @return True if server is accepting connections
     */
    bool isRunning() const { return m_running; }
    /// @}

    /// @name Connection Management
    /// @{
    /**
     * @brief Set game server instance for message routing
     * @param gameServer Pointer to game server instance
     * @note Must be called before processing any game messages
     */
    void setGameServer(GameServer* gameServer) { m_gameServer = gameServer; }

    /**
     * @brief Get current number of connected clients
     * @return Count of active player connections
     * @note Thread-safe operation
     */
    size_t getConnectionCount() const;

    /**
     * @brief Disconnect specific player by ID
     * @param playerId ID of player to disconnect
     * @note Thread-safe, handles cleanup automatically
     */
    void disconnectPlayer(PlayerId playerId);

    /**
     * @brief Disconnect all connected players
     * @note Useful for server shutdown or maintenance
     */
    void disconnectAll();
    /// @}

    /// @name Message Broadcasting
    /// @{
    /**
     * @brief Send packet to specific player
     * @param playerId Target player ID
     * @param packet Packet to send
     * @note No-op if player is not connected
     */
    void sendToPlayer(PlayerId playerId, const NetworkPacket& packet);

    /**
     * @brief Broadcast packet to all connected players
     * @param packet Packet to broadcast
     * @note Efficient bulk sending using thread pool
     */
    void sendToAll(const NetworkPacket& packet);

    /**
     * @brief Send packet to all players except one
     * @param excludePlayerId Player ID to exclude from broadcast
     * @param packet Packet to send
     * @note Commonly used for position updates (don't echo to sender)
     */
    void sendToAllExcept(PlayerId excludePlayerId, const NetworkPacket& packet);
    /// @}

    /// @name Server Configuration
    /// @{
    /**
     * @brief Set maximum number of concurrent connections
     * @param maxConnections Maximum client limit
     * @note Takes effect for new connections only
     */
    void setMaxConnections(size_t maxConnections) { m_maxConnections = maxConnections; }

    /**
     * @brief Get current connection limit
     * @return Maximum allowed concurrent connections
     */
    size_t getMaxConnections() const { return m_maxConnections; }
    /// @}

private:
    GameServer* m_gameServer = nullptr;             ///< Associated game server instance
    socket_t m_listenSocket = INVALID_SOCKET_VALUE; ///< Server listening socket
    std::atomic<bool> m_running{false};             ///< Server running state flag
    uint16_t m_port = 0;                            ///< Port server is bound to
    size_t m_maxConnections = 100;                  ///< Maximum concurrent connections

    /// @name Connection Management
    /// @{
    std::unordered_map<PlayerId, std::unique_ptr<NetworkConnection>> m_connections; ///< Active client connections
    mutable std::mutex m_connectionsMutex;         ///< Protects connections map
    PlayerId m_nextPlayerId = 1;                   ///< Next player ID to assign
    /// @}

    /// @name Threading Infrastructure
    /// @{
    std::unique_ptr<std::thread> m_acceptThread;   ///< Thread for accepting new connections
    std::unique_ptr<std::thread> m_networkThread;  ///< Thread for processing network I/O
    std::unique_ptr<ThreadPool> m_threadPool;      ///< Pool for connection-specific tasks
    /// @}

    /// @name Network Processing
    /// @{
    /**
     * @brief Main loop for accepting new client connections
     * @note Runs in dedicated thread until server stops
     */
    void acceptLoop();

    /**
     * @brief Main loop for processing network I/O from all connections
     * @note Handles message reception and routing to game server
     */
    void networkLoop();

    /**
     * @brief Handle new client connection setup
     * @param clientSocket Accepted client socket
     * @param clientAddress Client IP address string
     */
    void handleNewConnection(socket_t clientSocket, const std::string& clientAddress);

    /**
     * @brief Process messages from specific connection
     * @param playerId Player ID to process
     * @note Called from network thread for each active connection
     */
    void processConnection(PlayerId playerId);

    /**
     * @brief Clean up and remove disconnected player
     * @param playerId Player to remove
     * @note Thread-safe, notifies game server of disconnection
     */
    void removeConnection(PlayerId playerId);
    /// @}

    /// @name Socket Management
    /// @{
    /**
     * @brief Initialize listening socket and bind to port
     * @return True if socket was created and bound successfully
     */
    bool initializeSocket();

    /**
     * @brief Clean up server socket and WSA on Windows
     */
    void cleanupSocket();

    /**
     * @brief Get string representation of socket address
     * @param socket Socket to query
     * @return IP address string or empty if error
     */
    std::string getSocketAddress(socket_t socket);
    /// @}

#ifdef _WIN32
    bool m_wsaInitialized = false;                  ///< Windows Sockets API initialization flag
#endif
};

/**
 * @brief Client-side network manager for connecting to game servers
 *
 * Handles TCP connection to game server, manages authentication handshake,
 * and provides reliable message transmission. Integrates with GameClient
 * for seamless multiplayer gameplay.
 *
 * Features:
 * - Automatic connection management with reconnection support
 * - Threaded message processing
 * - Cross-platform socket implementation
 * - Integration with GameClient message routing
 *
 * @note All public methods are thread-safe
 * @see NetworkServer for server-side networking
 * @see GameClient for client game logic integration
 */
class NetworkClient {
public:
    /**
     * @brief Construct network client with default settings
     * @note Does not connect automatically - call connect() explicitly
     */
    NetworkClient();

    /**
     * @brief Destructor - disconnects from server and cleans up resources
     */
    ~NetworkClient();

    /// @name Connection Lifecycle
    /// @{
    /**
     * @brief Connect to game server and perform handshake
     * @param serverAddress Server IP address or hostname
     * @param port Server TCP port
     * @param playerName Desired player name for authentication
     * @return True if connection and handshake succeeded
     * @note Blocks until connection completes or times out
     */
    bool connect(const std::string& serverAddress, uint16_t port, const std::string& playerName);

    /**
     * @brief Disconnect from server gracefully
     * @note Thread-safe, stops network processing thread
     */
    void disconnect();

    /**
     * @brief Check if currently connected to server
     * @return True if connection is active
     */
    bool isConnected() const { return m_connected; }
    /// @}

    /// @name Message Handling
    /// @{
    /**
     * @brief Set game client instance for message routing
     * @param gameClient Pointer to game client instance
     * @note Required for processing incoming server messages
     */
    void setGameClient(GameClient* gameClient) { m_gameClient = gameClient; }

    /**
     * @brief Send packet to server
     * @param packet Packet to transmit
     * @return True if packet was sent successfully
     * @note Thread-safe, queues packet if needed
     */
    bool sendPacket(const NetworkPacket& packet);

    /**
     * @brief Process all pending incoming messages
     * @note Should be called regularly from main game thread
     */
    void processIncomingMessages();
    /// @}

    /// @name Connection Information
    /// @{
    /**
     * @brief Get assigned player ID from server
     * @return Player ID received during handshake
     * @note Returns INVALID_PLAYER_ID if not connected
     */
    PlayerId getPlayerId() const { return m_playerId; }

    /**
     * @brief Get server address currently connected to
     * @return Server IP address or hostname
     */
    const std::string& getServerAddress() const { return m_serverAddress; }
    /// @}

private:
    GameClient* m_gameClient = nullptr;             ///< Associated game client instance
    std::unique_ptr<NetworkConnection> m_connection; ///< Connection to server
    std::atomic<bool> m_connected{false};           ///< Connection status flag
    PlayerId m_playerId = INVALID_PLAYER_ID;       ///< Player ID assigned by server
    std::string m_serverAddress;                    ///< Server address string
    std::string m_playerName;                       ///< Player name for authentication

    /// @name Threading
    /// @{
    std::unique_ptr<std::thread> m_networkThread;   ///< Thread for network message processing
    /// @}

    /// @name Message Processing
    /// @{
    /**
     * @brief Main network processing loop
     * @note Runs in dedicated thread, handles incoming messages
     */
    void networkLoop();

    /**
     * @brief Process individual message from server
     * @param packet Received packet to handle
     * @note Routes messages to appropriate game client handlers
     */
    void handleMessage(const NetworkPacket& packet);

    /**
     * @brief Perform initial handshake with server
     * @return True if handshake completed successfully
     * @note Exchanges protocol version and player authentication
     */
    bool performHandshake();
    /// @}

#ifdef _WIN32
    bool m_wsaInitialized = false;                  ///< Windows Sockets API initialization flag
#endif
};