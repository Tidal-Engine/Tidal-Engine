#include "game/GameClient.h"
#include "game/GameServer.h"
#include <iostream>
#include <thread>
#include <memory>

void runMainMenu();
void runDedicatedServer();
void runMultiplayerClient(const std::string& serverAddress);
void runSingleplayer();

void printUsage() {
    std::cout << "Tidal Engine - Client-Server Architecture Demo\n" << std::endl;
    std::cout << "Usage:" << std::endl;
    std::cout << "  TidalEngine                    - Run main menu" << std::endl;
    std::cout << "  TidalEngine --client <host>    - Run multiplayer client" << std::endl;
    std::cout << "  TidalEngine --server           - Run dedicated server" << std::endl;
    std::cout << "  TidalEngine --help             - Show this help" << std::endl;
}

void runDedicatedServer() {
    std::cout << "Starting Tidal Engine Dedicated Server..." << std::endl;

    ServerConfig config;
    config.worldName = "server_world";
    config.serverName = "Tidal Engine Dedicated Server";
    config.maxPlayers = 20;
    config.port = 25565;

    GameServer server(config);
    server.run(); // Blocks until server shuts down
}

void runMultiplayerClient(const std::string& serverAddress) {
    std::cout << "Starting Tidal Engine Client..." << std::endl;

    ClientConfig config;
    config.serverAddress = serverAddress;
    config.serverPort = 25565;
    config.playerName = "Player1";
    config.windowWidth = 1200;
    config.windowHeight = 800;

    GameClient client(config);

    if (!client.initialize()) {
        std::cerr << "Failed to initialize client!" << std::endl;
        return;
    }

    // Connect to remote server
    if (!client.connectToServer(serverAddress, config.serverPort)) {
        std::cerr << "Failed to connect to server!" << std::endl;
        return;
    }

    client.run(); // Blocks until client shuts down
}

void runSingleplayer() {
    std::cout << "Starting Tidal Engine Singleplayer..." << std::endl;

    // Create local server configuration
    ServerConfig serverConfig;
    serverConfig.worldName = "singleplayer_world";
    serverConfig.serverName = "Local Server";
    serverConfig.maxPlayers = 1;
    serverConfig.autosaveInterval = 30.0f; // More frequent saves in singleplayer

    // Create client configuration
    ClientConfig clientConfig;
    clientConfig.playerName = "Player";
    clientConfig.windowWidth = 1200;
    clientConfig.windowHeight = 800;
    clientConfig.renderDistance = 8;

    // Create both server and client
    auto server = std::make_unique<GameServer>(serverConfig);
    auto client = std::make_unique<GameClient>(clientConfig);

    // Initialize server
    if (!server->initialize()) {
        std::cerr << "Failed to initialize local server!" << std::endl;
        return;
    }

    // Start server in background thread
    std::thread serverThread([&server]() {
        server->run();
    });

    // Initialize client
    if (!client->initialize()) {
        std::cerr << "Failed to initialize client!" << std::endl;
        server->stop();
        if (serverThread.joinable()) {
            serverThread.join();
        }
        return;
    }

    // Connect client to local server
    client->connectToLocalServer(server.get());

    std::cout << "Singleplayer game ready!" << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "  WASD - Move" << std::endl;
    std::cout << "  Mouse - Look around" << std::endl;
    std::cout << "  Left Click - Break blocks" << std::endl;
    std::cout << "  Right Click - Place blocks" << std::endl;
    std::cout << "  1-8 - Select block type" << std::endl;
    std::cout << "  F3 - Toggle debug info" << std::endl;
    std::cout << "  ESC - Toggle mouse capture" << std::endl;

    // Run client (blocks until client shuts down)
    client->run();

    // Cleanup
    std::cout << "Shutting down singleplayer..." << std::endl;
    client->disconnect();
    server->stop();

    if (serverThread.joinable()) {
        serverThread.join();
    }

    std::cout << "Singleplayer shutdown complete." << std::endl;
}

void runMainMenu() {
    std::cout << "Starting Tidal Engine Main Menu..." << std::endl;

    // Create client configuration for menu mode
    ClientConfig clientConfig;
    clientConfig.windowWidth = 1200;
    clientConfig.windowHeight = 800;

    // Create client in menu mode (no server connection)
    auto client = std::make_unique<GameClient>(clientConfig);

    // Initialize client graphics/window but don't connect to any server
    if (!client->initialize()) {
        std::cerr << "Failed to initialize client!" << std::endl;
        return;
    }

    std::cout << "Main Menu ready!" << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "  Use ImGui interface to navigate menu" << std::endl;
    std::cout << "  ESC - Quit" << std::endl;

    // Run client in menu mode (will show main menu UI)
    client->run();

    std::cout << "Main Menu shutdown complete." << std::endl;
}

int main(int argc, char* argv[]) {
    std::cout << "Tidal Engine - Voxel Game Engine" << std::endl;
    std::cout << "Client-Server Architecture Demo" << std::endl;
    std::cout << "================================" << std::endl;

    if (argc == 1) {
        // Default: start with main menu
        runMainMenu();
    } else if (argc == 2) {
        std::string arg = argv[1];
        if (arg == "--server") {
            runDedicatedServer();
        } else if (arg == "--help" || arg == "-h") {
            printUsage();
            return 0;
        } else {
            std::cerr << "Unknown argument: " << arg << std::endl;
            printUsage();
            return 1;
        }
    } else if (argc == 3) {
        std::string arg1 = argv[1];
        std::string arg2 = argv[2];
        if (arg1 == "--client") {
            runMultiplayerClient(arg2);
        } else {
            std::cerr << "Unknown arguments" << std::endl;
            printUsage();
            return 1;
        }
    } else {
        std::cerr << "Too many arguments" << std::endl;
        printUsage();
        return 1;
    }

    return 0;
}