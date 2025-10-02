#include "core/Logger.hpp"
#include "core/CrashHandler.hpp"
#include "vulkan/VulkanEngine.hpp"

#include <exception>

int main(int argc, char* argv[]) {
    // Initialize infrastructure
    engine::Logger::init();
    engine::CrashHandler::init();

    LOG_INFO("=== Tidal Engine Starting ===");

    try {
        engine::VulkanEngine engine;
        engine.run();
    } catch (const std::exception& e) {
        LOG_CRITICAL("Fatal error: {}", e.what());
        engine::CrashHandler::logStackTrace();
        engine::Logger::shutdown();
        return 1;
    }

    LOG_INFO("=== Tidal Engine Shutdown ===");
    engine::Logger::shutdown();

    return 0;
}
