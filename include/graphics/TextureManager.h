/**
 * @file TextureManager.h
 * @brief Vulkan-based texture atlas management system for block-based rendering
 * @author Tidal Engine Team
 * @version 1.0
 */

#pragma once

#include "vulkan/VulkanDevice.h"
#include "game/Chunk.h"
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <string>
#include <unordered_map>
#include <vector>

// Forward declarations
class UserDataManager;

/**
 * @brief Texture information for UV coordinate mapping
 *
 * Contains UV coordinate boundaries and source filename for a texture
 * within the texture atlas. Used for mapping block faces to specific
 * regions of the atlas texture.
 */
struct TextureInfo {
    glm::vec2 uvMin;        ///< Top-left UV coordinate in atlas (0.0-1.0)
    glm::vec2 uvMax;        ///< Bottom-right UV coordinate in atlas (0.0-1.0)
    std::string filename;   ///< Source texture filename for debugging
};

/**
 * @brief Vulkan-based texture atlas manager for block rendering
 *
 * The TextureManager handles loading, organizing, and providing access to
 * block textures through a unified texture atlas system. Key features include:
 * - Texturepack loading and management
 * - Automatic texture atlas generation from individual textures
 * - Vulkan image and sampler management
 * - UV coordinate mapping for block face rendering
 * - Support for different textures per block face
 *
 * The system uses a 256x256 atlas containing 16x16 pixel textures arranged
 * in a 16x16 grid, allowing for up to 256 unique textures per texturepack.
 *
 * @see TextureInfo for UV coordinate structures
 * @see VulkanDevice for graphics device integration
 */
class TextureManager {
public:
    /**
     * @brief Construct texture manager with Vulkan device
     * @param device Vulkan device for graphics operations
     * @param userDataManager Optional user data manager for settings
     */
    TextureManager(VulkanDevice& device, UserDataManager* userDataManager = nullptr);

    /**
     * @brief Destructor - cleans up Vulkan resources
     */
    ~TextureManager();

    /// @name Texturepack Management
    /// @{
    /**
     * @brief Load texturepack from specified directory
     * @param texturepackPath Path to texturepack directory containing PNG files
     * @return True if texturepack loaded successfully
     * @note Generates texture atlas from individual 16x16 PNG files
     */
    bool loadTexturepack(const std::string& texturepackPath);

    /**
     * @brief Load the currently active texturepack from user settings
     * @return True if active texturepack loaded successfully
     * @note Falls back to default texturepack if active one fails to load
     */
    bool loadActiveTexturepack();

    /**
     * @brief Create and load default built-in texturepack
     * @note Provides fallback textures when no custom texturepack is available
     */
    void createDefaultTexturepack();
    /// @}

    /// @name Vulkan Resource Access
    /// @{
    /**
     * @brief Get Vulkan image view for the texture atlas
     * @return Image view handle for shader binding
     */
    VkImageView getAtlasImageView() const { return m_atlasImageView; }

    /**
     * @brief Get Vulkan sampler for texture atlas
     * @return Sampler handle configured for block texture filtering
     */
    VkSampler getAtlasSampler() const { return m_atlasSampler; }
    /// @}

    /// @name Block Texture Mapping
    /// @{
    /**
     * @brief Get texture information for specific block face
     * @param blockType Type of block to get texture for
     * @param face Face index (0-5 for cube faces, default 0 for all faces)
     * @return TextureInfo containing UV coordinates and filename
     * @note Returns default texture if block type not found
     */
    TextureInfo getBlockTexture(BlockType blockType, int face = 0) const;

    /**
     * @brief Convert local UV coordinates to atlas UV coordinates
     * @param blockType Block type to get texture mapping for
     * @param face Face index for multi-textured blocks
     * @param localUV Local UV coordinates (0.0-1.0) within the texture
     * @return Atlas UV coordinates accounting for texture position
     */
    glm::vec2 getTextureUV(BlockType blockType, int face, glm::vec2 localUV) const;
    /// @}

    /// @name Atlas Properties
    /// @{
    /**
     * @brief Get texture atlas size in pixels
     * @return Atlas width/height (currently 256x256)
     */
    int getAtlasSize() const { return m_atlasSize; }

    /**
     * @brief Get individual texture size in pixels
     * @return Texture width/height (currently 16x16)
     */
    int getTextureSize() const { return m_textureSize; }
    /// @}

private:
    /// @name Core Dependencies
    /// @{
    VulkanDevice& m_device;                     ///< Reference to Vulkan device for graphics operations
    UserDataManager* m_userDataManager;        ///< Optional user data manager for settings access
    /// @}

    /// @name Atlas Configuration
    /// @{
    static constexpr int m_textureSize = 16;   ///< Individual texture size in pixels (16x16)
    int m_atlasSize = 256;                     ///< Atlas size in pixels (256x256 = 16x16 grid)
    int m_texturesPerRow = 16;                 ///< Number of textures per row in atlas
    /// @}

    /// @name Vulkan Resources
    /// @{
    VkImage m_atlasImage = VK_NULL_HANDLE;         ///< Vulkan image for texture atlas
    VkDeviceMemory m_atlasMemory = VK_NULL_HANDLE; ///< Device memory backing the atlas image
    VkImageView m_atlasImageView = VK_NULL_HANDLE; ///< Image view for shader access
    VkSampler m_atlasSampler = VK_NULL_HANDLE;     ///< Sampler with filtering configuration
    /// @}

    /// @name Texture Management
    /// @{
    std::unordered_map<BlockType, std::vector<TextureInfo>> m_blockTextures; ///< Block type to texture mapping
    std::string m_currentTexturepack;                                        ///< Name of currently loaded texturepack
    /// @}

    /// @name File I/O Operations
    /// @{
    /**
     * @brief Load PNG file and decode to raw pixel data
     * @param filepath Path to PNG file to load
     * @param data Output vector for decoded pixel data (RGBA)
     * @param width Output width of loaded image
     * @param height Output height of loaded image
     * @return True if PNG loaded successfully
     * @note Expects 16x16 pixel PNG files for proper atlas generation
     */
    bool loadPNGFile(const std::string& filepath, std::vector<uint8_t>& data, int& width, int& height);
    /// @}

    /// @name Atlas Generation
    /// @{
    /**
     * @brief Generate texture atlas from texturepack directory
     * @param texturepackPath Path to directory containing PNG texture files
     * @note Combines individual 16x16 textures into 256x256 atlas
     */
    void generateTextureAtlas(const std::string& texturepackPath);

    /**
     * @brief Create Vulkan image from atlas pixel data
     * @param atlasData RGBA pixel data for the complete atlas
     * @note Creates VkImage, allocates device memory, and uploads data
     */
    void createAtlasImage(const std::vector<uint8_t>& atlasData);

    /**
     * @brief Create image view for atlas texture access
     * @note Creates VkImageView for shader binding
     */
    void createAtlasImageView();

    /**
     * @brief Create sampler for atlas texture filtering
     * @note Configures nearest-neighbor filtering for pixelated look
     */
    void createAtlasSampler();
    /// @}

    /// @name Texture Mapping Setup
    /// @{
    /**
     * @brief Setup default block to texture mappings
     * @note Establishes relationships between block types and texture filenames
     */
    void setupDefaultBlockMapping();

    /**
     * @brief Add texture mapping for specific block type and face
     * @param blockType Block type to associate with texture
     * @param filename Texture filename (used for atlas lookup)
     * @param face Face index for multi-textured blocks (default 0)
     */
    void addBlockTexture(BlockType blockType, const std::string& filename, int face = 0);

    /**
     * @brief Calculate UV coordinates from atlas grid position
     * @param atlasX X position in atlas grid (0-15)
     * @param atlasY Y position in atlas grid (0-15)
     * @return UV coordinates as glm::vec2 (0.0-1.0 range)
     */
    glm::vec2 calculateUVFromAtlasPosition(int atlasX, int atlasY) const;

    /**
     * @brief Determine block type from texture filename
     * @param filename Texture filename to analyze
     * @return Corresponding block type, or default if not recognized
     * @note Uses filename patterns to infer block types
     */
    BlockType getBlockTypeFromFilename(const std::string& filename) const;
    /// @}

    /// @name Resource Management
    /// @{
    /**
     * @brief Clean up all Vulkan resources
     * @note Destroys images, memory, views, and samplers
     */
    void cleanup();
    /// @}
};