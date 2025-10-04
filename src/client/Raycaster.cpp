#include "client/Raycaster.hpp"
#include "client/NetworkClient.hpp"
#include "shared/Chunk.hpp"
#include "shared/ChunkCoord.hpp"
#include "core/Logger.hpp"
#include <cmath>
#include <limits>
#include <unordered_set>

namespace engine {

std::optional<RaycastHit> Raycaster::cast(
    const glm::vec3& origin,
    const glm::vec3& direction,
    float maxDistance,
    const NetworkClient* client
) {
    if (!client) {
        return std::nullopt;
    }

    // Normalize direction
    glm::vec3 dir = glm::normalize(direction);

    // Current voxel position (floor of ray origin)
    glm::ivec3 voxel(
        static_cast<int>(std::floor(origin.x)),
        static_cast<int>(std::floor(origin.y)),
        static_cast<int>(std::floor(origin.z))
    );

    // Step direction for each axis (+1 or -1)
    glm::ivec3 step(
        dir.x > 0 ? 1 : (dir.x < 0 ? -1 : 0),
        dir.y > 0 ? 1 : (dir.y < 0 ? -1 : 0),
        dir.z > 0 ? 1 : (dir.z < 0 ? -1 : 0)
    );

    // tDelta: distance along ray to traverse one voxel on each axis
    glm::vec3 tDelta(
        std::abs(safeDivide(1.0f, dir.x)),
        std::abs(safeDivide(1.0f, dir.y)),
        std::abs(safeDivide(1.0f, dir.z))
    );

    // tMax: distance along ray to next voxel boundary on each axis
    glm::vec3 tMax;

    // X axis
    if (dir.x > 0) {
        tMax.x = (static_cast<float>(voxel.x + 1) - origin.x) / dir.x;
    } else if (dir.x < 0) {
        tMax.x = (static_cast<float>(voxel.x) - origin.x) / dir.x;
    } else {
        tMax.x = std::numeric_limits<float>::max();
    }

    // Y axis
    if (dir.y > 0) {
        tMax.y = (static_cast<float>(voxel.y + 1) - origin.y) / dir.y;
    } else if (dir.y < 0) {
        tMax.y = (static_cast<float>(voxel.y) - origin.y) / dir.y;
    } else {
        tMax.y = std::numeric_limits<float>::max();
    }

    // Z axis
    if (dir.z > 0) {
        tMax.z = (static_cast<float>(voxel.z + 1) - origin.z) / dir.z;
    } else if (dir.z < 0) {
        tMax.z = (static_cast<float>(voxel.z) - origin.z) / dir.z;
    } else {
        tMax.z = std::numeric_limits<float>::max();
    }

    // Track which face we hit
    glm::ivec3 normal(0, 0, 0);
    float distance = 0.0f;

    // DDA traversal
    while (distance < maxDistance) {
        // Check current voxel
        BlockType blockType = getBlockAt(voxel, client);
        if (blockType != BlockType::Air) {
            // Hit a solid block
            RaycastHit hit;
            hit.blockPos = voxel;
            hit.normal = normal;
            hit.distance = distance;
            hit.blockType = blockType;
            return hit;
        }

        // Step to next voxel (choose axis with smallest tMax)
        if (tMax.x < tMax.y) {
            if (tMax.x < tMax.z) {
                // Step in X direction
                voxel.x += step.x;
                distance = tMax.x;
                tMax.x += tDelta.x;
                normal = glm::ivec3(-step.x, 0, 0);
            } else {
                // Step in Z direction
                voxel.z += step.z;
                distance = tMax.z;
                tMax.z += tDelta.z;
                normal = glm::ivec3(0, 0, -step.z);
            }
        } else {
            if (tMax.y < tMax.z) {
                // Step in Y direction
                voxel.y += step.y;
                distance = tMax.y;
                tMax.y += tDelta.y;
                normal = glm::ivec3(0, -step.y, 0);
            } else {
                // Step in Z direction
                voxel.z += step.z;
                distance = tMax.z;
                tMax.z += tDelta.z;
                normal = glm::ivec3(0, 0, -step.z);
            }
        }
    }

    // No hit within max distance
    return std::nullopt;
}

BlockType Raycaster::getBlockAt(const glm::ivec3& pos, const NetworkClient* client) {
    // Calculate chunk coordinate using floor division
    ChunkCoord chunkCoord(
        pos.x < 0 ? ((pos.x + 1) / 32) - 1 : pos.x / 32,
        pos.y < 0 ? ((pos.y + 1) / 32) - 1 : pos.y / 32,
        pos.z < 0 ? ((pos.z + 1) / 32) - 1 : pos.z / 32
    );

    // Get chunk from client
    const Chunk* chunk = client->getChunk(chunkCoord);
    if (!chunk) {
        // Log missing chunks to help debug raycasting issues
        static std::unordered_set<ChunkCoord> loggedMissingChunks;
        if (loggedMissingChunks.find(chunkCoord) == loggedMissingChunks.end()) {
            LOG_TRACE("Raycast: chunk ({}, {}, {}) not loaded for block at ({}, {}, {})",
                     chunkCoord.x, chunkCoord.y, chunkCoord.z, pos.x, pos.y, pos.z);
            loggedMissingChunks.insert(chunkCoord);
        }
        return BlockType::Air;  // Unloaded chunks are treated as air
    }

    // Calculate local block position within chunk (proper modulo for negatives)
    int localX = pos.x - (chunkCoord.x * 32);
    int localY = pos.y - (chunkCoord.y * 32);
    int localZ = pos.z - (chunkCoord.z * 32);

    return chunk->getBlock(localX, localY, localZ).type;
}

float Raycaster::safeDivide(float numerator, float denominator) {
    const float epsilon = 1e-8f;
    if (std::abs(denominator) < epsilon) {
        return std::numeric_limits<float>::max();
    }
    return numerator / denominator;
}

} // namespace engine
