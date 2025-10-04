#pragma once

#include "shared/Item.hpp"
#include <array>
#include <cstdint>
#include <cstddef>

namespace engine {

/**
 * @brief Player inventory manager
 *
 * Manages 36 inventory slots: 9 hotbar slots (always accessible) + 27 main inventory slots.
 * Hotbar slots are indices 0-8, main inventory is 9-35.
 */
class Inventory {
public:
    static constexpr size_t HOTBAR_SIZE = 9;
    static constexpr size_t MAIN_INVENTORY_SIZE = 27;
    static constexpr size_t TOTAL_SIZE = HOTBAR_SIZE + MAIN_INVENTORY_SIZE;  // 36

    Inventory();
    ~Inventory() = default;

    // Hotbar management
    /**
     * @brief Get the currently selected hotbar slot (0-8)
     */
    size_t getSelectedHotbarIndex() const { return selectedHotbarSlot; }

    /**
     * @brief Set the selected hotbar slot (0-8)
     */
    void setSelectedHotbarIndex(size_t index);

    /**
     * @brief Get the currently selected item stack
     */
    ItemStack& getSelectedSlot();
    const ItemStack& getSelectedSlot() const;

    /**
     * @brief Get a specific hotbar slot (0-8)
     */
    ItemStack& getHotbarSlot(size_t index);
    const ItemStack& getHotbarSlot(size_t index) const;

    // General inventory access
    /**
     * @brief Get any inventory slot by index (0-35)
     * @param index Slot index: 0-8 = hotbar, 9-35 = main inventory
     */
    ItemStack& getSlot(size_t index);
    const ItemStack& getSlot(size_t index) const;

    /**
     * @brief Set a slot to a specific item and count
     */
    void setSlot(size_t index, ItemType type, uint16_t count);

    /**
     * @brief Add items to inventory (finds first available slot or stacks)
     * @return true if all items were added, false if inventory full
     */
    bool addItem(ItemType type, uint16_t count = 1);

    /**
     * @brief Remove items from a specific slot
     * @return true if successful
     */
    bool removeItem(size_t slot, uint16_t count = 1);

    /**
     * @brief Clear all inventory slots
     */
    void clear();

private:
    std::array<ItemStack, TOTAL_SIZE> slots;
    size_t selectedHotbarSlot = 0;  // 0-8
};

} // namespace engine
