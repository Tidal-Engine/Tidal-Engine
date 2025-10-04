#pragma once

#include <glm/glm.hpp>
#include <optional>
#include "shared/Block.hpp"

namespace engine {

// Forward declarations
class NetworkClient;

/**
 * @brief Result of a raycast operation
 */
struct RaycastHit {
    glm::ivec3 blockPos;      ///< Position of the hit block
    glm::ivec3 normal;        ///< Face normal of the hit (-1, 0, or 1 for each axis)
    float distance;           ///< Distance from ray origin to hit point
    BlockType blockType;      ///< Type of block that was hit
};

/**
 * @brief Voxel raycasting using DDA algorithm (Amanatides & Woo)
 *
 * Implements "A Fast Voxel Traversal Algorithm for Ray Tracing"
 * Efficiently traverses voxel grid to find ray-block intersections
 */
class Raycaster {
public:
    /**
     * @brief Cast a ray through the voxel world
     *
     * @param origin Ray origin in world space
     * @param direction Ray direction (should be normalized)
     * @param maxDistance Maximum ray distance
     * @param client Network client to query chunk data
     * @return RaycastHit if a solid block was hit, std::nullopt otherwise
     */
    static std::optional<RaycastHit> cast(
        const glm::vec3& origin,
        const glm::vec3& direction,
        float maxDistance,
        const NetworkClient* client
    );

private:
    /**
     * @brief Get block at world position from network client
     */
    static BlockType getBlockAt(const glm::ivec3& pos, const NetworkClient* client);

    /**
     * @brief Safely compute 1/x, returning a large value if x is near zero
     */
    static float safeDivide(float numerator, float denominator);
};

} // namespace engine
