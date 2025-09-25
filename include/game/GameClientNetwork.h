/**
 * @file GameClientNetwork.h
 * @brief Network communication system for game client
 * @author Tidal Engine Team
 * @version 1.0
 */

#pragma once

#include "network/NetworkProtocol.h"
#include "core/ThreadPool.h"

#include <memory>
#include <atomic>
#include <thread>

// Forward declarations
class GameClient;
class NetworkClient;
struct ServerHelloMessage;
struct PlayerUpdateMessage;
struct BlockUpdateMessage;
struct ChunkDataMessage;

/**
 * @brief Network communication and message handling system
 *
 * GameClientNetwork manages all network operations for the game client:
 * - Server connection establishment and management
 * - Message queuing and processing for incoming/outgoing packets
 * - Multi-threaded network operations for non-blocking gameplay
 * - Protocol message handling and dispatch
 * - Connection state management and error recovery
 *
 * Features:
 * - Asynchronous message processing with thread-safe queues
 * - Automatic message type dispatch to appropriate handlers
 * - Connection lifecycle management (connect/disconnect/reconnect)
 * - Integration with main game loop through message callbacks
 *
 * Message Flow:
 * - Outgoing: GameClient → GameClientNetwork → NetworkClient → Server
 * - Incoming: Server → NetworkClient → GameClientNetwork → GameClient
 *
 * @see NetworkClient for low-level network operations
 * @see NetworkProtocol for message definitions
 * @see GameClient for main client coordination
 */
class GameClientNetwork {
public:
    /**
     * @brief Construct network system with game client reference
     * @param gameClient Reference to main game client
     */
    GameClientNetwork(GameClient& gameClient);
    /**
     * @brief Destructor - cleanup network resources and stop threads
     */
    ~GameClientNetwork();

    /// @name Network Management
    /// @{
    /**
     * @brief Initialize network system and prepare for connections
     * @return True if initialization successful
     */
    bool initialize();

    /**
     * @brief Shutdown network system and cleanup resources
     */
    void shutdown();
    /// @}

    /// @name Message Handling
    /// @{
    /**
     * @brief Send message packet to server
     * @param packet Network packet to send
     */
    void sendToServer(const NetworkPacket& packet);

    /**
     * @brief Process all queued incoming messages from server
     */
    void processIncomingMessages();

    /**
     * @brief Process individual server message and dispatch to handlers
     * @param packet Network packet to process
     */
    void processServerMessage(const NetworkPacket& packet);
    /// @}

    /// @name Connection Management
    /// @{
    /**
     * @brief Establish connection to game server
     * @param address Server hostname or IP address
     * @param port Server port number
     * @return True if connection initiated successfully
     */
    bool connectToServer(const std::string& address, uint16_t port);

    /**
     * @brief Disconnect from current server
     */
    void disconnect();
    /// @}

    /// @name Connection State
    /// @{
    /**
     * @brief Set network client instance for communication
     * @param client Unique pointer to network client
     */
    void setNetworkClient(std::unique_ptr<NetworkClient> client);

    /**
     * @brief Get current network client instance
     * @return Pointer to network client or nullptr
     */
    NetworkClient* getNetworkClient() const { return m_networkClient.get(); }
    /// @}

    /// @name Threading
    /// @{
    /**
     * @brief Start background network processing thread
     */
    void startNetworkThread();

    /**
     * @brief Stop background network processing thread
     */
    void stopNetworkThread();
    /// @}

private:
    GameClient& m_gameClient;       ///< Reference to main game client

    /// @name Network Communication
    /// @{
    std::unique_ptr<NetworkClient> m_networkClient; ///< Low-level network client
    MessageQueue<NetworkPacket> m_incomingMessages; ///< Thread-safe incoming message queue
    MessageQueue<NetworkPacket> m_outgoingMessages; ///< Thread-safe outgoing message queue
    /// @}

    /// @name Threading
    /// @{
    std::unique_ptr<std::thread> m_networkThread;   ///< Background network processing thread
    std::atomic<bool> m_running{false};             ///< Thread running state flag
    /// @}

    /// @name Message Handlers
    /// @{
    /**
     * @brief Handle server hello/welcome message
     * @param msg Server hello message data
     */
    void handleServerHello(const ServerHelloMessage& msg);

    /**
     * @brief Handle player position/rotation update
     * @param msg Player update message data
     */
    void handlePlayerUpdate(const PlayerUpdateMessage& msg);

    /**
     * @brief Handle block change notification
     * @param msg Block update message data
     */
    void handleBlockUpdate(const BlockUpdateMessage& msg);

    /**
     * @brief Handle chunk data transmission from server
     * @param msg Chunk data message header
     * @param packetPayload Raw packet data containing compressed chunk
     */
    void handleChunkData(const ChunkDataMessage& msg, const std::vector<uint8_t>& packetPayload);
    /// @}

    /// @name Private Implementation
    /// @{
    /**
     * @brief Main network thread function for background processing
     */
    void networkThreadFunc();
    /// @}
};
};