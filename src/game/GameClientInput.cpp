#include "game/GameClientInput.h"
#include <vulkan/vulkan.h>
#include "game/GameClient.h"
#include "game/GameClientUI.h"
#include <imgui.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <cstring>

GameClientInput::GameClientInput(GameClient& gameClient, GLFWwindow* window, Camera& camera)
    : m_gameClient(gameClient), m_window(window), m_camera(camera) {

    // Initialize input state
    memset(m_keys, false, sizeof(m_keys));
    m_firstMouse = true;
    m_lastX = 0.0f;
    m_lastY = 0.0f;

    // Set initial cursor mode based on game state
    bool shouldCapture = (m_gameClient.getGameState() != GameState::MAIN_MENU);
    setMouseCaptured(shouldCapture);
}

void GameClientInput::processInput(float deltaTime) {
    // Speed boost when holding Ctrl
    float speedMultiplier = m_keys[GLFW_KEY_LEFT_CONTROL] || m_keys[GLFW_KEY_RIGHT_CONTROL] ? 5.0f : 1.0f;
    float adjustedDeltaTime = deltaTime * speedMultiplier;

    // Camera movement - only when cursor is captured
    if (m_mouseCaptured) {
        if (m_keys[GLFW_KEY_W]) m_camera.processKeyboard(CameraMovement::FORWARD, adjustedDeltaTime);
        if (m_keys[GLFW_KEY_S]) m_camera.processKeyboard(CameraMovement::BACKWARD, adjustedDeltaTime);
        if (m_keys[GLFW_KEY_A]) m_camera.processKeyboard(CameraMovement::LEFT, adjustedDeltaTime);
        if (m_keys[GLFW_KEY_D]) m_camera.processKeyboard(CameraMovement::RIGHT, adjustedDeltaTime);
        if (m_keys[GLFW_KEY_SPACE]) m_camera.processKeyboard(CameraMovement::UP, adjustedDeltaTime);
        if (m_keys[GLFW_KEY_LEFT_SHIFT]) m_camera.processKeyboard(CameraMovement::DOWN, adjustedDeltaTime);
    }

    // Hotbar selection - delegate to GameClient
    for (int i = GLFW_KEY_1; i <= GLFW_KEY_8; i++) {
        if (m_keys[i]) {
            int slot = i - GLFW_KEY_1;
            if (slot < 8 && slot != m_gameClient.getSelectedHotbarSlot()) {
                std::cout << "Hotbar selection changed from " << m_gameClient.getSelectedHotbarSlot() << " to " << slot
                          << " (key " << (i - GLFW_KEY_1 + 1) << " pressed)" << std::endl;
                m_gameClient.setSelectedHotbarSlot(slot);
            }
        }
    }

    // Debug window toggle (F1)
    if (m_keys[GLFW_KEY_F1]) {
        if (m_gameClient.getUI()) {
            m_gameClient.getUI()->toggleDebugWindow();
        }
        m_keys[GLFW_KEY_F1] = false; // Prevent rapid toggling
    }

    // Toggle cursor capture with F2
    if (m_keys[GLFW_KEY_F2]) {
        setMouseCaptured(!m_mouseCaptured);
        m_keys[GLFW_KEY_F2] = false;
    }

    // Toggle wireframe mode with F3
    if (m_keys[GLFW_KEY_F3]) {
        m_gameClient.toggleWireframeMode();
        m_keys[GLFW_KEY_F3] = false;
    }

    // Toggle chunk boundaries with F4
    if (m_keys[GLFW_KEY_F4]) {
        if (m_gameClient.getUI()) {
            bool current = m_gameClient.getUI()->isShowingChunkBoundaries();
            m_gameClient.getUI()->setShowChunkBoundaries(!current);
        }
        m_keys[GLFW_KEY_F4] = false;
    }

    // ESC key handling - toggle between IN_GAME and PAUSED states
    if (m_keys[GLFW_KEY_ESCAPE]) {
        auto currentState = m_gameClient.getGameState();
        if (currentState == GameState::IN_GAME) {
            m_gameClient.setGameState(GameState::PAUSED);
            setMouseCaptured(false);
        } else if (currentState == GameState::PAUSED) {
            m_gameClient.setGameState(GameState::IN_GAME);
            setMouseCaptured(true);
        }
        m_keys[GLFW_KEY_ESCAPE] = false;
    }
}

void GameClientInput::handleMouseInput(double xpos, double ypos) {
    if (!m_mouseCaptured) return;

    if (m_firstMouse) {
        m_lastX = xpos;
        m_lastY = ypos;
        m_firstMouse = false;
    }

    float xoffset = xpos - m_lastX;
    float yoffset = m_lastY - ypos; // Reversed since y-coordinates go from bottom to top

    m_lastX = xpos;
    m_lastY = ypos;

    m_camera.processMouseMovement(xoffset, yoffset);
}

void GameClientInput::handleKeyInput(int key, int action, int mods) {
    (void)mods; // Suppress unused parameter warning
    if (key >= 0 && key < 1024) {
        m_keys[key] = (action == GLFW_PRESS || action == GLFW_REPEAT);
    }
}

void GameClientInput::handleMouseButton(int button, int action, int mods) {
    (void)mods; // Suppress unused parameter warning

    std::cout << "Mouse button event: button=" << button << ", action=" << action << std::endl;

    // Don't handle mouse input if ImGui wants it
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) {
        std::cout << "ImGui wants mouse input, ignoring game input" << std::endl;
        return;
    }

    // Only handle input in game state
    if (m_gameClient.getGameState() != GameState::IN_GAME) {
        std::cout << "Not in game state, ignoring mouse input" << std::endl;
        return;
    }

    // Only handle block interaction if cursor is captured
    if (!m_mouseCaptured) {
        std::cout << "Cursor not captured, ignoring block interaction" << std::endl;
        return;
    }

    if (action == GLFW_PRESS) {
        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            std::cout << "Left mouse button pressed - breaking block" << std::endl;
            handleBlockInteraction(true); // Break block
        } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
            std::cout << "Right mouse button pressed - placing block" << std::endl;
            handleBlockInteraction(false); // Place block
        }
    } else if (action == GLFW_RELEASE) {
        std::cout << "Mouse button released (no action)" << std::endl;
    }
}

void GameClientInput::setMouseCaptured(bool captured) {
    m_mouseCaptured = captured;
    glfwSetInputMode(m_window, GLFW_CURSOR, m_mouseCaptured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    m_firstMouse = true; // Reset mouse to prevent jump
}

void GameClientInput::handleBlockInteraction(bool isBreaking) {
    if (m_gameClient.getGameState() != GameState::IN_GAME) return;

    // Create ray from camera for raycasting
    const auto& camera = m_gameClient.getCamera();
    Ray ray(camera.getPosition(), camera.getFront());

    // Raycast to find target block
    auto chunkManager = m_gameClient.getChunkManager();
    if (!chunkManager) return;

    BlockHitResult hit = m_gameClient.raycastVoxelsClient(ray, chunkManager, 5.0f); // 5 block reach

    if (hit.hit) {
        if (isBreaking) {
            // Break the block that was hit
            m_gameClient.onBlockBreak(hit.blockPosition);
        } else {
            // Place block adjacent to the hit face
            glm::ivec3 placePosition = hit.blockPosition + glm::ivec3(hit.normal);
            BlockType selectedBlock = m_gameClient.getHotbarBlocks()[m_gameClient.getSelectedHotbarSlot()];
            m_gameClient.onBlockPlace(placePosition, selectedBlock);
        }
    }
}

// Static GLFW callbacks
void GameClientInput::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    (void)scancode; // Suppress unused parameter warning
    auto gameClient = reinterpret_cast<GameClient*>(glfwGetWindowUserPointer(window));
    if (gameClient && gameClient->getInput()) {
        gameClient->getInput()->handleKeyInput(key, action, mods);
    }
}

void GameClientInput::mouseCallback(GLFWwindow* window, double xpos, double ypos) {
    auto gameClient = reinterpret_cast<GameClient*>(glfwGetWindowUserPointer(window));
    if (gameClient && gameClient->getInput()) {
        gameClient->getInput()->handleMouseInput(xpos, ypos);
    }
}

void GameClientInput::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    auto gameClient = reinterpret_cast<GameClient*>(glfwGetWindowUserPointer(window));
    if (gameClient && gameClient->getInput()) {
        gameClient->getInput()->handleMouseButton(button, action, mods);
    }
}

void GameClientInput::scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    (void)xoffset; (void)yoffset; // Suppress unused parameter warnings
    auto gameClient = reinterpret_cast<GameClient*>(glfwGetWindowUserPointer(window));
    (void)gameClient; // Suppress unused variable warning
    // Handle scroll if needed in the future
}