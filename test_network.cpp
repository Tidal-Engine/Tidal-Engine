/**
 * Simple test to verify client-server networking
 * Compile with: clang++ -std=c++20 test_network.cpp -o test_network -I./include -L./build -lTidalShared -lenet -lspdlog -lpthread
 */

#include "core/Logger.hpp"
#include "client/NetworkClient.hpp"
#include <thread>
#include <chrono>

int main() {
    engine::Logger::init();

    LOG_INFO("=== Network Test ===");
    LOG_INFO("Make sure TidalServer is running on localhost:25565");

    // Wait a bit for user to start server
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Create client
    engine::NetworkClient client;

    // Set up callback for chunk reception
    int chunksReceived = 0;
    client.setOnChunkReceived([&](const engine::ChunkCoord& coord) {
        chunksReceived++;
        LOG_INFO("Received chunk {} ({}, {}, {})",
                 chunksReceived, coord.x, coord.y, coord.z);
    });

    // Connect to server
    if (!client.connect("127.0.0.1", 25565)) {
        LOG_ERROR("Failed to connect to server!");
        return 1;
    }

    LOG_INFO("Connected! Waiting for chunks...");

    // Process network messages for 5 seconds
    auto startTime = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - startTime < std::chrono::seconds(5)) {
        client.update();
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
    }

    LOG_INFO("Test complete! Received {} chunks", chunksReceived);
    LOG_INFO("Total chunks stored: {}", client.getChunks().size());

    client.disconnect();
    engine::Logger::shutdown();

    return 0;
}
