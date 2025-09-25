#include "game/GameClientNetwork.h"
#include "game/GameClient.h"
#include "network/NetworkManager.h"
#include "game/GameServer.h"
#include <iostream>
#include <cstring>

GameClientNetwork::GameClientNetwork(GameClient& gameClient)
    : m_gameClient(gameClient) {
}

GameClientNetwork::~GameClientNetwork() {
    shutdown();
}

bool GameClientNetwork::initialize() {
    // Initialize network client
    m_networkClient = std::make_unique<NetworkClient>();
    m_networkClient->setGameClient(&m_gameClient);
    return true;
}

void GameClientNetwork::shutdown() {
    stopNetworkThread();
    if (m_networkClient) {
        m_networkClient->disconnect();
        m_networkClient.reset();
    }
}

bool GameClientNetwork::connectToServer(const std::string& address, uint16_t port) {
    if (m_gameClient.getState() != ClientState::DISCONNECTED) {
        return false;
    }

    std::cout << "Connecting to server " << address << ":" << port << "..." << std::endl;
    m_gameClient.setState(ClientState::CONNECTING);

    if (!m_networkClient) {
        std::cerr << "Network client not initialized!" << std::endl;
        m_gameClient.setState(ClientState::DISCONNECTED);
        return false;
    }

    // Connect using NetworkClient
    if (m_networkClient->connect(address, port, m_gameClient.getConfig().playerName)) {
        m_gameClient.setState(ClientState::CONNECTED);
        std::cout << "Connected to server!" << std::endl;
        return true;
    } else {
        std::cerr << "Failed to connect to server!" << std::endl;
        m_gameClient.setState(ClientState::DISCONNECTED);
        return false;
    }
}

void GameClientNetwork::disconnect() {
    if (m_networkClient) {
        m_networkClient->disconnect();
    }
    m_gameClient.setState(ClientState::DISCONNECTED);
    std::cout << "Disconnected from server." << std::endl;
}

void GameClientNetwork::sendToServer(const NetworkPacket& packet) {
    if (m_gameClient.getLocalServer()) {
        // Direct call for local server
        m_gameClient.getLocalServer()->processMessage(m_gameClient.getLocalPlayerId(), packet);
    } else {
        // Queue for network transmission
        m_outgoingMessages.push(packet);
    }
}

void GameClientNetwork::processIncomingMessages() {
    NetworkPacket packet;
    while (m_incomingMessages.pop(packet)) {
        processServerMessage(packet);
    }
}

void GameClientNetwork::processServerMessage(const NetworkPacket& packet) {
    switch (packet.header.type) {
        case MessageType::SERVER_HELLO: {
            if (packet.payload.size() >= sizeof(ServerHelloMessage)) {
                ServerHelloMessage msg;
                std::memcpy(&msg, packet.payload.data(), sizeof(msg));
                handleServerHello(msg);
            }
            break;
        }

        case MessageType::PLAYER_UPDATE: {
            if (packet.payload.size() >= sizeof(PlayerUpdateMessage)) {
                PlayerUpdateMessage msg;
                std::memcpy(&msg, packet.payload.data(), sizeof(msg));
                handlePlayerUpdate(msg);
            }
            break;
        }

        case MessageType::BLOCK_UPDATE: {
            if (packet.payload.size() >= sizeof(BlockUpdateMessage)) {
                BlockUpdateMessage msg;
                std::memcpy(&msg, packet.payload.data(), sizeof(msg));
                handleBlockUpdate(msg);
            }
            break;
        }

        case MessageType::CHUNK_DATA: {
            if (packet.payload.size() >= sizeof(ChunkDataMessage)) {
                ChunkDataMessage msg;
                std::memcpy(&msg, packet.payload.data(), sizeof(msg));
                handleChunkData(msg, packet.payload);
            }
            break;
        }

        default:
            std::cout << "Received unknown message type: " << static_cast<int>(packet.header.type) << std::endl;
            break;
    }
}

void GameClientNetwork::handleServerHello(const ServerHelloMessage& msg) {
    if (msg.accepted) {
        m_gameClient.setLocalPlayerId(msg.playerId);
        m_gameClient.getCamera().Position = msg.spawnPosition;
        m_gameClient.getCamera().Pitch = -45.0f; // Look down at terrain
        m_gameClient.getCamera().updateCameraVectors();

        m_gameClient.setState(ClientState::IN_GAME);
        m_gameClient.setGameState(GameState::LOADING);

        std::cout << "Connected to server! Player ID: " << msg.playerId
                  << ", Spawn: (" << msg.spawnPosition.x << ", " << msg.spawnPosition.y << ", " << msg.spawnPosition.z << ")" << std::endl;
    } else {
        std::cout << "Server rejected connection: " << msg.reason << std::endl;
        disconnect();
    }
}

void GameClientNetwork::handlePlayerUpdate(const PlayerUpdateMessage& msg) {
    // Delegate to GameClient for player management
    m_gameClient.handlePlayerUpdate(msg);
}

void GameClientNetwork::handleBlockUpdate(const BlockUpdateMessage& msg) {
    std::cout << "Block update: (" << msg.position.x << ", " << msg.position.y << ", " << msg.position.z
              << ") -> " << static_cast<int>(msg.blockType) << std::endl;

    // Apply block change to local world
    if (m_gameClient.getChunkManager()) {
        m_gameClient.getChunkManager()->updateBlock(msg.position, msg.blockType);
    }
}

void GameClientNetwork::handleChunkData(const ChunkDataMessage& msg, const std::vector<uint8_t>& packetPayload) {
    if (!m_gameClient.getChunkManager()) {
        std::cerr << "No chunk manager available for chunk data!" << std::endl;
        return;
    }

    std::cout << "Received chunk data for (" << msg.chunkPos.x << ", " << msg.chunkPos.y << ", " << msg.chunkPos.z
              << ") - " << (packetPayload.size() - sizeof(ChunkDataMessage)) << " bytes" << std::endl;

    // Extract chunk data from packet payload (skip the message header)
    size_t dataOffset = sizeof(ChunkDataMessage);
    if (packetPayload.size() <= dataOffset) {
        std::cerr << "No chunk data in packet!" << std::endl;
        return;
    }

    std::vector<uint8_t> compressedData(packetPayload.begin() + dataOffset,
                                       packetPayload.end());

    // Delegate to GameClient for chunk loading
    m_gameClient.handleChunkData(msg, compressedData);
}

void GameClientNetwork::setNetworkClient(std::unique_ptr<NetworkClient> client) {
    m_networkClient = std::move(client);
    if (m_networkClient) {
        m_networkClient->setGameClient(&m_gameClient);
    }
}

void GameClientNetwork::startNetworkThread() {
    if (m_running) return;

    m_running = true;
    m_networkThread = std::make_unique<std::thread>(&GameClientNetwork::networkThreadFunc, this);
}

void GameClientNetwork::stopNetworkThread() {
    if (!m_running) return;

    m_running = false;
    if (m_networkThread && m_networkThread->joinable()) {
        m_networkThread->join();
    }
    m_networkThread.reset();
}

void GameClientNetwork::networkThreadFunc() {
    while (m_running) {
        if (m_networkClient && m_networkClient->isConnected()) {
            m_networkClient->processIncomingMessages();
        }

        // Small delay to prevent excessive CPU usage
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}