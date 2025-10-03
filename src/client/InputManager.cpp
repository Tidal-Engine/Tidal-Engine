#include "client/InputManager.hpp"
#include "core/Logger.hpp"

namespace engine {

void InputManager::beginFrame() {
    // Clear per-frame state
    keyPressedThisFrame.clear();
    mouseDelta = glm::vec2(0.0f, 0.0f);
}

void InputManager::handleEvent(const SDL_Event& event) {
    switch (event.type) {
        case SDL_EVENT_KEY_DOWN:
            // Union access required by SDL3 API
            LOG_TRACE("Key down: scancode {}", static_cast<int>(event.key.scancode));  // NOLINT(cppcoreguidelines-pro-type-union-access)
            if (!keyState[event.key.scancode]) {  // NOLINT(cppcoreguidelines-pro-type-union-access)
                keyPressedThisFrame[event.key.scancode] = true;  // NOLINT(cppcoreguidelines-pro-type-union-access)
            }
            keyState[event.key.scancode] = true;  // NOLINT(cppcoreguidelines-pro-type-union-access)
            break;

        case SDL_EVENT_KEY_UP:
            LOG_TRACE("Key up: scancode {}", static_cast<int>(event.key.scancode));  // NOLINT(cppcoreguidelines-pro-type-union-access)
            keyState[event.key.scancode] = false;  // NOLINT(cppcoreguidelines-pro-type-union-access)
            break;

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            LOG_TRACE("Mouse button down: {}", static_cast<int>(event.button.button));  // NOLINT(cppcoreguidelines-pro-type-union-access)
            mouseButtonState[event.button.button] = true;  // NOLINT(cppcoreguidelines-pro-type-union-access)
            break;

        case SDL_EVENT_MOUSE_BUTTON_UP:
            LOG_TRACE("Mouse button up: {}", static_cast<int>(event.button.button));  // NOLINT(cppcoreguidelines-pro-type-union-access)
            mouseButtonState[event.button.button] = false;  // NOLINT(cppcoreguidelines-pro-type-union-access)
            break;

        case SDL_EVENT_MOUSE_MOTION:
            // Union access required by SDL3 API
            LOG_TRACE("Mouse motion: pos({}, {}), delta({}, {})",   // NOLINT(cppcoreguidelines-pro-type-union-access)
                     static_cast<float>(event.motion.x), static_cast<float>(event.motion.y),    // NOLINT(cppcoreguidelines-pro-type-union-access)
                     static_cast<float>(event.motion.xrel), static_cast<float>(event.motion.yrel));  // NOLINT(cppcoreguidelines-pro-type-union-access)
            mousePosition.x = event.motion.x;  // NOLINT(cppcoreguidelines-pro-type-union-access)
            mousePosition.y = event.motion.y;  // NOLINT(cppcoreguidelines-pro-type-union-access)
            mouseDelta.x += event.motion.xrel;  // NOLINT(cppcoreguidelines-pro-type-union-access)
            mouseDelta.y += event.motion.yrel;  // NOLINT(cppcoreguidelines-pro-type-union-access)
            break;

        default:
            // Unknown/unhandled event type - not necessarily an error
            break;
    }
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
