#pragma once

#include "shared/Item.hpp"
#include <imgui.h>
#include <vulkan/vulkan.h>
#include <unordered_map>
#include <string>
#include <functional>

namespace engine {

// Forward declarations
class Inventory;
class ItemRegistry;

/**
 * @brief Creative mode inventory menu
 *
 * Full-screen overlay showing all available items/blocks.
 * Players can click items to add them to their hotbar/inventory.
 * Toggled with 'E' key.
 */
class CreativeMenu {
public:
    /**
     * @brief Construct creative menu
     * @param inventory Player inventory to add items to
     * @param device Vulkan device for texture loading
     * @param physicalDevice Vulkan physical device
     * @param commandPool Command pool for texture uploads
     * @param graphicsQueue Graphics queue for texture uploads
     */
    CreativeMenu(Inventory* inventory, VkDevice device, VkPhysicalDevice physicalDevice,
                 VkCommandPool commandPool, VkQueue graphicsQueue);
    ~CreativeMenu();

    /**
     * @brief Initialize textures (call after ImGui is initialized)
     */
    void init();

    /**
     * @brief Toggle menu visibility
     */
    void toggle();

    /**
     * @brief Open the menu
     */
    void open() { isOpen = true; }

    /**
     * @brief Close the menu
     */
    void close() { isOpen = false; }

    /**
     * @brief Check if menu is open
     */
    bool isMenuOpen() const { return isOpen; }

    /**
     * @brief Render the creative menu
     * Should be called after ImGui::NewFrame() and before ImGui::Render()
     */
    void render();

    /**
     * @brief Set callback for when inventory is modified via creative menu
     */
    void setOnInventoryChanged(std::function<void()> callback) {
        onInventoryChanged = callback;
    }

private:
    Inventory* inventory;
    bool isOpen = false;
    std::string searchFilter;

    // Callback for inventory changes
    std::function<void()> onInventoryChanged;

    VkDevice device;
    VkPhysicalDevice physicalDevice;
    VkCommandPool commandPool;
    VkQueue graphicsQueue;

    // Block texture cache: ItemType -> ImTextureID
    std::unordered_map<ItemType, VkDescriptorSet> blockTextures;

    // Vulkan resources for textures
    struct TextureData {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkSampler sampler = VK_NULL_HANDLE;
    };
    std::unordered_map<ItemType, TextureData> textureResources;

    // UI layout constants
    static constexpr float ITEM_BUTTON_SIZE = 64.0f;
    static constexpr float ITEM_PADDING = 8.0f;
    static constexpr int ITEMS_PER_ROW = 8;

    /**
     * @brief Render search bar
     */
    void renderSearchBar();

    /**
     * @brief Render grid of available items
     */
    void renderItemGrid();

    /**
     * @brief Render a single item button
     * @param itemType Type of item to render
     * @return true if clicked
     */
    bool renderItemButton(ItemType itemType);

    /**
     * @brief Load a block texture for ImGui
     * @param itemType Item type to load texture for
     * @param texturePath Path to texture file
     */
    void loadBlockTexture(ItemType itemType, const std::string& texturePath);

    /**
     * @brief Cleanup all texture resources
     */
    void cleanupTextures();

    /**
     * @brief Helper to find memory type
     */
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
};

} // namespace engine
