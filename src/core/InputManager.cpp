#include "core/InputManager.hpp"

namespace engine {

bool InputManager::processEvents() {
    // Clear per-frame state
    keyPressedThisFrame.clear();
    mouseDelta = glm::vec2(0.0f, 0.0f);

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_QUIT:
                return false;

            case SDL_EVENT_KEY_DOWN:
                if (!keyState[event.key.scancode]) {
                    keyPressedThisFrame[event.key.scancode] = true;
                }
                keyState[event.key.scancode] = true;
                break;

            case SDL_EVENT_KEY_UP:
                keyState[event.key.scancode] = false;
                break;

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                mouseButtonState[event.button.button] = true;
                break;

            case SDL_EVENT_MOUSE_BUTTON_UP:
                mouseButtonState[event.button.button] = false;
                break;

            case SDL_EVENT_MOUSE_MOTION:
                mousePosition.x = static_cast<float>(event.motion.x);
                mousePosition.y = static_cast<float>(event.motion.y);
                mouseDelta.x = static_cast<float>(event.motion.xrel);
                mouseDelta.y = static_cast<float>(event.motion.yrel);
                break;

            default:
                break;
        }
    }

    return true;
}

bool InputManager::isKeyPressed(SDL_Scancode key) const {
    auto iter = keyState.find(key);
    return iter != keyState.end() && iter->second;
}

bool InputManager::isKeyJustPressed(SDL_Scancode key) const {
    auto iter = keyPressedThisFrame.find(key);
    return iter != keyPressedThisFrame.end() && iter->second;
}

bool InputManager::isMouseButtonPressed(uint8_t button) const {
    auto iter = mouseButtonState.find(button);
    return iter != mouseButtonState.end() && iter->second;
}

void InputManager::endFrame() {
    keyPressedThisFrame.clear();
    mouseDelta = glm::vec2(0.0f, 0.0f);
}

} // namespace engine
