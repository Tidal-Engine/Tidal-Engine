#include "core/Logger.hpp"
#include "core/CrashHandler.hpp"
#include "server/GameServer.hpp"
#include "server/World.hpp"

#include <exception>
#include <csignal>
#include <atomic>
#include <thread>
#include <iostream>
#include <string>

// Global flag for graceful shutdown
std::atomic<bool> g_shutdownRequested{false};

void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        LOG_WARN("Shutdown signal received");
        g_shutdownRequested = true;
    }
}

int main(int argc, char* argv[]) {
    // Initialize infrastructure
    engine::Logger::init("TidalEngine", "logs/server.log");
    engine::CrashHandler::init();

    // Setup signal handlers for graceful shutdown
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    LOG_INFO("=== Tidal Engine Dedicated Server Starting ===");

    try {
        // Create server (40 TPS for smooth automation)
        engine::GameServer server(25565, 40.0);

        // Run server in a separate thread so we can check for shutdown signal
        std::thread serverThread([&server]() {
            server.run();
        });

        // Input thread to listen for commands
        std::thread inputThread([&]() {
            std::string line;
            while (!g_shutdownRequested && std::getline(std::cin, line)) {
                // Trim whitespace
                line.erase(0, line.find_first_not_of(" \t\n\r"));
                line.erase(line.find_last_not_of(" \t\n\r") + 1);

                if (line.empty()) {
                    continue;
                }

                // Handle commands
                if (line == "/stop" || line == "stop") {
                    LOG_INFO("Stop command received");
                    g_shutdownRequested = true;
                    break;
                }
                else if (line == "/tunnel stop" || line == "tunnel stop") {
                    server.stopTunnel();
                }
                else if (line.rfind("/tunnel start", 0) == 0 || line.rfind("tunnel start", 0) == 0) {
                    // Extract secret key if provided
                    size_t spacePos = line.find(' ', line.find("start") + 5);
                    std::string secretKey;
                    if (spacePos != std::string::npos) {
                        secretKey = line.substr(spacePos + 1);
                        // Trim whitespace from key
                        secretKey.erase(0, secretKey.find_first_not_of(" \t\n\r"));
                        secretKey.erase(secretKey.find_last_not_of(" \t\n\r") + 1);
                    }
                    server.startTunnel(secretKey);
                }
                else if (line == "/tunnel status" || line == "tunnel status") {
                    if (server.isTunnelRunning()) {
                        LOG_INFO("Tunnel is currently running");
                        LOG_INFO("Check https://playit.gg/account for tunnel address");
                    } else {
                        LOG_INFO("Tunnel is not running");
                    }
                }
                else if (line == "/save" || line == "save") {
                    LOG_INFO("Saving world...");
                    size_t chunks = server.getWorld()->saveWorld("world");
                    LOG_INFO("Saved {} chunks", chunks);
                }
                else if (line == "/help" || line == "help") {
                    LOG_INFO("========================================");
                    LOG_INFO("Available commands:");
                    LOG_INFO("  /stop - Stop the server");
                    LOG_INFO("  /save - Save world to disk");
                    LOG_INFO("  /tunnel start [secret-key] - Start playit.gg tunnel");
                    LOG_INFO("  /tunnel stop - Stop playit.gg tunnel");
                    LOG_INFO("  /tunnel status - Check tunnel status");
                    LOG_INFO("  /help - Show this help message");
                    LOG_INFO("========================================");
                }
                else {
                    LOG_WARN("Unknown command: {}", line);
                    LOG_INFO("Type '/help' for available commands");
                }
            }
        });

        // Wait for shutdown signal
        while (!g_shutdownRequested) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // Stop server gracefully
        server.stop();
        serverThread.join();

        // Clean up input thread
        if (inputThread.joinable()) {
            inputThread.detach();  // Detach since getline might be blocking
        }

    } catch (const std::exception& e) {
        LOG_CRITICAL("Fatal server error: {}", e.what());
        engine::CrashHandler::logStackTrace();
        engine::Logger::shutdown();
        return 1;
    }

    LOG_INFO("=== Tidal Engine Server Shutdown ===");
    engine::Logger::shutdown();

    return 0;
}
