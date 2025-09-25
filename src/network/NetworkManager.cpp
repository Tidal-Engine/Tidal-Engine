#include "NetworkManager.h"
#include "GameServer.h"
#ifndef HEADLESS_SERVER
#include "GameClient.h"
#endif
#include <iostream>
#include <algorithm>
#include <cstring>

// Platform-specific socket initialization
#ifdef _WIN32
class WSAInitializer {
public:
    WSAInitializer() {
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        initialized = (result == 0);
    }
    ~WSAInitializer() {
        if (initialized) {
            WSACleanup();
        }
    }
    bool initialized = false;
};

static WSAInitializer g_wsaInit;
#endif

// NetworkConnection implementation
NetworkConnection::NetworkConnection(socket_t socket, const std::string& address)
    : m_socket(socket), m_address(address) {
    setSocketNonBlocking(socket);
    m_receiveBuffer.resize(65536); // 64KB receive buffer
}

NetworkConnection::~NetworkConnection() {
    disconnect();
}

void NetworkConnection::disconnect() {
    if (m_connected.exchange(false)) {
#ifdef _WIN32
        closesocket(m_socket);
#else
        close(m_socket);
#endif
    }
}

bool NetworkConnection::sendPacket(const NetworkPacket& packet) {
    if (!m_connected) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_sendMutex);

    // Serialize packet
    auto data = packet.serialize();

    // Send packet size first (4 bytes)
    uint32_t packetSize = static_cast<uint32_t>(data.size());
    if (!sendData(&packetSize, sizeof(packetSize))) {
        return false;
    }

    // Send packet data
    if (!sendData(data.data(), data.size())) {
        return false;
    }

    m_packetsSent++;
    m_bytesSent += sizeof(packetSize) + data.size();
    return true;
}

bool NetworkConnection::receivePackets(std::queue<NetworkPacket>& outPackets) {
    if (!m_connected) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_receiveMutex);

    // Try to receive data into our buffer
    const size_t maxReceive = m_receiveBuffer.size() - m_receiveBufferOffset;
    if (maxReceive == 0) {
        // Buffer full - this shouldn't happen with proper packet processing
        m_receiveBufferOffset = 0;
        return true;
    }

#ifdef _WIN32
    int bytesReceived = recv(m_socket,
        reinterpret_cast<char*>(m_receiveBuffer.data() + m_receiveBufferOffset),
        static_cast<int>(maxReceive), 0);

    if (bytesReceived == SOCKET_ERROR) {
        int error = WSAGetLastError();
        if (error == WSAEWOULDBLOCK) {
            return true; // No data available, but connection is fine
        }
        disconnect();
        return false;
    }
#else
    ssize_t bytesReceived = recv(m_socket,
        m_receiveBuffer.data() + m_receiveBufferOffset,
        maxReceive, 0);

    if (bytesReceived < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return true; // No data available, but connection is fine
        }
        disconnect();
        return false;
    }
#endif

    if (bytesReceived == 0) {
        // Connection closed by peer
        disconnect();
        return false;
    }

    m_receiveBufferOffset += bytesReceived;
    m_bytesReceived += bytesReceived;

    // Process complete packets
    size_t processedBytes = 0;
    while (processedBytes + sizeof(uint32_t) <= m_receiveBufferOffset) {
        // Read packet size
        uint32_t packetSize;
        std::memcpy(&packetSize, m_receiveBuffer.data() + processedBytes, sizeof(packetSize));

        // Check if we have the complete packet
        if (processedBytes + sizeof(uint32_t) + packetSize > m_receiveBufferOffset) {
            break; // Incomplete packet, wait for more data
        }

        // Deserialize packet
        NetworkPacket packet;
        if (NetworkPacket::deserialize(
            m_receiveBuffer.data() + processedBytes + sizeof(uint32_t),
            packetSize,
            packet)) {
            outPackets.push(packet);
            m_packetsReceived++;
        }

        processedBytes += sizeof(uint32_t) + packetSize;
    }

    // Move remaining data to the beginning of the buffer
    if (processedBytes > 0) {
        if (processedBytes < m_receiveBufferOffset) {
            std::memmove(m_receiveBuffer.data(),
                        m_receiveBuffer.data() + processedBytes,
                        m_receiveBufferOffset - processedBytes);
        }
        m_receiveBufferOffset -= processedBytes;
    }

    return true;
}

bool NetworkConnection::sendData(const void* data, size_t size) {
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    size_t totalSent = 0;

    while (totalSent < size) {
#ifdef _WIN32
        int sent = send(m_socket,
            reinterpret_cast<const char*>(bytes + totalSent),
            static_cast<int>(size - totalSent), 0);

        if (sent == SOCKET_ERROR) {
            int error = WSAGetLastError();
            if (error == WSAEWOULDBLOCK) {
                continue; // Try again
            }
            disconnect();
            return false;
        }
#else
        ssize_t sent = send(m_socket, bytes + totalSent, size - totalSent, MSG_NOSIGNAL);

        if (sent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue; // Try again
            }
            disconnect();
            return false;
        }
#endif

        totalSent += sent;
    }

    return true;
}

void NetworkConnection::setSocketNonBlocking(socket_t socket) {
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(socket, FIONBIO, &mode);
#else
    int flags = fcntl(socket, F_GETFL, 0);
    fcntl(socket, F_SETFL, flags | O_NONBLOCK);
#endif
}

// NetworkServer implementation
NetworkServer::NetworkServer() {
    m_threadPool = std::make_unique<ThreadPool>(4);
}

NetworkServer::~NetworkServer() {
    stop();
}

bool NetworkServer::start(uint16_t port, const std::string& bindAddress) {
    if (m_running) {
        return false;
    }

#ifdef _WIN32
    if (!g_wsaInit.initialized) {
        std::cerr << "Failed to initialize Winsock" << std::endl;
        return false;
    }
#endif

    m_port = port;

    if (!initializeSocket()) {
        return false;
    }

    // Bind socket
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    if (bindAddress == "0.0.0.0") {
        serverAddr.sin_addr.s_addr = INADDR_ANY;
    } else {
        inet_pton(AF_INET, bindAddress.c_str(), &serverAddr.sin_addr);
    }

    if (bind(m_listenSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0) {
        std::cerr << "Failed to bind socket to port " << port << std::endl;
        cleanupSocket();
        return false;
    }

    // Listen for connections
    if (listen(m_listenSocket, 10) < 0) {
        std::cerr << "Failed to listen on socket" << std::endl;
        cleanupSocket();
        return false;
    }

    m_running = true;

    // Start network threads
    m_acceptThread = std::make_unique<std::thread>(&NetworkServer::acceptLoop, this);
    m_networkThread = std::make_unique<std::thread>(&NetworkServer::networkLoop, this);

    std::cout << "Server listening on " << bindAddress << ":" << port << std::endl;
    return true;
}

void NetworkServer::stop() {
    if (!m_running) {
        return;
    }

    m_running = false;

    // Disconnect all clients
    disconnectAll();

    // Close listen socket
    cleanupSocket();

    // Wait for threads to finish
    if (m_acceptThread && m_acceptThread->joinable()) {
        m_acceptThread->join();
    }
    if (m_networkThread && m_networkThread->joinable()) {
        m_networkThread->join();
    }

    m_acceptThread.reset();
    m_networkThread.reset();

    std::cout << "Server stopped" << std::endl;
}

void NetworkServer::acceptLoop() {
    while (m_running) {
        sockaddr_in clientAddr{};
        socklen_t clientAddrLen = sizeof(clientAddr);

        socket_t clientSocket = accept(m_listenSocket,
            reinterpret_cast<sockaddr*>(&clientAddr), &clientAddrLen);

        if (clientSocket == INVALID_SOCKET_VALUE) {
#ifdef _WIN32
            if (WSAGetLastError() == WSAEWOULDBLOCK) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
#else
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
#endif
            if (m_running) {
                std::cerr << "Error accepting connection" << std::endl;
            }
            break;
        }

        // Get client address
        char clientAddrStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, clientAddrStr, INET_ADDRSTRLEN);
        std::string clientAddress = std::string(clientAddrStr) + ":" + std::to_string(ntohs(clientAddr.sin_port));

        handleNewConnection(clientSocket, clientAddress);
    }
}

void NetworkServer::networkLoop() {
    while (m_running) {
        std::vector<PlayerId> playersToProcess;

        // Get list of connected players
        {
            std::lock_guard<std::mutex> lock(m_connectionsMutex);
            for (const auto& [playerId, connection] : m_connections) {
                if (connection->isConnected()) {
                    playersToProcess.push_back(playerId);
                }
            }
        }

        // Process each connection
        for (PlayerId playerId : playersToProcess) {
            m_threadPool->submit_detached([this, playerId]() {
                processConnection(playerId);
            }, TaskPriority::HIGH, "process_connection");
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
    }
}

void NetworkServer::handleNewConnection(socket_t clientSocket, const std::string& clientAddress) {
    std::lock_guard<std::mutex> lock(m_connectionsMutex);

    if (m_connections.size() >= m_maxConnections) {
        std::cout << "Connection from " << clientAddress << " rejected: server full" << std::endl;
#ifdef _WIN32
        closesocket(clientSocket);
#else
        close(clientSocket);
#endif
        return;
    }

    PlayerId playerId = m_nextPlayerId++;
    auto connection = std::make_unique<NetworkConnection>(clientSocket, clientAddress);
    connection->setPlayerId(playerId);

    m_connections[playerId] = std::move(connection);

    std::cout << "New connection from " << clientAddress << " (Player ID: " << playerId << ")" << std::endl;
}

void NetworkServer::processConnection(PlayerId playerId) {
    std::unique_ptr<NetworkConnection> connection;

    // Get connection (with safety check)
    {
        std::lock_guard<std::mutex> lock(m_connectionsMutex);
        auto it = m_connections.find(playerId);
        if (it == m_connections.end()) {
            return;
        }
        connection = std::move(it->second);
        m_connections.erase(it);
    }

    if (!connection || !connection->isConnected()) {
        return;
    }

    // Process incoming messages
    std::queue<NetworkPacket> packets;
    if (connection->receivePackets(packets)) {
        while (!packets.empty()) {
            if (m_gameServer) {
                m_gameServer->processMessage(playerId, packets.front());
            }
            packets.pop();
        }

        // Put connection back
        std::lock_guard<std::mutex> lock(m_connectionsMutex);
        m_connections[playerId] = std::move(connection);
    } else {
        // Connection lost
        std::cout << "Lost connection to player " << playerId << std::endl;
        if (m_gameServer) {
            // Notify server of disconnection
            NetworkPacket disconnectPacket(MessageType::PLAYER_LEAVE, nullptr, 0);
            m_gameServer->processMessage(playerId, disconnectPacket);
        }
    }
}

void NetworkServer::sendToPlayer(PlayerId playerId, const NetworkPacket& packet) {
    std::lock_guard<std::mutex> lock(m_connectionsMutex);
    auto it = m_connections.find(playerId);
    if (it != m_connections.end() && it->second->isConnected()) {
        it->second->sendPacket(packet);
    }
}

void NetworkServer::sendToAll(const NetworkPacket& packet) {
    std::lock_guard<std::mutex> lock(m_connectionsMutex);
    for (const auto& [playerId, connection] : m_connections) {
        if (connection->isConnected()) {
            connection->sendPacket(packet);
        }
    }
}

void NetworkServer::sendToAllExcept(PlayerId excludePlayerId, const NetworkPacket& packet) {
    std::lock_guard<std::mutex> lock(m_connectionsMutex);
    for (const auto& [playerId, connection] : m_connections) {
        if (playerId != excludePlayerId && connection->isConnected()) {
            connection->sendPacket(packet);
        }
    }
}

void NetworkServer::disconnectPlayer(PlayerId playerId) {
    std::lock_guard<std::mutex> lock(m_connectionsMutex);
    auto it = m_connections.find(playerId);
    if (it != m_connections.end()) {
        it->second->disconnect();
        m_connections.erase(it);
    }
}

void NetworkServer::disconnectAll() {
    std::lock_guard<std::mutex> lock(m_connectionsMutex);
    for (auto& [playerId, connection] : m_connections) {
        connection->disconnect();
    }
    m_connections.clear();
}

size_t NetworkServer::getConnectionCount() const {
    std::lock_guard<std::mutex> lock(m_connectionsMutex);
    return m_connections.size();
}

bool NetworkServer::initializeSocket() {
    m_listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_listenSocket == INVALID_SOCKET_VALUE) {
        std::cerr << "Failed to create socket" << std::endl;
        return false;
    }

    // Set socket options
    int optval = 1;
    setsockopt(m_listenSocket, SOL_SOCKET, SO_REUSEADDR,
#ifdef _WIN32
        reinterpret_cast<const char*>(&optval),
#else
        &optval,
#endif
        sizeof(optval));

    // Set non-blocking for accept loop
    NetworkConnection::setSocketNonBlocking(m_listenSocket);

    return true;
}

void NetworkServer::cleanupSocket() {
    if (m_listenSocket != INVALID_SOCKET_VALUE) {
#ifdef _WIN32
        closesocket(m_listenSocket);
#else
        close(m_listenSocket);
#endif
        m_listenSocket = INVALID_SOCKET_VALUE;
    }
}

// NetworkClient implementation
NetworkClient::NetworkClient() {
#ifdef _WIN32
    m_wsaInitialized = g_wsaInit.initialized;
#endif
}

NetworkClient::~NetworkClient() {
    disconnect();
}

bool NetworkClient::connect(const std::string& serverAddress, uint16_t port, const std::string& playerName) {
    if (m_connected) {
        return false;
    }

#ifdef _WIN32
    if (!m_wsaInitialized) {
        std::cerr << "Winsock not initialized" << std::endl;
        return false;
    }
#endif

    m_playerName = playerName;
    m_serverAddress = serverAddress + ":" + std::to_string(port);

    // Create socket
    socket_t clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET_VALUE) {
        std::cerr << "Failed to create client socket" << std::endl;
        return false;
    }

    // Connect to server
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    if (inet_pton(AF_INET, serverAddress.c_str(), &serverAddr.sin_addr) <= 0) {
        std::cerr << "Invalid server address: " << serverAddress << std::endl;
#ifdef _WIN32
        closesocket(clientSocket);
#else
        close(clientSocket);
#endif
        return false;
    }

    if (::connect(clientSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0) {
        std::cerr << "Failed to connect to server " << m_serverAddress << std::endl;
#ifdef _WIN32
        closesocket(clientSocket);
#else
        close(clientSocket);
#endif
        return false;
    }

    m_connection = std::make_unique<NetworkConnection>(clientSocket, m_serverAddress);
    m_connected = true;

    // Perform handshake
    if (!performHandshake()) {
        disconnect();
        return false;
    }

    // Start network thread
    m_networkThread = std::make_unique<std::thread>(&NetworkClient::networkLoop, this);

    std::cout << "Connected to server " << m_serverAddress << std::endl;
    return true;
}

void NetworkClient::disconnect() {
    if (!m_connected) {
        return;
    }

    m_connected = false;

    if (m_connection) {
        // Send disconnect message
        DisconnectMessage disconnectMsg;
        strncpy(disconnectMsg.reason, "Client disconnecting", sizeof(disconnectMsg.reason) - 1);
        NetworkPacket packet(MessageType::DISCONNECT, &disconnectMsg, sizeof(disconnectMsg));
        m_connection->sendPacket(packet);

        m_connection->disconnect();
        m_connection.reset();
    }

    if (m_networkThread && m_networkThread->joinable()) {
        m_networkThread->join();
    }
    m_networkThread.reset();

    std::cout << "Disconnected from server" << std::endl;
}

bool NetworkClient::sendPacket(const NetworkPacket& packet) {
    if (!m_connected || !m_connection) {
        return false;
    }
    return m_connection->sendPacket(packet);
}

void NetworkClient::processIncomingMessages() {
    if (!m_connected || !m_connection) {
        return;
    }

    std::queue<NetworkPacket> packets;
    if (m_connection->receivePackets(packets)) {
        while (!packets.empty()) {
            handleMessage(packets.front());
            packets.pop();
        }
    } else {
        // Connection lost
        m_connected = false;
        std::cout << "Lost connection to server" << std::endl;
    }
}

void NetworkClient::networkLoop() {
    while (m_connected) {
        processIncomingMessages();
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
    }
}

void NetworkClient::handleMessage(const NetworkPacket& packet) {
#ifndef HEADLESS_SERVER
    if (m_gameClient) {
        // Forward message to game client for processing
        m_gameClient->handleNetworkMessage(packet);
    }
#endif
}

bool NetworkClient::performHandshake() {
    // Send CLIENT_HELLO
    ClientHelloMessage helloMsg{};
    helloMsg.protocolVersion = NETWORK_PROTOCOL_VERSION;
    strncpy(helloMsg.playerName, m_playerName.c_str(), sizeof(helloMsg.playerName) - 1);

    NetworkPacket helloPacket(MessageType::CLIENT_HELLO, &helloMsg, sizeof(helloMsg));
    if (!m_connection->sendPacket(helloPacket)) {
        std::cerr << "Failed to send hello message" << std::endl;
        return false;
    }

    // Wait for SERVER_HELLO response (with timeout)
    auto startTime = std::chrono::steady_clock::now();
    const auto timeout = std::chrono::seconds(10);

    while (std::chrono::steady_clock::now() - startTime < timeout) {
        std::queue<NetworkPacket> packets;
        if (m_connection->receivePackets(packets)) {
            while (!packets.empty()) {
                const auto& packet = packets.front();
                if (packet.header.type == MessageType::SERVER_HELLO) {
                    if (packet.payload.size() == sizeof(ServerHelloMessage)) {
                        ServerHelloMessage serverHello;
                        std::memcpy(&serverHello, packet.payload.data(), sizeof(serverHello));

                        if (serverHello.accepted) {
                            m_playerId = serverHello.playerId;
                            std::cout << "Handshake successful! Player ID: " << m_playerId << std::endl;
                            return true;
                        } else {
                            std::cerr << "Connection rejected: " << serverHello.reason << std::endl;
                            return false;
                        }
                    }
                }
                packets.pop();
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cerr << "Handshake timeout" << std::endl;
    return false;
}