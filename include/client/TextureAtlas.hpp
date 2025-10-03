#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include "shared/Block.hpp"

namespace engine {

/**
 * @brief Manages a texture atlas for block textures
 *
 * Combines multiple block textures into a single atlas for efficient rendering.
 * Provides UV coordinate mapping for each block type.
 */
class TextureAtlas {
public:
    TextureAtlas(VkDevice device, VkPhysicalDevice physicalDevice,
                 VkCommandPool commandPool, VkQueue graphicsQueue);
    ~TextureAtlas();

    // Delete copy
    TextureAtlas(const TextureAtlas&) = delete;
    TextureAtlas& operator=(const TextureAtlas&) = delete;

    /**
     * @brief Load block textures and build the atlas
     * @param texturePath Base path to texture directory
     */
    void loadTextures(const std::string& texturePath);

    /**
     * @brief Get UV coordinates for a specific block type
     * @param type Block type
     * @return vec4 containing (u_min, v_min, u_max, v_max)
     */
    glm::vec4 getBlockUVs(BlockType type) const;

    VkImageView getImageView() const { return textureImageView; }
    VkSampler getSampler() const { return textureSampler; }

private:
    /**
     * @brief Create Vulkan texture image from pixel data
     * @param pixels Raw pixel data (RGBA)
     * @param width Image width
     * @param height Image height
     */
    void createTextureImage(const unsigned char* pixels, uint32_t width, uint32_t height);

    /**
     * @brief Create image view for the texture image
     */
    void createTextureImageView();

    /**
     * @brief Create sampler for texture filtering
     */
    void createTextureSampler();

    /**
     * @brief Transition image layout using pipeline barrier
     * @param image Image to transition
     * @param format Image format
     * @param oldLayout Current layout
     * @param newLayout Target layout
     */
    void transitionImageLayout(VkImage image, VkFormat format,
                              VkImageLayout oldLayout, VkImageLayout newLayout);

    /**
     * @brief Copy buffer data to image
     * @param buffer Source buffer
     * @param image Destination image
     * @param width Image width
     * @param height Image height
     */
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

    VkDevice device;
    VkPhysicalDevice physicalDevice;
    VkCommandPool commandPool;
    VkQueue graphicsQueue;

    VkImage textureImage = VK_NULL_HANDLE;
    VkDeviceMemory textureImageMemory = VK_NULL_HANDLE;
    VkImageView textureImageView = VK_NULL_HANDLE;
    VkSampler textureSampler = VK_NULL_HANDLE;

    // Map block type to UV coordinates in the atlas
    std::unordered_map<BlockType, glm::vec4> blockUVs;

    uint32_t atlasWidth = 0;
    uint32_t atlasHeight = 0;
    uint32_t textureSize = 160;  // Size of individual textures (160x160)
};

} // namespace engine
