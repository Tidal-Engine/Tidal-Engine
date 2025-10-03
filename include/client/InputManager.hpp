#pragma once

#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <unordered_map>

namespace engine {

/**
 * @brief Handles keyboard and mouse input for the engine
 *
 * Provides a simple interface for querying input state and mouse movement.
 * Updates are called once per frame to poll SDL events.
 */
class InputManager {
public:
    InputManager() = default;
    ~InputManager() = default;

    // Delete copy operations
    InputManager(const InputManager&) = delete;
    InputManager& operator=(const InputManager&) = delete;

    // Allow move operations
    InputManager(InputManager&&) noexcept = default;
    InputManager& operator=(InputManager&&) noexcept = default;

    /**
     * @brief Begin a new input frame (call before polling events)
     */
    void beginFrame();

    /**
     * @brief Handle a single SDL event
     * @param event SDL event to process
     */
    void handleEvent(const SDL_Event& event);

    /**
     * @brief Check if a key is currently pressed
     * @param key SDL_Scancode of the key to check
     * @return true if key is pressed
     */
    bool isKeyPressed(SDL_Scancode key) const;

    /**
     * @brief Check if a key was just pressed this frame
     * @param key SDL_Scancode of the key to check
     * @return true if key was just pressed
     */
    bool isKeyJustPressed(SDL_Scancode key) const;

    /**
     * @brief Get mouse movement delta since last frame
     * @return glm::vec2 with x and y movement
     */
    glm::vec2 getMouseDelta() const { return mouseDelta; }

    /**
     * @brief Get current mouse position
     * @return glm::vec2 with x and y position
     */
    glm::vec2 getMousePosition() const { return mousePosition; }

    /**
     * @brief Check if mouse button is pressed
     * @param button SDL mouse button constant
     * @return true if button is pressed
     */
    bool isMouseButtonPressed(uint8_t button) const;

    /**
     * @brief Reset per-frame state (call at end of frame)
     */
    void endFrame();

private:
    std::unordered_map<SDL_Scancode, bool> keyState;
    std::unordered_map<SDL_Scancode, bool> keyPressedThisFrame;
    std::unordered_map<uint8_t, bool> mouseButtonState;

    glm::vec2 mouseDelta{0.0f, 0.0f};
    glm::vec2 mousePosition{0.0f, 0.0f};
    glm::vec2 lastMousePosition{0.0f, 0.0f};
};

} // namespace engine
