#include "client/ItemRegistry.hpp"
#include "core/Logger.hpp"

namespace engine {

ItemRegistry::ItemRegistry() {
    registerVanillaItems();
}

ItemRegistry& ItemRegistry::instance() {
    static ItemRegistry inst;
    return inst;
}

void ItemRegistry::registerItem(const ItemProperties& props) {
    items[props.type] = props;
    LOG_DEBUG("Registered item: {} ({})", props.displayName, props.name);
}

const ItemProperties* ItemRegistry::getItem(ItemType type) const {
    auto it = items.find(type);
    if (it != items.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<ItemType> ItemRegistry::getAllItems() const {
    std::vector<ItemType> result;
    result.reserve(items.size());
    for (const auto& [type, props] : items) {
        if (type != ItemType::Empty) {
            result.push_back(type);
        }
    }
    return result;
}

std::vector<ItemType> ItemRegistry::getAllBlocks() const {
    std::vector<ItemType> result;
    for (const auto& [type, props] : items) {
        if (props.isBlock && type != ItemType::Empty) {
            result.push_back(type);
        }
    }
    return result;
}

void ItemRegistry::registerVanillaItems() {
    if (initialized) {
        return;
    }

    LOG_INFO("Registering vanilla items...");

    // Register all blocks from BlockType enum as placeable items
    registerItem({ItemType::Stone, "stone", "Stone", true, BlockType::Stone, 64});
    registerItem({ItemType::Dirt, "dirt", "Dirt", true, BlockType::Dirt, 64});
    registerItem({ItemType::Cobblestone, "cobblestone", "Cobblestone", true, BlockType::Cobblestone, 64});
    registerItem({ItemType::Wood, "wood", "Wood", true, BlockType::Wood, 64});
    registerItem({ItemType::Sand, "sand", "Sand", true, BlockType::Sand, 64});
    registerItem({ItemType::Brick, "brick", "Brick", true, BlockType::Brick, 64});
    registerItem({ItemType::Snow, "snow", "Snow", true, BlockType::Snow, 64});
    registerItem({ItemType::Grass, "grass", "Grass Block", true, BlockType::Grass, 64});

    // Future: Register tools, food, etc.

    initialized = true;
    LOG_INFO("Registered {} item types", items.size());
}

} // namespace engine
