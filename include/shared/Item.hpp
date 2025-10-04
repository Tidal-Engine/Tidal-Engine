#pragma once

#include "shared/Block.hpp"
#include <cstdint>

namespace engine {

/**
 * @brief Item type identifiers
 *
 * Items represent what can be in inventory slots. Some items (blocks) can be placed in the world.
 * The enum values for blocks match BlockType to simplify conversion.
 */
enum class ItemType : uint16_t {  // NOLINT(performance-enum-size, readability-enum-initial-value)
    // Special
    Empty = 0,  // NOLINT(readability-identifier-naming)

    // Blocks (match BlockType enum values)
    Stone = static_cast<uint16_t>(BlockType::Stone),  // NOLINT(readability-identifier-naming, readability-enum-initial-value)
    Dirt = static_cast<uint16_t>(BlockType::Dirt),  // NOLINT(readability-identifier-naming, readability-enum-initial-value)
    Cobblestone = static_cast<uint16_t>(BlockType::Cobblestone),  // NOLINT(readability-identifier-naming, readability-enum-initial-value)
    Wood = static_cast<uint16_t>(BlockType::Wood),  // NOLINT(readability-identifier-naming, readability-enum-initial-value)
    Sand = static_cast<uint16_t>(BlockType::Sand),  // NOLINT(readability-identifier-naming, readability-enum-initial-value)
    Brick = static_cast<uint16_t>(BlockType::Brick),  // NOLINT(readability-identifier-naming, readability-enum-initial-value)
    Snow = static_cast<uint16_t>(BlockType::Snow),  // NOLINT(readability-identifier-naming, readability-enum-initial-value)
    Grass = static_cast<uint16_t>(BlockType::Grass),  // NOLINT(readability-identifier-naming, readability-enum-initial-value)

    // Future: Tools, consumables, etc.
    // Sword = 1000,
    // Pickaxe = 1001,

    Count  // NOLINT(readability-identifier-naming)
};

/**
 * @brief A stack of items in an inventory slot
 *
 * Represents one or more items of the same type. An empty slot has type=Empty or count=0.
 */
struct ItemStack {
    ItemType type = ItemType::Empty;
    uint16_t count = 0;

    // Future: NBT data for enchantments, custom names, etc.
    // std::unique_ptr<NBTData> data;

    /**
     * @brief Check if this stack is empty
     */
    bool isEmpty() const {
        return type == ItemType::Empty || count == 0;
    }

    /**
     * @brief Check if this item can be placed as a block
     */
    bool isBlock() const {
        return static_cast<uint16_t>(type) >= static_cast<uint16_t>(BlockType::Stone) &&
               static_cast<uint16_t>(type) < static_cast<uint16_t>(BlockType::Count);
    }

    /**
     * @brief Convert this item to a block type (only valid if isBlock() returns true)
     */
    BlockType toBlockType() const {
        return static_cast<BlockType>(type);
    }

    /**
     * @brief Create an item stack from a block type
     */
    static ItemStack fromBlock(BlockType blockType, uint16_t count = 1) {
        ItemStack stack;
        stack.type = static_cast<ItemType>(blockType);
        stack.count = count;
        return stack;
    }
};

} // namespace engine
