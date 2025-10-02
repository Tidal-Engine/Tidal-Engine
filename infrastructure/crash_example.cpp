// Crash Handling Examples
// This file demonstrates crash detection and stack trace capabilities

#include "../include/core/Logger.hpp"
#include "../include/core/CrashHandler.hpp"
#include <vulkan/vulkan.h>
#include <vector>
#include <stdexcept>

void functionA();
void functionB();
void functionC();

// Example 1: Automatic crash detection
void simulateCrash() {
    LOG_INFO("Simulating a crash (segmentation fault)...");
    int* ptr = nullptr;
    *ptr = 42;  // This will crash and cpptrace will catch it
}

// Example 2: Manual stack trace
void functionC() {
    LOG_INFO("Function C called");

    // Manually print stack trace for debugging
    LOG_INFO("Printing stack trace from function C:");
    engine::CrashHandler::printStackTrace();
}

void functionB() {
    LOG_INFO("Function B called");
    functionC();
}

void functionA() {
    LOG_INFO("Function A called");
    functionB();
}

// Example 3: Exception handling with stack trace
void riskyOperation() {
    try {
        // Simulate an error condition
        throw std::runtime_error("Something went wrong!");
    } catch (const std::exception& e) {
        LOG_ERROR("Caught exception: {}", e.what());
        LOG_ERROR("Stack trace at exception:");
        engine::CrashHandler::logStackTrace();
    }
}

// Example 4: Array bounds checking with trace
void arrayBoundsExample() {
    std::vector<int> data = {1, 2, 3, 4, 5};
    int index = 10;  // Out of bounds

    if (index >= static_cast<int>(data.size())) {
        LOG_ERROR("Array index out of range! Index: {}, Size: {}", index, data.size());
        LOG_ERROR("Stack trace:");
        engine::CrashHandler::printStackTrace();
    }
}

// Example 5: Vulkan error handling pattern
void vulkanErrorPattern() {
    VkResult result = VK_ERROR_DEVICE_LOST;  // Simulate Vulkan error

    if (result != VK_SUCCESS) {
        LOG_ERROR("Vulkan operation failed with error: {}", static_cast<int>(result));
        LOG_ERROR("Call stack:");
        engine::CrashHandler::logStackTrace();

        // In real code, you might want to:
        // - Clean up resources
        // - Try to recover
        // - Exit gracefully
    }
}

int main() {
    // Initialize infrastructure
    engine::Logger::init();
    engine::CrashHandler::init();  // cpptrace auto-installs handlers

    LOG_INFO("=== Crash Handler Examples (cpptrace) ===\n");

    // Example 1: Manual stack trace
    LOG_INFO("Example 1: Manual Stack Trace");
    functionA();

    // Example 2: Exception handling
    LOG_INFO("\nExample 2: Exception with Stack Trace");
    riskyOperation();

    // Example 3: Array bounds checking
    LOG_INFO("\nExample 3: Array Bounds Check");
    arrayBoundsExample();

    // Example 4: Vulkan error pattern
    LOG_INFO("\nExample 4: Vulkan Error Handling");
    vulkanErrorPattern();

    // UNCOMMENT TO TEST CRASH DETECTION:
    // LOG_INFO("\nExample 5: Automatic Crash Detection");
    // simulateCrash();  // This will crash the program, but cpptrace will print a detailed trace

    LOG_INFO("\nAll examples completed successfully!");
    LOG_INFO("\ncpptrace advantages over backward-cpp:");
    LOG_INFO("- Better cross-platform support (Windows/Linux/Mac)");
    LOG_INFO("- Cleaner output with colors");
    LOG_INFO("- More accurate line numbers");
    LOG_INFO("- Active development and modern C++");

    engine::Logger::shutdown();

    return 0;
}
