#pragma once

#include <cstdint>

namespace engine {

/**
 * @brief Block type identifiers
 */
enum class BlockType : uint16_t {  // NOLINT(performance-enum-size, readability-enum-initial-value)
    Air = 0,  // NOLINT(readability-identifier-naming)
    Stone = 1,  // NOLINT(readability-identifier-naming)
    Dirt = 2,  // NOLINT(readability-identifier-naming)
    GrassSide = 3,      // For grass block sides  // NOLINT(readability-identifier-naming)
    GrassTop = 4,       // For grass block top  // NOLINT(readability-identifier-naming)
    Cobblestone = 5,  // NOLINT(readability-identifier-naming)
    Wood = 6,  // NOLINT(readability-identifier-naming)
    Sand = 7,  // NOLINT(readability-identifier-naming)
    Brick = 8,  // NOLINT(readability-identifier-naming)
    Snow = 9,  // NOLINT(readability-identifier-naming)
    Grass = 10,         // Special: uses GrassSide, GrassTop, and Dirt  // NOLINT(readability-identifier-naming)

    Count  // Always keep this last - gives us total count  // NOLINT(readability-identifier-naming)
};

/**
 * @brief Bitmask constants for face culling optimization
 * Each bit represents whether a face in that direction should be culled
 */
constexpr uint8_t ADJACENT_BITMASK_NEG_X = 1 << 0;  // -X (left)
constexpr uint8_t ADJACENT_BITMASK_POS_X = 1 << 1;  // +X (right)
constexpr uint8_t ADJACENT_BITMASK_NEG_Y = 1 << 2;  // -Y (bottom)
constexpr uint8_t ADJACENT_BITMASK_POS_Y = 1 << 3;  // +Y (top)
constexpr uint8_t ADJACENT_BITMASK_NEG_Z = 1 << 4;  // -Z (back)
constexpr uint8_t ADJACENT_BITMASK_POS_Z = 1 << 5;  // +Z (front)
constexpr uint8_t ALL_ADJACENT_BITMASKS = 0x3F;     // All 6 faces culled

/**
 * @brief Block data structure
 *
 * Represents a single block in the world. Currently just stores type,
 * but can be extended to include metadata (facing direction, state, etc.)
 */
struct Block {
    BlockType type = BlockType::Air;

    // Future: Add metadata here (facing, state, etc.)
    // uint8_t metadata = 0;

    /**
     * @brief Check if block is solid (not air)
     */
    bool isSolid() const {
        return type != BlockType::Air;
    }

    /**
     * @brief Check if block is transparent (for rendering optimization)
     */
    bool isTransparent() const {
        return type == BlockType::Air;
    }
};

} // namespace engine
