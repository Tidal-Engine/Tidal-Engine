/**
 * @file GameClientInput.h
 * @brief Input handling system for keyboard, mouse, and game interactions
 * @author Tidal Engine Team
 * @version 1.0
 */

#pragma once

#include "core/Camera.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

// Forward declarations
class GameClient;
class ClientChunkManager;


/**
 * @brief Input processing and handling system for the game client
 *
 * GameClientInput manages all user input interactions:
 * - Keyboard input for movement, actions, and UI controls
 * - Mouse input for camera rotation and block interactions
 * - GLFW callback integration and event processing
 * - Camera movement and transformation updates
 * - Block placement and destruction mechanics
 * - Input state management and configuration
 *
 * Features:
 * - Real-time camera controls with configurable sensitivity
 * - Mouse capture for immersive gameplay
 * - Ray casting for precise block interaction
 * - Hotbar selection and block type switching
 * - Debug mode toggles and wireframe controls
 *
 * @see Camera for camera transformation system
 * @see GameClient for main client coordination
 * @see ClientChunkManager for world interaction
 */
class GameClientInput {
public:
    /**
     * @brief Construct input system with required dependencies
     * @param gameClient Reference to main game client
     * @param window GLFW window handle for input context
     * @param camera Reference to camera for movement updates
     */
    GameClientInput(GameClient& gameClient, GLFWwindow* window, Camera& camera);
    /**
     * @brief Default destructor
     */
    ~GameClientInput() = default;

    /// @name Input Processing
    /// @{
    /**
     * @brief Process continuous input (movement keys) per frame
     * @param deltaTime Time since last frame for frame-rate independent movement
     */
    void processInput(float deltaTime);

    /**
     * @brief Handle mouse cursor movement for camera rotation
     * @param xpos Current mouse X position
     * @param ypos Current mouse Y position
     */
    void handleMouseInput(double xpos, double ypos);

    /**
     * @brief Handle discrete keyboard input events
     * @param key GLFW key code
     * @param action GLFW action (press/release/repeat)
     * @param mods Modifier key flags
     */
    void handleKeyInput(int key, int action, int mods);

    /**
     * @brief Handle mouse button press/release events
     * @param button GLFW mouse button code
     * @param action GLFW action (press/release)
     * @param mods Modifier key flags
     */
    void handleMouseButton(int button, int action, int mods);
    /// @}

    /// @name Input State Management
    /// @{
    /**
     * @brief Set mouse capture state for camera control
     * @param captured True to capture mouse for camera rotation
     */
    void setMouseCaptured(bool captured);

    /**
     * @brief Check if mouse is captured for camera control
     * @return True if mouse is captured
     */
    bool isMouseCaptured() const { return m_mouseCaptured; }

    /**
     * @brief Set first mouse movement flag to prevent camera jump
     * @param firstMouse True if this is the first mouse movement
     */
    void setFirstMouse(bool firstMouse) { m_firstMouse = firstMouse; }
    /// @}

    /// @name Configuration
    /// @{
    /**
     * @brief Set mouse look sensitivity multiplier
     * @param sensitivity Sensitivity value (higher = more sensitive)
     */
    void setMouseSensitivity(float sensitivity) { m_mouseSensitivity = sensitivity; }

    /**
     * @brief Set player movement speed
     * @param speed Movement speed in units per second
     */
    void setMovementSpeed(float speed) { m_movementSpeed = speed; }
    /// @}

    /// @name Static GLFW Callbacks
    /// @{
    /**
     * @brief GLFW keyboard input callback
     * @param window GLFW window handle
     * @param key Key code
     * @param scancode Platform-specific scancode
     * @param action Key action (press/release/repeat)
     * @param mods Modifier key flags
     */
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

    /**
     * @brief GLFW mouse movement callback
     * @param window GLFW window handle
     * @param xpos Mouse X position
     * @param ypos Mouse Y position
     */
    static void mouseCallback(GLFWwindow* window, double xpos, double ypos);

    /**
     * @brief GLFW mouse button callback
     * @param window GLFW window handle
     * @param button Mouse button code
     * @param action Button action (press/release)
     * @param mods Modifier key flags
     */
    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);

    /**
     * @brief GLFW scroll wheel callback
     * @param window GLFW window handle
     * @param xoffset Horizontal scroll offset
     * @param yoffset Vertical scroll offset
     */
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
    /// @}

private:
    GameClient& m_gameClient;   ///< Reference to main game client
    GLFWwindow* m_window;       ///< GLFW window handle
    Camera& m_camera;           ///< Reference to camera for movement

    /// @name Input State
    /// @{
    bool m_keys[1024] = {};         ///< Keyboard key state array
    bool m_mouseCaptured = true;    ///< Mouse capture state for camera
    float m_lastX = 0.0f;           ///< Last mouse X position
    float m_lastY = 0.0f;           ///< Last mouse Y position
    bool m_firstMouse = true;       ///< First mouse movement flag
    /// @}

    /// @name Configuration Settings
    /// @{
    float m_mouseSensitivity = 0.1f;    ///< Mouse look sensitivity multiplier
    float m_movementSpeed = 2.5f;       ///< Player movement speed
    /// @}

    /// @name Private Implementation
    /// @{
    /**
     * @brief Update camera transforms based on input state
     * @param deltaTime Time since last frame for smooth movement
     */
    void updateTransforms(float deltaTime);

    /**
     * @brief Handle block placement or destruction interaction
     * @param isBreaking True for block breaking, false for placement
     */
    void handleBlockInteraction(bool isBreaking);
    /// @}
};