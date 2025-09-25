#include "GameServer.h"
#include "NetworkManager.h"
#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>

// Global server instance for signal handling
static std::unique_ptr<GameServer> g_server;
static std::atomic<bool> g_running{true};

// Signal handler for graceful shutdown
void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down server..." << std::endl;
    g_running = false;
    if (g_server) {
        g_server->stop();
    }
}

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -p, --port <port>          Server port (default: 25565)" << std::endl;
    std::cout << "  -w, --world <name>         World name (default: default)" << std::endl;
    std::cout << "  -n, --name <name>          Server name (default: Tidal Engine Server)" << std::endl;
    std::cout << "  -m, --max-players <num>    Maximum players (default: 20)" << std::endl;
    std::cout << "  -b, --bind <address>       Bind address (default: 0.0.0.0)" << std::endl;
    std::cout << "  -s, --saves <directory>    Saves directory (default: saves)" << std::endl;
    std::cout << "  -h, --help                 Show this help message" << std::endl;
    std::cout << std::endl;
    std::cout << "Commands (while running):" << std::endl;
    std::cout << "  help                       Show available commands" << std::endl;
    std::cout << "  stop                       Stop the server" << std::endl;
    std::cout << "  save                       Save the world" << std::endl;
    std::cout << "  list                       List connected players" << std::endl;
    std::cout << "  status                     Show server status" << std::endl;
    std::cout << "  broadcast <message>        Broadcast message to all players" << std::endl;
}

int main(int argc, char* argv[]) {
    std::cout << "Tidal Engine Dedicated Server" << std::endl;
    std::cout << "=============================" << std::endl;

    // Parse command line arguments
    ServerConfig config;
    config.serverName = "Tidal Engine Server";
    config.worldName = "default";
    config.port = 25565;
    config.maxPlayers = 20;
    config.savesDirectory = "saves";
    config.enableServerCommands = true;
    config.enableRemoteAccess = true;
    std::string bindAddress = "0.0.0.0";

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if ((arg == "-p" || arg == "--port") && i + 1 < argc) {
            config.port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if ((arg == "-w" || arg == "--world") && i + 1 < argc) {
            config.worldName = argv[++i];
        } else if ((arg == "-n" || arg == "--name") && i + 1 < argc) {
            config.serverName = argv[++i];
        } else if ((arg == "-m" || arg == "--max-players") && i + 1 < argc) {
            config.maxPlayers = std::stoi(argv[++i]);
        } else if ((arg == "-b" || arg == "--bind") && i + 1 < argc) {
            bindAddress = argv[++i];
        } else if ((arg == "-s" || arg == "--saves") && i + 1 < argc) {
            config.savesDirectory = argv[++i];
        } else {
            std::cerr << "Unknown argument: " << arg << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }

    // Set up signal handlers for graceful shutdown
    std::signal(SIGINT, signalHandler);   // Ctrl+C
    std::signal(SIGTERM, signalHandler);  // Terminate
#ifdef SIGQUIT
    std::signal(SIGQUIT, signalHandler);  // Quit (Unix)
#endif

    try {
        // Create and initialize server
        g_server = std::make_unique<GameServer>(config);

        if (!g_server->initialize()) {
            std::cerr << "Failed to initialize server!" << std::endl;
            return 1;
        }

        // Start network server
        if (!g_server->startNetworkServer(config.port, bindAddress)) {
            std::cerr << "Failed to start network server on " << bindAddress << ":" << config.port << std::endl;
            return 1;
        }

        std::cout << "Server started successfully!" << std::endl;
        std::cout << "Server Name: " << config.serverName << std::endl;
        std::cout << "World: " << config.worldName << std::endl;
        std::cout << "Listening on: " << bindAddress << ":" << config.port << std::endl;
        std::cout << "Max Players: " << config.maxPlayers << std::endl;
        std::cout << "Type 'help' for commands or Ctrl+C to stop." << std::endl;
        std::cout << std::endl;

        // Start console input thread
        std::thread consoleThread([&]() {
            std::string input;
            while (g_running && std::getline(std::cin, input)) {
                if (!input.empty()) {
                    g_server->processConsoleCommand(input);

                    // Check for stop command
                    if (input == "stop") {
                        g_running = false;
                        break;
                    }
                }
            }
        });

        // Main server loop
        auto lastTickTime = std::chrono::steady_clock::now();
        const auto tickInterval = std::chrono::milliseconds(50); // 20 TPS

        while (g_running && g_server->isRunning()) {
            auto currentTime = std::chrono::steady_clock::now();
            auto deltaTime = std::chrono::duration<float>(currentTime - lastTickTime).count();

            // Process server tick (game logic, physics, etc.)
            // g_server->tick(deltaTime);

            lastTickTime = currentTime;

            // Sleep until next tick
            std::this_thread::sleep_until(currentTime + tickInterval);
        }

        // Shutdown
        std::cout << "Shutting down server..." << std::endl;

        if (consoleThread.joinable()) {
            consoleThread.detach(); // Don't wait for console input
        }

        g_server->shutdown();
        g_server.reset();

        std::cout << "Server shutdown complete." << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Server error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}