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
                // Union access required by SDL3 API
                if (!keyState[event.key.scancode]) {  // NOLINT(cppcoreguidelines-pro-type-union-access)
                    keyPressedThisFrame[event.key.scancode] = true;  // NOLINT(cppcoreguidelines-pro-type-union-access)
                }
                keyState[event.key.scancode] = true;  // NOLINT(cppcoreguidelines-pro-type-union-access)
                break;

            case SDL_EVENT_KEY_UP:
                keyState[event.key.scancode] = false;  // NOLINT(cppcoreguidelines-pro-type-union-access)
                break;

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                mouseButtonState[event.button.button] = true;  // NOLINT(cppcoreguidelines-pro-type-union-access)
                break;

            case SDL_EVENT_MOUSE_BUTTON_UP:
                mouseButtonState[event.button.button] = false;  // NOLINT(cppcoreguidelines-pro-type-union-access)
                break;

            case SDL_EVENT_MOUSE_MOTION:
                // Union access required by SDL3 API
                mousePosition.x = event.motion.x;  // NOLINT(cppcoreguidelines-pro-type-union-access)
                mousePosition.y = event.motion.y;  // NOLINT(cppcoreguidelines-pro-type-union-access)
                mouseDelta.x = event.motion.xrel;  // NOLINT(cppcoreguidelines-pro-type-union-access)
                mouseDelta.y = event.motion.yrel;  // NOLINT(cppcoreguidelines-pro-type-union-access)
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
