#pragma once

#include "core/Camera.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

// Forward declarations
class GameClient;
class ClientChunkManager;


class GameClientInput {
public:
    GameClientInput(GameClient& gameClient, GLFWwindow* window, Camera& camera);
    ~GameClientInput() = default;

    // Input processing
    void processInput(float deltaTime);
    void handleMouseInput(double xpos, double ypos);
    void handleKeyInput(int key, int action, int mods);
    void handleMouseButton(int button, int action, int mods);

    // Input state
    void setMouseCaptured(bool captured);
    bool isMouseCaptured() const { return m_mouseCaptured; }
    void setFirstMouse(bool firstMouse) { m_firstMouse = firstMouse; }

    // Configuration
    void setMouseSensitivity(float sensitivity) { m_mouseSensitivity = sensitivity; }
    void setMovementSpeed(float speed) { m_movementSpeed = speed; }

    // Static callbacks (GLFW requires static functions)
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void mouseCallback(GLFWwindow* window, double xpos, double ypos);
    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);

private:
    GameClient& m_gameClient;
    GLFWwindow* m_window;
    Camera& m_camera;

    // Input state
    bool m_keys[1024] = {};
    bool m_mouseCaptured = true;
    float m_lastX = 0.0f;
    float m_lastY = 0.0f;
    bool m_firstMouse = true;

    // Configuration
    float m_mouseSensitivity = 0.1f;
    float m_movementSpeed = 2.5f;

    // Helper methods
    void updateTransforms(float deltaTime);
    void handleBlockInteraction(bool isBreaking);
};