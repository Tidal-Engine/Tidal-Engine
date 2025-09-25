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

class GameClientNetwork {
public:
    GameClientNetwork(GameClient& gameClient);
    ~GameClientNetwork();

    // Network management
    bool initialize();
    void shutdown();

    // Message handling
    void sendToServer(const NetworkPacket& packet);
    void processIncomingMessages();
    void processServerMessage(const NetworkPacket& packet);

    // Connection management
    bool connectToServer(const std::string& address, uint16_t port);
    void disconnect();

    // Connection state
    void setNetworkClient(std::unique_ptr<NetworkClient> client);
    NetworkClient* getNetworkClient() const { return m_networkClient.get(); }

    // Threading
    void startNetworkThread();
    void stopNetworkThread();

private:
    GameClient& m_gameClient;

    // Networking
    std::unique_ptr<NetworkClient> m_networkClient;
    MessageQueue<NetworkPacket> m_incomingMessages;
    MessageQueue<NetworkPacket> m_outgoingMessages;

    // Threading
    std::unique_ptr<std::thread> m_networkThread;
    std::atomic<bool> m_running{false};

    // Message handlers
    void handleServerHello(const ServerHelloMessage& msg);
    void handlePlayerUpdate(const PlayerUpdateMessage& msg);
    void handleBlockUpdate(const BlockUpdateMessage& msg);
    void handleChunkData(const ChunkDataMessage& msg, const std::vector<uint8_t>& packetPayload);

    // Threading
    void networkThreadFunc();
};