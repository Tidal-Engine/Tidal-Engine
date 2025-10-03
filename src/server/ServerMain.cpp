#include "core/Logger.hpp"
#include "core/CrashHandler.hpp"
#include "server/GameServer.hpp"

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

        // Input thread to listen for /stop command
        std::thread inputThread([&]() {
            std::string line;
            while (!g_shutdownRequested && std::getline(std::cin, line)) {
                if (line == "/stop" || line == "stop") {
                    LOG_INFO("Stop command received");
                    g_shutdownRequested = true;
                    break;
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
