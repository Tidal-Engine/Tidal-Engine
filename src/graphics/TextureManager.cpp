#include "graphics/TextureManager.h"
#include "system/UserDataManager.h"
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <fstream>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_SUPPORT_ZLIB  // Required for some formats
#include <stb_image.h>

namespace fs = std::filesystem;

TextureManager::TextureManager(VulkanDevice& device, UserDataManager* userDataManager)
    : m_device(device), m_userDataManager(userDataManager) {
    // Calculate atlas properties
    m_texturesPerRow = m_atlasSize / m_textureSize;
}

TextureManager::~TextureManager() {
    cleanup();
}

bool TextureManager::loadActiveTexturepack() {
    // TEMPORARY: Try loading from dev assets first for debugging
    std::string devTexturepack = "./assets/texturepacks/default";
    if (fs::exists(devTexturepack + "/blocks")) {
        std::cout << "Loading from dev assets directory for debugging..." << std::endl;
        return loadTexturepack(devTexturepack);
    }

    if (!m_userDataManager) {
        std::cerr << "No UserDataManager available" << std::endl;
        createDefaultTexturepack();
        return false;
    }

    std::string activeTexturePack = m_userDataManager->getActiveTexturepackPath();
    return loadTexturepack(activeTexturePack);
}

bool TextureManager::loadTexturepack(const std::string& texturepackPath) {
    if (!fs::exists(texturepackPath)) {
        std::cerr << "Texturepack path does not exist: " << texturepackPath << std::endl;
        // Try to load from user data directory
        if (m_userDataManager) {
            std::string userTexturePack = m_userDataManager->getActiveTexturepackPath();
            if (fs::exists(userTexturePack) && userTexturePack != texturepackPath) {
                return loadTexturepack(userTexturePack);
            }
        }
        createDefaultTexturepack();
        return false;
    }

    cleanup();
    m_currentTexturepack = texturepackPath;
    m_blockTextures.clear();

    try {
        generateTextureAtlas(texturepackPath);
        std::cout << "Successfully loaded texturepack: " << texturepackPath << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to load texturepack: " << e.what() << std::endl;
        createDefaultTexturepack();
        return false;
    }
}

void TextureManager::createDefaultTexturepack() {
    cleanup();
    m_blockTextures.clear();

    // Create a simple procedural atlas
    std::vector<uint8_t> atlasData(m_atlasSize * m_atlasSize * 4);

    // Generate default textures for each block type
    for (int blockIndex = 0; blockIndex < static_cast<int>(BlockType::COUNT); ++blockIndex) {
        BlockType blockType = static_cast<BlockType>(blockIndex);

        int atlasX = (blockIndex % m_texturesPerRow) * m_textureSize;
        int atlasY = (blockIndex / m_texturesPerRow) * m_textureSize;

        // Generate different colors for each block type
        uint8_t r = 128, g = 128, b = 128;
        switch (blockType) {
            case BlockType::STONE: r = 128; g = 128; b = 128; break;
            case BlockType::DIRT: r = 139; g = 69; b = 19; break;
            case BlockType::GRASS: r = 34; g = 139; b = 34; break;
            case BlockType::WOOD: r = 160; g = 82; b = 45; break;
            case BlockType::SAND: r = 238; g = 203; b = 173; break;
            case BlockType::BRICK: r = 178; g = 34; b = 34; break;
            case BlockType::COBBLESTONE: r = 105; g = 105; b = 105; break;
            case BlockType::SNOW: r = 255; g = 250; b = 250; break;
            default: break;
        }

        // Fill texture area with solid color
        for (int y = 0; y < m_textureSize; ++y) {
            for (int x = 0; x < m_textureSize; ++x) {
                int pixelIndex = ((atlasY + y) * m_atlasSize + (atlasX + x)) * 4;
                atlasData[pixelIndex + 0] = r;
                atlasData[pixelIndex + 1] = g;
                atlasData[pixelIndex + 2] = b;
                atlasData[pixelIndex + 3] = 255;
            }
        }

        // Map block type to texture coordinates
        TextureInfo texInfo;
        texInfo.uvMin = calculateUVFromAtlasPosition(atlasX, atlasY);
        texInfo.uvMax = calculateUVFromAtlasPosition(atlasX + m_textureSize, atlasY + m_textureSize);
        texInfo.filename = "default_" + std::to_string(blockIndex);

        m_blockTextures[blockType] = { texInfo };
    }

    createAtlasImage(atlasData);
    createAtlasImageView();
    createAtlasSampler();

    std::cout << "Created default texturepack with procedural textures" << std::endl;
}

bool TextureManager::loadPNGFile(const std::string& filepath, std::vector<uint8_t>& data, int& width, int& height) {
    int channels;
    unsigned char* imageData = stbi_load(filepath.c_str(), &width, &height, &channels, STBI_rgb_alpha);

    if (!imageData) {
        std::cerr << "Failed to load image: " << filepath << " - " << stbi_failure_reason() << std::endl;
        return false;
    }

    std::cout << "Loaded texture: " << filepath << " (" << width << "x" << height << ")" << std::endl;

    // Copy data to vector
    size_t imageSize = width * height * 4;
    data.resize(imageSize);
    memcpy(data.data(), imageData, imageSize);

    stbi_image_free(imageData);
    return true;
}

void TextureManager::generateTextureAtlas(const std::string& texturepackPath) {
    std::string blocksPath = texturepackPath + "/blocks";
    if (!fs::exists(blocksPath)) {
        throw std::runtime_error("Blocks directory not found in texturepack");
    }

    // Count PNG files first
    int pngCount = 0;
    for (const auto& entry : fs::directory_iterator(blocksPath)) {
        if (entry.path().extension() == ".png") {
            pngCount++;
        }
    }

    // If no PNGs found, use default procedural textures
    if (pngCount == 0) {
        std::cout << "No PNG files found, using procedural textures" << std::endl;
        createDefaultTexturepack();
        return;
    }

    std::vector<uint8_t> atlasData(m_atlasSize * m_atlasSize * 4, 0);
    int currentTextureIndex = 0;

    // Load all PNG files from blocks directory
    for (const auto& entry : fs::directory_iterator(blocksPath)) {
        std::string extension = entry.path().extension().string();

        if (extension != ".png" && extension != ".PNG") {
            continue;
        }

        if (currentTextureIndex >= m_texturesPerRow * m_texturesPerRow) {
            std::cerr << "Warning: Too many textures, atlas is full" << std::endl;
            break;
        }

        std::vector<uint8_t> textureData;
        int texWidth, texHeight;

        if (!loadPNGFile(entry.path().string(), textureData, texWidth, texHeight)) {
            continue;
        }

        // Handle texture resizing if needed
        std::vector<uint8_t> resizedData;
        if (texWidth != m_textureSize || texHeight != m_textureSize) {
            std::cout << "Resizing texture " << entry.path().filename()
                      << " from " << texWidth << "x" << texHeight
                      << " to " << m_textureSize << "x" << m_textureSize << std::endl;

            // Simple nearest-neighbor resize
            resizedData.resize(m_textureSize * m_textureSize * 4);
            for (int y = 0; y < m_textureSize; ++y) {
                for (int x = 0; x < m_textureSize; ++x) {
                    int srcX = (x * texWidth) / m_textureSize;
                    int srcY = (y * texHeight) / m_textureSize;

                    int srcIndex = (srcY * texWidth + srcX) * 4;
                    int dstIndex = (y * m_textureSize + x) * 4;

                    resizedData[dstIndex + 0] = textureData[srcIndex + 0]; // R
                    resizedData[dstIndex + 1] = textureData[srcIndex + 1]; // G
                    resizedData[dstIndex + 2] = textureData[srcIndex + 2]; // B
                    resizedData[dstIndex + 3] = textureData[srcIndex + 3]; // A
                }
            }
            textureData = resizedData;
        }

        // Calculate position in atlas
        int atlasX = (currentTextureIndex % m_texturesPerRow) * m_textureSize;
        int atlasY = (currentTextureIndex / m_texturesPerRow) * m_textureSize;

        // Copy texture data to atlas
        for (int y = 0; y < m_textureSize; ++y) {
            for (int x = 0; x < m_textureSize; ++x) {
                int srcIndex = (y * m_textureSize + x) * 4;
                int dstIndex = ((atlasY + y) * m_atlasSize + (atlasX + x)) * 4;

                atlasData[dstIndex + 0] = textureData[srcIndex + 0];
                atlasData[dstIndex + 1] = textureData[srcIndex + 1];
                atlasData[dstIndex + 2] = textureData[srcIndex + 2];
                atlasData[dstIndex + 3] = textureData[srcIndex + 3];
            }
        }

        // Map texture to block type based on filename
        std::string filename = entry.path().stem().string();
        std::cout << "Loaded texture: " << filename << " at atlas position (" << atlasX << ", " << atlasY << ")" << std::endl;

        // Create texture info
        TextureInfo texInfo;
        texInfo.uvMin = calculateUVFromAtlasPosition(atlasX, atlasY);
        texInfo.uvMax = calculateUVFromAtlasPosition(atlasX + m_textureSize, atlasY + m_textureSize);
        texInfo.filename = filename;

        // Map filename to block type
        BlockType blockType = getBlockTypeFromFilename(filename);
        if (blockType != BlockType::AIR) {
            m_blockTextures[blockType] = { texInfo };
            std::cout << "  Mapped to block type: " << static_cast<int>(blockType) << std::endl;
        }

        currentTextureIndex++;
    }

    createAtlasImage(atlasData);
    createAtlasImageView();
    createAtlasSampler();

    std::cout << "Generated texture atlas with " << currentTextureIndex << " textures" << std::endl;
}

void TextureManager::createAtlasImage(const std::vector<uint8_t>& atlasData) {
    VkDeviceSize imageSize = atlasData.size();

    // Create staging buffer
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    m_device.createBuffer(
        imageSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer,
        stagingBufferMemory);

    // Copy data to staging buffer
    void* data;
    vkMapMemory(m_device.getDevice(), stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, atlasData.data(), static_cast<size_t>(imageSize));
    vkUnmapMemory(m_device.getDevice(), stagingBufferMemory);

    // Create image
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = static_cast<uint32_t>(m_atlasSize);
    imageInfo.extent.height = static_cast<uint32_t>(m_atlasSize);
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    m_device.createImageWithInfo(imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_atlasImage, m_atlasMemory);

    // Transition and copy
    VkCommandBuffer commandBuffer = m_device.beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_atlasImage;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    // Copy buffer to image
    m_device.copyBufferToImage(stagingBuffer, m_atlasImage, static_cast<uint32_t>(m_atlasSize), static_cast<uint32_t>(m_atlasSize));

    // Transition to shader read
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    m_device.endSingleTimeCommands(commandBuffer);

    // Cleanup staging buffer
    vkDestroyBuffer(m_device.getDevice(), stagingBuffer, nullptr);
    vkFreeMemory(m_device.getDevice(), stagingBufferMemory, nullptr);
}

void TextureManager::createAtlasImageView() {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_atlasImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(m_device.getDevice(), &viewInfo, nullptr, &m_atlasImageView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create texture atlas image view!");
    }
}

void TextureManager::createAtlasSampler() {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_NEAREST;  // Pixelated look like Minecraft
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;

    if (vkCreateSampler(m_device.getDevice(), &samplerInfo, nullptr, &m_atlasSampler) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create texture atlas sampler!");
    }
}

void TextureManager::setupDefaultBlockMapping() {
    // This function is now only used for procedural textures
    // For PNG loading, textures are mapped directly by filename
    for (int i = 0; i < static_cast<int>(BlockType::COUNT); ++i) {
        BlockType blockType = static_cast<BlockType>(i);

        int atlasX = (i % m_texturesPerRow) * m_textureSize;
        int atlasY = (i / m_texturesPerRow) * m_textureSize;

        TextureInfo texInfo;
        texInfo.uvMin = calculateUVFromAtlasPosition(atlasX, atlasY);
        texInfo.uvMax = calculateUVFromAtlasPosition(atlasX + m_textureSize, atlasY + m_textureSize);
        texInfo.filename = "block_" + std::to_string(i);

        m_blockTextures[blockType] = { texInfo };
    }
}

BlockType TextureManager::getBlockTypeFromFilename(const std::string& filename) const {
    // Map texture filenames to block types
    if (filename == "stone") return BlockType::STONE;
    if (filename == "dirt") return BlockType::DIRT;
    if (filename == "grass_top" || filename == "grass") return BlockType::GRASS;
    if (filename == "grass_side") return BlockType::GRASS; // Handle multiple grass textures later
    if (filename == "wood") return BlockType::WOOD;
    if (filename == "sand") return BlockType::SAND;
    if (filename == "brick") return BlockType::BRICK;
    if (filename == "cobblestone") return BlockType::COBBLESTONE;
    if (filename == "snow") return BlockType::SNOW;

    // Return AIR for unknown filenames (will be skipped)
    std::cout << "Warning: Unknown texture filename: " << filename << std::endl;
    return BlockType::AIR;
}

TextureInfo TextureManager::getBlockTexture(BlockType blockType, int face) const {
    auto it = m_blockTextures.find(blockType);
    if (it == m_blockTextures.end() || it->second.empty()) {
        // Return default texture (stone)
        auto defaultIt = m_blockTextures.find(BlockType::STONE);
        if (defaultIt != m_blockTextures.end() && !defaultIt->second.empty()) {
            return defaultIt->second[0];
        }

        // Fallback: return first texture in atlas
        TextureInfo fallback;
        fallback.uvMin = {0.0f, 0.0f};
        fallback.uvMax = {1.0f / m_texturesPerRow, 1.0f / m_texturesPerRow};
        return fallback;
    }

    // Support multiple textures per block (for different faces)
    int textureIndex = std::min(face, static_cast<int>(it->second.size() - 1));
    return it->second[textureIndex];
}

glm::vec2 TextureManager::getTextureUV(BlockType blockType, int face, glm::vec2 localUV) const {
    TextureInfo texInfo = getBlockTexture(blockType, face);

    // Interpolate between uvMin and uvMax based on localUV
    return glm::mix(texInfo.uvMin, texInfo.uvMax, localUV);
}

glm::vec2 TextureManager::calculateUVFromAtlasPosition(int atlasX, int atlasY) const {
    return glm::vec2(
        static_cast<float>(atlasX) / static_cast<float>(m_atlasSize),
        static_cast<float>(atlasY) / static_cast<float>(m_atlasSize)
    );
}

void TextureManager::cleanup() {
    if (m_atlasSampler != VK_NULL_HANDLE) {
        vkDestroySampler(m_device.getDevice(), m_atlasSampler, nullptr);
        m_atlasSampler = VK_NULL_HANDLE;
    }

    if (m_atlasImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(m_device.getDevice(), m_atlasImageView, nullptr);
        m_atlasImageView = VK_NULL_HANDLE;
    }

    if (m_atlasImage != VK_NULL_HANDLE) {
        vkDestroyImage(m_device.getDevice(), m_atlasImage, nullptr);
        m_atlasImage = VK_NULL_HANDLE;
    }

    if (m_atlasMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device.getDevice(), m_atlasMemory, nullptr);
        m_atlasMemory = VK_NULL_HANDLE;
    }
}