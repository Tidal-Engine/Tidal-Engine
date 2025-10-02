// Logging System Examples
// This file demonstrates all logging capabilities

#include "../include/core/Logger.hpp"
#include <vulkan/vulkan.h>
#include <thread>
#include <chrono>

void basicLogging() {
    // Different log levels
    LOG_TRACE("This is a trace message (most verbose)");
    LOG_DEBUG("This is a debug message");
    LOG_INFO("This is an info message");
    LOG_WARN("This is a warning message");
    LOG_ERROR("This is an error message");
    LOG_CRITICAL("This is a critical message (highest severity)");
}

void formattedLogging() {
    // Format strings like printf
    int frameNumber = 42;
    float deltaTime = 0.016f;

    LOG_INFO("Frame {} completed in {:.3f}ms", frameNumber, deltaTime);
    LOG_DEBUG("Vulkan device: {} ({} MB VRAM)", "NVIDIA RTX 4090", 24576);
}

void multipleLoggers() {
    // Create subsystem-specific loggers
    auto vulkanLogger = engine::Logger::create("Vulkan");
    auto rendererLogger = engine::Logger::create("Renderer");
    auto physicsLogger = engine::Logger::create("Physics");

    // Each logger can have different settings and outputs
    vulkanLogger->info("Vulkan instance created");
    rendererLogger->info("Renderer initialized");
    physicsLogger->info("Physics world created");

    // Get logger by name later
    auto vk = engine::Logger::get("Vulkan");
    vk->debug("Creating swapchain...");
}

void threadSafeLogging() {
    // Logging is thread-safe and async
    auto worker = [](int id) {
        for (int i = 0; i < 5; i++) {
            LOG_INFO("Thread {} - iteration {}", id, i);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    };

    std::thread t1(worker, 1);
    std::thread t2(worker, 2);
    std::thread t3(worker, 3);

    t1.join();
    t2.join();
    t3.join();
}

void conditionalLogging() {
    // Only log if condition is met (no overhead if false)
    bool debugMode = true;

    if (debugMode) {
        LOG_DEBUG("Debug mode is enabled");
    }

    // Log errors with context
    VkResult result = VK_SUCCESS; // Pretend this is from Vulkan
    if (result != VK_SUCCESS) {
        LOG_ERROR("Vulkan operation failed with error code: {}", static_cast<int>(result));
    }
}

int main() {
    // Initialize logging system
    engine::Logger::init();

    LOG_INFO("=== Basic Logging ===");
    basicLogging();

    LOG_INFO("\n=== Formatted Logging ===");
    formattedLogging();

    LOG_INFO("\n=== Multiple Loggers ===");
    multipleLoggers();

    LOG_INFO("\n=== Thread-Safe Logging ===");
    threadSafeLogging();

    LOG_INFO("\n=== Conditional Logging ===");
    conditionalLogging();

    // Shutdown (flushes all pending logs)
    engine::Logger::shutdown();

    return 0;
}
