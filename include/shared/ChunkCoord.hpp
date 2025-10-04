#pragma once

#include <cstdint>
#include <functional>
#include <glm/glm.hpp>

namespace engine {

/**
 * @brief Chunk coordinate in world space
 *
 * Represents the position of a chunk in the world. Each chunk is 32x32x32 blocks.
 * This is used as a key in hash maps to store/retrieve chunks.
 */
struct ChunkCoord {
    int32_t x;
    int32_t y;
    int32_t z;

    ChunkCoord() : x(0), y(0), z(0) {}
    ChunkCoord(int32_t x, int32_t y, int32_t z) : x(x), y(y), z(z) {}  // NOLINT(readability-identifier-length)

    /**
     * @brief Convert world position to chunk coordinate
     * @param worldPos Position in world space
     * @return Chunk coordinate containing that position
     */
    static ChunkCoord fromWorldPos(const glm::vec3& worldPos) {
        // Integer division to get chunk coordinates
        // Note: Handles negative coordinates correctly with floor division
        return ChunkCoord(
            static_cast<int32_t>(std::floor(worldPos.x / 32.0f)),  // NOLINT(cppcoreguidelines-pro-type-union-access)
            static_cast<int32_t>(std::floor(worldPos.y / 32.0f)),  // NOLINT(cppcoreguidelines-pro-type-union-access)
            static_cast<int32_t>(std::floor(worldPos.z / 32.0f))  // NOLINT(cppcoreguidelines-pro-type-union-access)
        );
    }

    /**
     * @brief Get world position of chunk origin (0,0,0 block of chunk)
     */
    glm::vec3 toWorldPos() const {
        return glm::vec3(static_cast<float>(x) * 32.0f, static_cast<float>(y) * 32.0f, static_cast<float>(z) * 32.0f);  // NOLINT(cppcoreguidelines-narrowing-conversions)
    }

    // Comparison operators for use in maps/sets
    bool operator==(const ChunkCoord& other) const {
        return x == other.x && y == other.y && z == other.z;
    }

    bool operator!=(const ChunkCoord& other) const {
        return !(*this == other);
    }

    // For ordered containers (std::map)
    bool operator<(const ChunkCoord& other) const {
        if (x != other.x) { return x < other.x; }
        if (y != other.y) { return y < other.y; }
        return z < other.z;
    }
};

} // namespace engine

// Hash function for ChunkCoord to use in unordered_map
namespace std {
    template<>
    struct hash<engine::ChunkCoord> {
        size_t operator()(const engine::ChunkCoord& coord) const {
            // Combine hashes using a simple mixing function
            size_t hash1 = std::hash<int32_t>{}(coord.x);  // NOLINT(readability-identifier-length)
            size_t hash2 = std::hash<int32_t>{}(coord.y);  // NOLINT(readability-identifier-length)
            size_t hash3 = std::hash<int32_t>{}(coord.z);  // NOLINT(readability-identifier-length)

            // Mix the hashes together
            return hash1 ^ (hash2 << 1) ^ (hash3 << 2);
        }
    };
}
