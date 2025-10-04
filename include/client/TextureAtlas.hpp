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
    /**
     * @brief Construct a new Texture Atlas
     * @param device Logical Vulkan device
     * @param physicalDevice Physical device for memory properties
     * @param commandPool Command pool for transfer commands
     * @param graphicsQueue Graphics queue for command submission
     */
    TextureAtlas(VkDevice device, VkPhysicalDevice physicalDevice,
                 VkCommandPool commandPool, VkQueue graphicsQueue);

    /**
     * @brief Destroy the Texture Atlas and free Vulkan resources
     */
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

    /**
     * @brief Get the texture atlas image view
     * @return VkImageView Image view for the complete texture atlas
     */
    VkImageView getImageView() const { return textureImageView; }

    /**
     * @brief Get the texture sampler
     * @return VkSampler Sampler with filtering settings for block textures
     */
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
     * @brief Calculate UV coordinates for a texture in the atlas
     * @param index Texture index in the atlas
     * @param totalTextures Total number of textures in the atlas
     * @return glm::vec4 UV coordinates (uMin, vMin, uMax, vMax)
     */
    glm::vec4 calculateUVs(uint32_t index, uint32_t totalTextures) const;

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

    VkDevice device;                    ///< Logical Vulkan device
    VkPhysicalDevice physicalDevice;    ///< Physical device for memory queries
    VkCommandPool commandPool;          ///< Command pool for transfer operations
    VkQueue graphicsQueue;              ///< Graphics queue for command submission

    VkImage textureImage = VK_NULL_HANDLE;              ///< Texture atlas image
    VkDeviceMemory textureImageMemory = VK_NULL_HANDLE; ///< Device memory for texture image
    VkImageView textureImageView = VK_NULL_HANDLE;      ///< Image view for texture sampling
    VkSampler textureSampler = VK_NULL_HANDLE;          ///< Sampler for texture filtering

    std::unordered_map<BlockType, glm::vec4> blockUVs;  ///< Map block type to UV coordinates in atlas

    uint32_t atlasWidth = 0;        ///< Total atlas width in pixels
    uint32_t atlasHeight = 0;       ///< Total atlas height in pixels
    uint32_t textureSize = 160;     ///< Size of individual block textures (160x160)
};

} // namespace engine
