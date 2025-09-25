/**
 * @file Camera.h
 * @brief 3D camera system with frustum culling and raycasting
 * @author Tidal Engine Team
 * @version 1.0
 */

#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <array>

/**
 * @brief Camera movement directions for keyboard input
 */
enum class CameraMovement {
    FORWARD,    ///< Move camera forward
    BACKWARD,   ///< Move camera backward
    LEFT,       ///< Move camera left (strafe)
    RIGHT,      ///< Move camera right (strafe)
    UP,         ///< Move camera up
    DOWN        ///< Move camera down
};

/// @defgroup CameraDefaults Default camera values
/// @{
const float YAW = -90.0f;         ///< Default yaw angle in degrees
const float PITCH = 0.0f;         ///< Default pitch angle in degrees
const float SPEED = 2.5f;         ///< Default movement speed in units per second
const float SENSITIVITY = 0.1f;   ///< Default mouse sensitivity
const float ZOOM = 90.0f;         ///< Default field of view in degrees
/// @}

/**
 * @brief Geometric plane for frustum culling calculations
 */
struct Plane {
    glm::vec3 normal;    ///< Plane normal vector
    float distance;      ///< Distance from origin

    /**
     * @brief Calculate distance from point to plane
     * @param point Point to test
     * @return Signed distance to plane (negative means behind plane)
     */
    float distanceToPoint(const glm::vec3& point) const {
        return glm::dot(normal, point) + distance;
    }
};

/**
 * @brief Camera frustum defined by 6 planes for culling
 */
struct Frustum {
    std::array<Plane, 6> planes;    ///< The six frustum planes

    /**
     * @brief Plane indices for frustum array
     */
    enum PlaneIndex {
        LEFT = 0, RIGHT, BOTTOM, TOP, NEAR, FAR
    };
};

/**
 * @brief Axis-Aligned Bounding Box for collision detection
 */
struct AABB {
    glm::vec3 min;    ///< Minimum corner of the box
    glm::vec3 max;    ///< Maximum corner of the box

    /**
     * @brief Default constructor creates empty AABB at origin
     */
    AABB() : min(0.0f), max(0.0f) {}

    /**
     * @brief Construct AABB with min/max points
     * @param minPoint Minimum corner
     * @param maxPoint Maximum corner
     */
    AABB(const glm::vec3& minPoint, const glm::vec3& maxPoint) : min(minPoint), max(maxPoint) {}
};

/**
 * @brief Ray for raycasting operations
 */
struct Ray {
    glm::vec3 origin;       ///< Ray origin point
    glm::vec3 direction;    ///< Ray direction (normalized)

    /**
     * @brief Construct ray from origin and direction
     * @param orig Ray origin point
     * @param dir Ray direction (will be normalized)
     */
    Ray(const glm::vec3& orig, const glm::vec3& dir) : origin(orig), direction(glm::normalize(dir)) {}
};

/**
 * @brief Result of a ray-block intersection test
 */
struct BlockHitResult {
    bool hit;                    ///< Whether the ray hit a block
    glm::ivec3 blockPosition;    ///< World position of the hit block
    glm::ivec3 normal;           ///< Face normal of the hit
    float distance;              ///< Distance from ray origin to hit point
    glm::vec3 hitPoint;          ///< Exact hit point in world space

    /**
     * @brief Default constructor creates a miss result
     */
    BlockHitResult() : hit(false), blockPosition(0), normal(0), distance(0.0f), hitPoint(0.0f) {}
};

/**
 * @brief 3D camera with frustum culling and raycasting capabilities
 *
 * The Camera class provides a complete 3D camera system with support for:
 * - First-person camera controls (keyboard and mouse)
 * - View matrix generation for rendering
 * - Frustum culling for performance optimization
 * - Ray casting for object picking and collision detection
 *
 * @see CameraMovement for movement directions
 * @see Ray for raycasting operations
 * @see Frustum for culling operations
 */
class Camera {
public:
    /// @name Camera Transform
    /// @{
    glm::vec3 Position;     ///< Camera position in world space
    glm::vec3 Front;        ///< Camera forward direction vector
    glm::vec3 Up;           ///< Camera up direction vector
    glm::vec3 Right;        ///< Camera right direction vector
    glm::vec3 WorldUp;      ///< World up vector (usually 0,1,0)
    /// @}

    /// @name Camera Orientation
    /// @{
    float Yaw;              ///< Yaw angle in degrees (horizontal rotation)
    float Pitch;            ///< Pitch angle in degrees (vertical rotation)
    /// @}

    /// @name Camera Settings
    /// @{
    float MovementSpeed;    ///< Movement speed in units per second
    float MouseSensitivity; ///< Mouse sensitivity multiplier
    float Zoom;             ///< Field of view in degrees
    /// @}

    /**
     * @brief Construct camera with position and orientation vectors
     * @param position Initial camera position
     * @param up World up vector
     * @param yaw Initial yaw angle in degrees
     * @param pitch Initial pitch angle in degrees
     */
    Camera(glm::vec3 position = glm::vec3(0.0f, 18.0f, 0.0f),
           glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f),
           float yaw = YAW, float pitch = PITCH);

    /**
     * @brief Construct camera with scalar values
     * @param posX X position
     * @param posY Y position
     * @param posZ Z position
     * @param upX X component of up vector
     * @param upY Y component of up vector
     * @param upZ Z component of up vector
     * @param yaw Initial yaw angle
     * @param pitch Initial pitch angle
     */
    Camera(float posX, float posY, float posZ, float upX, float upY, float upZ, float yaw, float pitch);

    /**
     * @brief Generate view matrix for rendering
     * @return View matrix calculated using current position and orientation
     */
    glm::mat4 getViewMatrix() const;

    /**
     * @brief Process keyboard input for camera movement
     * @param direction Movement direction
     * @param deltaTime Time since last frame in seconds
     */
    void processKeyboard(CameraMovement direction, float deltaTime);

    /**
     * @brief Process mouse movement for camera rotation
     * @param xoffset Mouse X offset
     * @param yoffset Mouse Y offset
     * @param constrainPitch Whether to constrain pitch to [-89, 89] degrees
     */
    void processMouseMovement(float xoffset, float yoffset, bool constrainPitch = true);

    /**
     * @brief Process mouse scroll wheel for zoom
     * @param yoffset Scroll wheel offset
     */
    void processMouseScroll(float yoffset);

    /// @name Accessors
    /// @{
    glm::vec3 getPosition() const { return Position; }  ///< Get camera position
    glm::vec3 getFront() const { return Front; }        ///< Get camera front vector
    float getZoom() const { return Zoom; }              ///< Get field of view
    /// @}

    /// @name Frustum Culling
    /// @{
    /**
     * @brief Calculate camera frustum from projection and view matrices
     * @param projectionMatrix Camera projection matrix
     * @param viewMatrix Camera view matrix
     * @return Frustum structure with 6 planes
     */
    Frustum calculateFrustum(const glm::mat4& projectionMatrix, const glm::mat4& viewMatrix) const;

    /**
     * @brief Test if AABB is visible in frustum
     * @param aabb Bounding box to test
     * @param frustum Camera frustum
     * @return True if AABB is at least partially visible
     */
    bool isAABBInFrustum(const AABB& aabb, const Frustum& frustum) const;
    /// @}

    /// @name Raycasting
    /// @{
    /**
     * @brief Generate ray from camera position through screen center
     * @return Ray originating from camera position
     */
    Ray getRay() const;

    /**
     * @brief Cast ray through voxel world to find intersections
     * @param ray Ray to cast
     * @param chunkManager Chunk manager containing voxel data
     * @param maxDistance Maximum ray distance
     * @return Hit result with block information if hit
     */
    static BlockHitResult raycastVoxels(const Ray& ray, class ChunkManager* chunkManager, float maxDistance = 10.0f);
    /// @}

    /**
     * @brief Update camera vectors from current Euler angles
     * @note Called automatically by movement/rotation functions
     */
    void updateCameraVectors();

private:
};