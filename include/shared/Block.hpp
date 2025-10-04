#pragma once

#include <cstdint>

namespace engine {

/**
 * @brief Block type identifiers
 */
enum class BlockType : uint16_t {
    Air = 0,
    Stone = 1,
    Dirt = 2,
    GrassSide = 3,      // For grass block sides
    GrassTop = 4,       // For grass block top
    Cobblestone = 5,
    Wood = 6,
    Sand = 7,
    Brick = 8,
    Snow = 9,
    Grass = 10,         // Special: uses GrassSide, GrassTop, and Dirt

    Count // Always keep this last - gives us total count
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
