#pragma once

#include <cstdint>
#include <string>

namespace engine {

/**
 * @brief Centralized engine configuration
 *
 * Contains all configurable constants for the engine. These can be
 * modified at compile-time or loaded from a config file at runtime.
 */
struct EngineConfig {
    // Window settings
    static constexpr uint32_t DEFAULT_WINDOW_WIDTH = 800;
    static constexpr uint32_t DEFAULT_WINDOW_HEIGHT = 600;
    static constexpr const char* DEFAULT_WINDOW_TITLE = "Tidal Engine - Voxel Renderer";

    // Rendering settings
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
    static constexpr float FOV_DEGREES = 45.0f;
    static constexpr float NEAR_PLANE = 0.1f;
    static constexpr float FAR_PLANE = 1000.0f;

    // Camera settings
    static constexpr float DEFAULT_CAMERA_SPEED = 2.5f;
    static constexpr float DEFAULT_MOUSE_SENSITIVITY = 0.1f;

    // Vulkan settings
    static constexpr const char* ENGINE_NAME = "Tidal Engine";
    static constexpr uint32_t ENGINE_VERSION_MAJOR = 0;
    static constexpr uint32_t ENGINE_VERSION_MINOR = 1;
    static constexpr uint32_t ENGINE_VERSION_PATCH = 0;

    // Runtime configurable settings (loaded from config file or defaults)
    struct Runtime {
        uint32_t windowWidth = DEFAULT_WINDOW_WIDTH;
        uint32_t windowHeight = DEFAULT_WINDOW_HEIGHT;
        std::string windowTitle = DEFAULT_WINDOW_TITLE;
        bool fullscreen = false;
        bool vsync = true;
        float fov = FOV_DEGREES;
        float cameraSpeed = DEFAULT_CAMERA_SPEED;
        float mouseSensitivity = DEFAULT_MOUSE_SENSITIVITY;
    };
};

} // namespace engine
