#pragma once

#include <imgui.h>
#include <vulkan/vulkan.h>
#include <unordered_map>
#include <string>
#include "shared/Item.hpp"

namespace engine {

// Forward declarations
class Inventory;
class InputManager;
class ItemRegistry;

/**
 * @brief Hotbar UI renderer
 *
 * Displays the player's hotbar (9 slots) at the bottom of the screen.
 * Shows item icons, counts, and highlights the selected slot.
 */
class HotbarUI {
public:
    /**
     * @brief Construct hotbar UI
     * @param inventory Player inventory to display
     * @param device Vulkan device for texture loading
     * @param physicalDevice Vulkan physical device
     * @param commandPool Command pool for texture uploads
     * @param graphicsQueue Graphics queue for texture uploads
     */
    HotbarUI(Inventory* inventory, VkDevice device, VkPhysicalDevice physicalDevice,
             VkCommandPool commandPool, VkQueue graphicsQueue);
    ~HotbarUI();

    /**
     * @brief Initialize textures (call after ImGui is initialized)
     */
    void init();

    /**
     * @brief Render the hotbar UI
     * Should be called after ImGui::NewFrame() and before ImGui::Render()
     */
    void render();

    /**
     * @brief Handle input for hotbar selection
     * @param input Input manager to poll keys
     */
    void handleInput(const InputManager* input);

private:
    Inventory* inventory;

    VkDevice device;
    VkPhysicalDevice physicalDevice;
    VkCommandPool commandPool;
    VkQueue graphicsQueue;

    // Block texture cache: BlockType -> ImTextureID
    std::unordered_map<ItemType, VkDescriptorSet> blockTextures;

    // Vulkan resources for textures
    struct TextureData {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkSampler sampler = VK_NULL_HANDLE;
    };
    std::unordered_map<ItemType, TextureData> textureResources;

    static constexpr float SLOT_SIZE = 64.0f;
    static constexpr float SLOT_PADDING = 4.0f;
    static constexpr float HOTBAR_Y_OFFSET = 20.0f;  // Distance from bottom of screen

    /**
     * @brief Render a single inventory slot
     * @param index Slot index (0-8)
     * @param selected Is this the selected slot?
     */
    void renderSlot(size_t index, bool selected);

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
