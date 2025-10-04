#include "client/Inventory.hpp"
#include "core/Logger.hpp"
#include <algorithm>

namespace engine {

Inventory::Inventory() {
    clear();
}

void Inventory::setSelectedHotbarIndex(size_t index) {
    if (index >= HOTBAR_SIZE) {
        LOG_WARN("Invalid hotbar index: {} (max: {})", index, HOTBAR_SIZE - 1);
        return;
    }
    selectedHotbarSlot = index;
}

ItemStack& Inventory::getSelectedSlot() {
    return slots[selectedHotbarSlot];
}

const ItemStack& Inventory::getSelectedSlot() const {
    return slots[selectedHotbarSlot];
}

ItemStack& Inventory::getHotbarSlot(size_t index) {
    if (index >= HOTBAR_SIZE) {
        LOG_ERROR("Hotbar index out of range: {}", index);
        return slots[0];  // Fallback to first slot
    }
    return slots[index];
}

const ItemStack& Inventory::getHotbarSlot(size_t index) const {
    if (index >= HOTBAR_SIZE) {
        LOG_ERROR("Hotbar index out of range: {}", index);
        return slots[0];
    }
    return slots[index];
}

ItemStack& Inventory::getSlot(size_t index) {
    if (index >= TOTAL_SIZE) {
        LOG_ERROR("Inventory index out of range: {}", index);
        return slots[0];
    }
    return slots[index];
}

const ItemStack& Inventory::getSlot(size_t index) const {
    if (index >= TOTAL_SIZE) {
        LOG_ERROR("Inventory index out of range: {}", index);
        return slots[0];
    }
    return slots[index];
}

void Inventory::setSlot(size_t index, ItemType type, uint16_t count) {
    if (index >= TOTAL_SIZE) {
        LOG_ERROR("Inventory index out of range: {}", index);
        return;
    }

    slots[index].type = type;
    slots[index].count = count;
}

bool Inventory::addItem(ItemType type, uint16_t count) {
    if (type == ItemType::Empty || count == 0) {
        return true;
    }

    uint16_t remaining = count;

    // TODO: Stack with existing items (if stackable)
    // For now, just find first empty slot

    for (size_t i = 0; i < TOTAL_SIZE && remaining > 0; i++) {
        if (slots[i].isEmpty()) {
            slots[i].type = type;
            slots[i].count = remaining;
            remaining = 0;
            break;
        }
    }

    return remaining == 0;
}

bool Inventory::removeItem(size_t slot, uint16_t count) {
    if (slot >= TOTAL_SIZE) {
        return false;
    }

    if (slots[slot].count < count) {
        return false;
    }

    slots[slot].count -= count;
    if (slots[slot].count == 0) {
        slots[slot].type = ItemType::Empty;
    }

    return true;
}

void Inventory::clear() {
    for (auto& slot : slots) {
        slot.type = ItemType::Empty;
        slot.count = 0;
    }
}

} // namespace engine
