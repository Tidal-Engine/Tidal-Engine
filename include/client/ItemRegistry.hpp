#pragma once

#include "shared/Item.hpp"
#include "shared/Block.hpp"
#include <string>
#include <unordered_map>
#include <vector>

namespace engine {

/**
 * @brief Properties and metadata for an item type
 */
struct ItemProperties {
    ItemType type;
    std::string name;           // Internal name (e.g., "stone")
    std::string displayName;    // Display name (e.g., "Stone")
    bool isBlock;               // Can this item be placed as a block?
    BlockType blockType;        // If isBlock, what block does it place?
    uint16_t maxStackSize;      // Maximum stack size (usually 64)

    // Future: texture icon path, rarity, etc.
};

/**
 * @brief Central registry for all item types in the game
 *
 * Singleton that stores properties for every item. Automatically registers
 * all vanilla blocks as placeable items during initialization.
 */
class ItemRegistry {
public:
    /**
     * @brief Get the singleton instance
     */
    static ItemRegistry& instance();

    /**
     * @brief Register an item with its properties
     */
    void registerItem(const ItemProperties& props);

    /**
     * @brief Get properties for an item type
     * @return Pointer to properties, or nullptr if not found
     */
    const ItemProperties* getItem(ItemType type) const;

    /**
     * @brief Get all registered item types
     */
    std::vector<ItemType> getAllItems() const;

    /**
     * @brief Get only block items (can be placed)
     */
    std::vector<ItemType> getAllBlocks() const;

    /**
     * @brief Register all vanilla items/blocks
     * Called automatically on first use
     */
    void registerVanillaItems();

private:
    ItemRegistry();
    ~ItemRegistry() = default;

    // Delete copy/move
    ItemRegistry(const ItemRegistry&) = delete;
    ItemRegistry& operator=(const ItemRegistry&) = delete;

    std::unordered_map<ItemType, ItemProperties> items;
    bool initialized = false;
};

} // namespace engine
