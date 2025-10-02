#include "core/Camera.hpp"

#include <algorithm>
#include <cmath>

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

glm::mat4 Camera::getProjectionMatrix(float aspectRatio, float fov, float nearPlane, float farPlane) const {  // NOLINT(readability-convert-member-functions-to-static)
    glm::mat4 proj = glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane);
    proj[1][1] *= -1; // Flip Y for Vulkan coordinate system
    return proj;
}

void Camera::processMovement(bool forward, bool backward, bool left, bool right,
                             bool up, bool down, float deltaTime, float speed) {  // NOLINT(readability-identifier-length)
    float velocity = speed * deltaTime;

    if (forward) {
        position += front * velocity;
    }
    if (backward) {
        position -= front * velocity;
    }
    if (left) {
        position -= this->right * velocity;
    }
    if (right) {
        position += this->right * velocity;
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
        pitch = std::min(pitch, 89.0f);
        pitch = std::max(pitch, -89.0f);
    }

    updateCameraVectors();
}

void Camera::updateCameraVectors() {
    // Calculate new front vector
    glm::vec3 newFront;
    newFront.x = std::cos(glm::radians(yaw)) * std::cos(glm::radians(pitch));  // NOLINT(cppcoreguidelines-pro-type-union-access)
    newFront.y = std::sin(glm::radians(pitch));  // NOLINT(cppcoreguidelines-pro-type-union-access)
    newFront.z = std::sin(glm::radians(yaw)) * std::cos(glm::radians(pitch));  // NOLINT(cppcoreguidelines-pro-type-union-access)
    front = glm::normalize(newFront);

    // Recalculate right and up vectors
    right = glm::normalize(glm::cross(front, worldUp));
    up = glm::normalize(glm::cross(right, front));
}

} // namespace engine
