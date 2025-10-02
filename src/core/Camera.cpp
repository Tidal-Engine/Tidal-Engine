#include "core/Camera.hpp"

namespace engine {

Camera::Camera(const glm::vec3& position, const glm::vec3& worldUp, float yaw, float pitch)
    : position(position), worldUp(worldUp), yaw(yaw), pitch(pitch),
      front(glm::vec3(0.0f, 0.0f, -1.0f)), up(glm::vec3(0.0f, 1.0f, 0.0f)),
      right(glm::vec3(1.0f, 0.0f, 0.0f)) {
    updateCameraVectors();
}

glm::mat4 Camera::getViewMatrix() const {
    return glm::lookAt(position, position + front, up);
}

glm::mat4 Camera::getProjectionMatrix(float aspectRatio, float fov, float nearPlane, float farPlane) const {
    glm::mat4 proj = glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane);
    proj[1][1] *= -1; // Flip Y for Vulkan coordinate system
    return proj;
}

void Camera::processMovement(bool forward, bool backward, bool left, bool right,
                             bool up, bool down, float deltaTime, float speed) {
    float velocity = speed * deltaTime;

    if (forward) {
        position += front * velocity;
    }
    if (backward) {
        position -= front * velocity;
    }
    if (left) {
        position -= right * velocity;
    }
    if (right) {
        position += right * velocity;
    }
    if (up) {
        position += worldUp * velocity;
    }
    if (down) {
        position -= worldUp * velocity;
    }
}

void Camera::processMouseMovement(float xOffset, float yOffset, float sensitivity, bool constrainPitch) {
    xOffset *= sensitivity;
    yOffset *= sensitivity;

    yaw += xOffset;
    pitch += yOffset;

    // Constrain pitch to prevent screen flipping
    if (constrainPitch) {
        if (pitch > 89.0f) {
            pitch = 89.0f;
        }
        if (pitch < -89.0f) {
            pitch = -89.0f;
        }
    }

    updateCameraVectors();
}

void Camera::updateCameraVectors() {
    // Calculate new front vector
    glm::vec3 newFront;
    newFront.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    newFront.y = sin(glm::radians(pitch));
    newFront.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    front = glm::normalize(newFront);

    // Recalculate right and up vectors
    right = glm::normalize(glm::cross(front, worldUp));
    up = glm::normalize(glm::cross(right, front));
}

} // namespace engine
