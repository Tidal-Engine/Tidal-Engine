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
    static constexpr uint32_t DEFAULT_WINDOW_WIDTH = 800;                          ///< Default window width in pixels
    static constexpr uint32_t DEFAULT_WINDOW_HEIGHT = 600;                         ///< Default window height in pixels
    static constexpr const char* DEFAULT_WINDOW_TITLE = "Tidal Engine - Voxel Renderer";  ///< Default window title

    // Rendering settings
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;      ///< Maximum frames that can be processed concurrently
    static constexpr float FOV_DEGREES = 45.0f;         ///< Default field of view in degrees
    static constexpr float NEAR_PLANE = 0.1f;           ///< Near clipping plane distance
    static constexpr float FAR_PLANE = 1000.0f;         ///< Far clipping plane distance

    // Camera settings
    static constexpr float DEFAULT_CAMERA_SPEED = 2.5f;         ///< Default camera movement speed (units/second)
    static constexpr float DEFAULT_MOUSE_SENSITIVITY = 0.1f;    ///< Default mouse look sensitivity

    // Vulkan settings
    static constexpr const char* ENGINE_NAME = "Tidal Engine";  ///< Engine name for Vulkan application info
    static constexpr uint32_t ENGINE_VERSION_MAJOR = 0;         ///< Engine major version number
    static constexpr uint32_t ENGINE_VERSION_MINOR = 1;         ///< Engine minor version number
    static constexpr uint32_t ENGINE_VERSION_PATCH = 0;         ///< Engine patch version number

    /**
     * @brief Runtime configurable settings
     *
     * Settings that can be loaded from config file or modified at runtime.
     * Defaults are taken from the static constants above.
     */
    struct Runtime {
        uint32_t windowWidth = DEFAULT_WINDOW_WIDTH;            ///< Current window width in pixels
        uint32_t windowHeight = DEFAULT_WINDOW_HEIGHT;          ///< Current window height in pixels
        std::string windowTitle = DEFAULT_WINDOW_TITLE;         ///< Current window title
        bool fullscreen = false;                                ///< Enable fullscreen mode
        bool vsync = true;                                      ///< Enable vertical sync
        float fov = FOV_DEGREES;                                ///< Field of view in degrees
        float cameraSpeed = DEFAULT_CAMERA_SPEED;               ///< Camera movement speed (units/second)
        float mouseSensitivity = DEFAULT_MOUSE_SENSITIVITY;     ///< Mouse look sensitivity multiplier
    };
};

} // namespace engine
