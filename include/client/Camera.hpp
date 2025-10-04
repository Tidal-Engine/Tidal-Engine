#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace engine {

/**
 * @brief First-person camera with configurable movement and look
 *
 * Handles view and projection matrices for 3D rendering.
 * Supports WASD movement and mouse look.
 */
class Camera {
public:
    /**
     * @brief Construct a camera with position and look direction
     * @param position Initial camera position
     * @param worldUp World up vector (default: +Y)
     * @param yaw Initial yaw angle in degrees (default: -90, looking along +Z)
     * @param pitch Initial pitch angle in degrees (default: 0, horizontal)
     */
    Camera(const glm::vec3& position = glm::vec3(0.0f, 0.0f, 3.0f),
           const glm::vec3& worldUp = glm::vec3(0.0f, 1.0f, 0.0f),
           float yaw = -90.0f,
           float pitch = 0.0f);

    /**
     * @brief Get the view matrix for rendering
     * @return View matrix
     */
    glm::mat4 getViewMatrix() const;

    /**
     * @brief Get the projection matrix
     * @param aspectRatio Window aspect ratio (width / height)
     * @param fov Field of view in degrees
     * @param nearPlane Near clipping plane
     * @param farPlane Far clipping plane
     * @return Projection matrix
     */
    glm::mat4 getProjectionMatrix(float aspectRatio, float fov, float nearPlane, float farPlane) const;

    /**
     * @brief Process keyboard input for movement
     * @param forward Move forward (WASD: W)
     * @param backward Move backward (WASD: S)
     * @param left Strafe left (WASD: A)
     * @param right Strafe right (WASD: D)
     * @param up Move up (Space)
     * @param down Move down (Shift)
     * @param deltaTime Time since last frame
     * @param speed Movement speed
     */
    void processMovement(bool forward, bool backward, bool left, bool right,
                        bool up, bool down, float deltaTime, float speed);  // NOLINT(readability-identifier-length)

    /**
     * @brief Process mouse input for looking around
     * @param xOffset Mouse X movement
     * @param yOffset Mouse Y movement
     * @param sensitivity Mouse sensitivity multiplier
     * @param constrainPitch Prevent camera flipping (default: true)
     */
    void processMouseMovement(float xOffset, float yOffset, float sensitivity, bool constrainPitch = true);

    /**
     * @brief Get camera position
     * @return Position vector
     */
    glm::vec3 getPosition() const { return position; }

    /**
     * @brief Get camera front direction
     * @return Front direction vector
     */
    glm::vec3 getFront() const { return front; }

    /**
     * @brief Set camera position
     * @param pos New position
     */
    void setPosition(const glm::vec3& pos) { position = pos; }

    /**
     * @brief Get camera yaw angle
     * @return Yaw angle in degrees
     */
    float getYaw() const { return yaw; }

    /**
     * @brief Get camera pitch angle
     * @return Pitch angle in degrees
     */
    float getPitch() const { return pitch; }

    /**
     * @brief Set camera yaw angle
     * @param newYaw Yaw angle in degrees
     */
    void setYaw(float newYaw) {
        yaw = newYaw;
        updateCameraVectors();
    }

    /**
     * @brief Set camera pitch angle
     * @param newPitch Pitch angle in degrees
     */
    void setPitch(float newPitch) {
        pitch = newPitch;
        updateCameraVectors();
    }

private:
    // Camera attributes
    glm::vec3 position;
    glm::vec3 front;
    glm::vec3 up;
    glm::vec3 right;
    glm::vec3 worldUp;

    // Euler angles
    float yaw;
    float pitch;

    /**
     * @brief Update camera vectors based on current yaw and pitch
     */
    void updateCameraVectors();
};

} // namespace engine
