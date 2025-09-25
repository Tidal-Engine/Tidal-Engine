#include "core/Camera.h"
#include "game/ChunkManager.h"

Camera::Camera(glm::vec3 position, glm::vec3 up, float yaw, float pitch)
    : Front(glm::vec3(0.0f, 0.0f, -1.0f)), MovementSpeed(SPEED), MouseSensitivity(SENSITIVITY), Zoom(ZOOM) {
    Position = position;
    WorldUp = up;
    Yaw = yaw;
    Pitch = pitch;
    updateCameraVectors();
}

Camera::Camera(float posX, float posY, float posZ, float upX, float upY, float upZ, float yaw, float pitch)
    : Front(glm::vec3(0.0f, 0.0f, -1.0f)), MovementSpeed(SPEED), MouseSensitivity(SENSITIVITY), Zoom(ZOOM) {
    Position = glm::vec3(posX, posY, posZ);
    WorldUp = glm::vec3(upX, upY, upZ);
    Yaw = yaw;
    Pitch = pitch;
    updateCameraVectors();
}

glm::mat4 Camera::getViewMatrix() const {
    return glm::lookAt(Position, Position + Front, Up);
}

void Camera::processKeyboard(CameraMovement direction, float deltaTime) {
    float velocity = MovementSpeed * deltaTime;

    // Create horizontal-only movement vectors (Minecraft-style)
    // Front projected onto XZ plane (no Y component)
    glm::vec3 horizontalFront = glm::normalize(glm::vec3(Front.x, 0.0f, Front.z));
    // Right is already horizontal, but ensure no Y component
    glm::vec3 horizontalRight = glm::normalize(glm::vec3(Right.x, 0.0f, Right.z));

    if (direction == CameraMovement::FORWARD)
        Position += horizontalFront * velocity;
    if (direction == CameraMovement::BACKWARD)
        Position -= horizontalFront * velocity;
    if (direction == CameraMovement::LEFT)
        Position -= horizontalRight * velocity;
    if (direction == CameraMovement::RIGHT)
        Position += horizontalRight * velocity;
    if (direction == CameraMovement::UP)
        Position += glm::vec3(0.0f, 1.0f, 0.0f) * velocity; // Pure Y-axis up
    if (direction == CameraMovement::DOWN)
        Position -= glm::vec3(0.0f, 1.0f, 0.0f) * velocity; // Pure Y-axis down
}

void Camera::processMouseMovement(float xoffset, float yoffset, bool constrainPitch) {
    xoffset *= MouseSensitivity;
    yoffset *= MouseSensitivity;

    Yaw += xoffset;
    Pitch += yoffset;

    // Wrap yaw to keep it between 0 and 360 degrees
    if (Yaw >= 360.0f) {
        Yaw -= 360.0f;
    } else if (Yaw < 0.0f) {
        Yaw += 360.0f;
    }

    // Make sure that when pitch is out of bounds, screen doesn't get flipped
    if (constrainPitch) {
        if (Pitch > 89.0f)
            Pitch = 89.0f;
        if (Pitch < -89.0f)
            Pitch = -89.0f;
    }

    // Update Front, Right and Up Vectors using the updated Euler angles
    updateCameraVectors();
}

void Camera::processMouseScroll(float yoffset) {
    Zoom -= yoffset;
    if (Zoom < 1.0f)
        Zoom = 1.0f;
    if (Zoom > 90.0f)
        Zoom = 90.0f;
}

void Camera::updateCameraVectors() {
    // Calculate the new Front vector
    glm::vec3 front;
    front.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
    front.y = sin(glm::radians(Pitch));
    front.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
    Front = glm::normalize(front);
    // Also re-calculate the Right and Up vector
    Right = glm::normalize(glm::cross(Front, WorldUp));  // Normalize the vectors, because their length gets closer to 0 the more you look up or down which results in slower movement.
    Up = glm::normalize(glm::cross(Right, Front));
}

Frustum Camera::calculateFrustum(const glm::mat4& projectionMatrix, const glm::mat4& viewMatrix) const {
    Frustum frustum;

    // Calculate combined matrix (projection * view)
    glm::mat4 clip = projectionMatrix * viewMatrix;

    // Extract frustum planes from the combined matrix
    // Left plane
    frustum.planes[Frustum::LEFT].normal.x = clip[0][3] + clip[0][0];
    frustum.planes[Frustum::LEFT].normal.y = clip[1][3] + clip[1][0];
    frustum.planes[Frustum::LEFT].normal.z = clip[2][3] + clip[2][0];
    frustum.planes[Frustum::LEFT].distance = clip[3][3] + clip[3][0];

    // Right plane
    frustum.planes[Frustum::RIGHT].normal.x = clip[0][3] - clip[0][0];
    frustum.planes[Frustum::RIGHT].normal.y = clip[1][3] - clip[1][0];
    frustum.planes[Frustum::RIGHT].normal.z = clip[2][3] - clip[2][0];
    frustum.planes[Frustum::RIGHT].distance = clip[3][3] - clip[3][0];

    // Bottom plane
    frustum.planes[Frustum::BOTTOM].normal.x = clip[0][3] + clip[0][1];
    frustum.planes[Frustum::BOTTOM].normal.y = clip[1][3] + clip[1][1];
    frustum.planes[Frustum::BOTTOM].normal.z = clip[2][3] + clip[2][1];
    frustum.planes[Frustum::BOTTOM].distance = clip[3][3] + clip[3][1];

    // Top plane
    frustum.planes[Frustum::TOP].normal.x = clip[0][3] - clip[0][1];
    frustum.planes[Frustum::TOP].normal.y = clip[1][3] - clip[1][1];
    frustum.planes[Frustum::TOP].normal.z = clip[2][3] - clip[2][1];
    frustum.planes[Frustum::TOP].distance = clip[3][3] - clip[3][1];

    // Near plane
    frustum.planes[Frustum::NEAR_PLANE].normal.x = clip[0][3] + clip[0][2];
    frustum.planes[Frustum::NEAR_PLANE].normal.y = clip[1][3] + clip[1][2];
    frustum.planes[Frustum::NEAR_PLANE].normal.z = clip[2][3] + clip[2][2];
    frustum.planes[Frustum::NEAR_PLANE].distance = clip[3][3] + clip[3][2];

    // Far plane
    frustum.planes[Frustum::FAR_PLANE].normal.x = clip[0][3] - clip[0][2];
    frustum.planes[Frustum::FAR_PLANE].normal.y = clip[1][3] - clip[1][2];
    frustum.planes[Frustum::FAR_PLANE].normal.z = clip[2][3] - clip[2][2];
    frustum.planes[Frustum::FAR_PLANE].distance = clip[3][3] - clip[3][2];

    // Normalize all planes
    for (int i = 0; i < 6; i++) {
        float length = glm::length(frustum.planes[i].normal);
        frustum.planes[i].normal /= length;
        frustum.planes[i].distance /= length;
    }

    return frustum;
}

bool Camera::isAABBInFrustum(const AABB& aabb, const Frustum& frustum) const {
    // For each plane, check if the AABB is completely on the outside
    for (const auto& plane : frustum.planes) {
        // Find the positive vertex (the vertex of the AABB that is furthest along the plane normal)
        glm::vec3 positiveVertex = aabb.min;
        if (plane.normal.x >= 0) positiveVertex.x = aabb.max.x;
        if (plane.normal.y >= 0) positiveVertex.y = aabb.max.y;
        if (plane.normal.z >= 0) positiveVertex.z = aabb.max.z;

        // If the positive vertex is on the negative side of the plane, the AABB is outside
        if (plane.distanceToPoint(positiveVertex) < 0) {
            return false;
        }
    }

    // AABB is either intersecting or inside the frustum
    return true;
}

Ray Camera::getRay() const {
    return Ray(Position, Front);
}

BlockHitResult Camera::raycastVoxels(const Ray& ray, ChunkManager* chunkManager, float maxDistance) {
    BlockHitResult result;

    if (!chunkManager) {
        return result;
    }

    // DDA (Digital Differential Analyzer) ray casting algorithm
    glm::vec3 currentPos = ray.origin;
    glm::vec3 rayStep = ray.direction * 0.1f; // Step size

    float distance = 0.0f;

    while (distance < maxDistance) {
        // Convert world position to voxel coordinates
        glm::ivec3 voxelPos = glm::ivec3(glm::floor(currentPos));

        // Check if this voxel is solid
        if (chunkManager->isVoxelSolidAtWorldPosition(voxelPos.x, voxelPos.y, voxelPos.z)) {
            result.hit = true;
            result.blockPosition = voxelPos;
            result.distance = distance;
            result.hitPoint = currentPos;

            // Calculate face normal by checking which face was hit
            glm::vec3 voxelCenter = glm::vec3(voxelPos) + glm::vec3(0.5f);
            glm::vec3 diff = currentPos - voxelCenter;

            // Find the axis with the largest absolute difference to determine face
            float absX = glm::abs(diff.x);
            float absY = glm::abs(diff.y);
            float absZ = glm::abs(diff.z);

            if (absX >= absY && absX >= absZ) {
                result.normal = glm::ivec3(diff.x > 0 ? 1 : -1, 0, 0);
            } else if (absY >= absX && absY >= absZ) {
                result.normal = glm::ivec3(0, diff.y > 0 ? 1 : -1, 0);
            } else {
                result.normal = glm::ivec3(0, 0, diff.z > 0 ? 1 : -1);
            }

            break;
        }

        currentPos += rayStep;
        distance += glm::length(rayStep);
    }

    return result;
}