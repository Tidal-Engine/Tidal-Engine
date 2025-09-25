#pragma once

#include "VulkanDevice.h"
#include "Chunk.h"
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <string>
#include <unordered_map>
#include <vector>

// Forward declarations
class UserDataManager;

struct TextureInfo {
    glm::vec2 uvMin;  // Top-left UV coordinate
    glm::vec2 uvMax;  // Bottom-right UV coordinate
    std::string filename;
};

class TextureManager {
public:
    TextureManager(VulkanDevice& device, UserDataManager* userDataManager = nullptr);
    ~TextureManager();

    // Texturepack management
    bool loadTexturepack(const std::string& texturepackPath);
    bool loadActiveTexturepack();
    void createDefaultTexturepack();

    // Texture atlas access
    VkImageView getAtlasImageView() const { return m_atlasImageView; }
    VkSampler getAtlasSampler() const { return m_atlasSampler; }

    // Block texture mapping
    TextureInfo getBlockTexture(BlockType blockType, int face = 0) const;
    glm::vec2 getTextureUV(BlockType blockType, int face, glm::vec2 localUV) const;

    // Atlas info
    int getAtlasSize() const { return m_atlasSize; }
    int getTextureSize() const { return m_textureSize; }

private:
    VulkanDevice& m_device;
    UserDataManager* m_userDataManager;

    // Atlas properties
    static constexpr int m_textureSize = 16;  // Individual texture size (16x16)
    int m_atlasSize = 256;  // Atlas size (256x256 = 16x16 grid)
    int m_texturesPerRow = 16;  // 16 textures per row

    // Vulkan objects
    VkImage m_atlasImage = VK_NULL_HANDLE;
    VkDeviceMemory m_atlasMemory = VK_NULL_HANDLE;
    VkImageView m_atlasImageView = VK_NULL_HANDLE;
    VkSampler m_atlasSampler = VK_NULL_HANDLE;

    // Texture mapping
    std::unordered_map<BlockType, std::vector<TextureInfo>> m_blockTextures;
    std::string m_currentTexturepack;

    // Helper methods
    bool loadPNGFile(const std::string& filepath, std::vector<uint8_t>& data, int& width, int& height);
    void generateTextureAtlas(const std::string& texturepackPath);
    void createAtlasImage(const std::vector<uint8_t>& atlasData);
    void createAtlasImageView();
    void createAtlasSampler();
    void setupDefaultBlockMapping();
    void addBlockTexture(BlockType blockType, const std::string& filename, int face = 0);
    glm::vec2 calculateUVFromAtlasPosition(int atlasX, int atlasY) const;
    BlockType getBlockTypeFromFilename(const std::string& filename) const;

    // Cleanup
    void cleanup();
};