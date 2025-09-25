#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <array>

enum class CameraMovement {
    FORWARD,
    BACKWARD,
    LEFT,
    RIGHT,
    UP,
    DOWN
};

// Default camera values
const float YAW = -90.0f;
const float PITCH = 0.0f;
const float SPEED = 2.5f;
const float SENSITIVITY = 0.1f;
const float ZOOM = 90.0f;

// Plane for frustum culling
struct Plane {
    glm::vec3 normal;
    float distance;

    float distanceToPoint(const glm::vec3& point) const {
        return glm::dot(normal, point) + distance;
    }
};

// Frustum defined by 6 planes
struct Frustum {
    std::array<Plane, 6> planes;

    enum PlaneIndex {
        LEFT = 0, RIGHT, BOTTOM, TOP, NEAR, FAR
    };
};

// Axis-Aligned Bounding Box
struct AABB {
    glm::vec3 min;
    glm::vec3 max;

    AABB() : min(0.0f), max(0.0f) {}
    AABB(const glm::vec3& minPoint, const glm::vec3& maxPoint) : min(minPoint), max(maxPoint) {}
};

// Ray for ray casting
struct Ray {
    glm::vec3 origin;
    glm::vec3 direction;

    Ray(const glm::vec3& orig, const glm::vec3& dir) : origin(orig), direction(glm::normalize(dir)) {}
};

// Ray-block intersection result
struct BlockHitResult {
    bool hit;
    glm::ivec3 blockPosition;
    glm::ivec3 normal;
    float distance;
    glm::vec3 hitPoint;

    BlockHitResult() : hit(false), blockPosition(0), normal(0), distance(0.0f), hitPoint(0.0f) {}
};

class Camera {
public:
    // Camera Attributes
    glm::vec3 Position;
    glm::vec3 Front;
    glm::vec3 Up;
    glm::vec3 Right;
    glm::vec3 WorldUp;

    // Euler Angles
    float Yaw;
    float Pitch;

    // Camera options
    float MovementSpeed;
    float MouseSensitivity;
    float Zoom;

    // Constructor with vectors
    Camera(glm::vec3 position = glm::vec3(0.0f, 18.0f, 0.0f), glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f), float yaw = YAW, float pitch = PITCH);

    // Constructor with scalar values
    Camera(float posX, float posY, float posZ, float upX, float upY, float upZ, float yaw, float pitch);

    // Returns the view matrix calculated using Euler Angles and the LookAt Matrix
    glm::mat4 getViewMatrix() const;

    // Processes input received from any keyboard-like input system
    void processKeyboard(CameraMovement direction, float deltaTime);

    // Processes input received from a mouse input system
    void processMouseMovement(float xoffset, float yoffset, bool constrainPitch = true);

    // Processes input received from a mouse scroll-wheel event
    void processMouseScroll(float yoffset);

    // Getters
    glm::vec3 getPosition() const { return Position; }
    glm::vec3 getFront() const { return Front; }
    float getZoom() const { return Zoom; }

    // Frustum culling methods
    Frustum calculateFrustum(const glm::mat4& projectionMatrix, const glm::mat4& viewMatrix) const;
    bool isAABBInFrustum(const AABB& aabb, const Frustum& frustum) const;

    // Ray casting methods
    Ray getRay() const;
    static BlockHitResult raycastVoxels(const Ray& ray, class ChunkManager* chunkManager, float maxDistance = 10.0f);

    // Calculates the front vector from the Camera's (updated) Euler Angles
    void updateCameraVectors();

private:
};