#include "client/TextureAtlas.hpp"
#include "vulkan/VulkanBuffer.hpp"
#include "core/Logger.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "../../external/stb/stb_image.h"

#include <cstring>
#include <stdexcept>
#include <vector>

namespace engine {

TextureAtlas::TextureAtlas(VkDevice device, VkPhysicalDevice physicalDevice,
                           VkCommandPool commandPool, VkQueue graphicsQueue)
    : device(device), physicalDevice(physicalDevice),
      commandPool(commandPool), graphicsQueue(graphicsQueue) {
}

TextureAtlas::~TextureAtlas() {
    if (textureSampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, textureSampler, nullptr);
    }
    if (textureImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, textureImageView, nullptr);
    }
    if (textureImage != VK_NULL_HANDLE) {
        vkDestroyImage(device, textureImage, nullptr);
    }
    if (textureImageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, textureImageMemory, nullptr);
    }
}

void TextureAtlas::loadTextures(const std::string& texturePath) {
    LOG_INFO("Loading texture atlas from: {}", texturePath);

    // Define all block textures to load (order matters for texture IDs)
    std::vector<std::string> textureNames = {
        "stone",
        "dirt",
        "grass_side",
        "grass_top",
        "cobblestone",
        "wood",
        "sand",
        "brick",
        "snow"
    };

    const uint32_t numTextures = textureNames.size();
    atlasWidth = textureSize * numTextures;
    atlasHeight = textureSize;

    // Allocate atlas buffer
    std::vector<unsigned char> atlasData(atlasWidth * atlasHeight * 4, 0);

    // Load each texture
    int width, height, channels;
    for (uint32_t i = 0; i < numTextures; ++i) {
        std::string texPath = texturePath + "/default/blocks/" + textureNames[i] + ".png";
        unsigned char* pixels = stbi_load(texPath.c_str(), &width, &height, &channels, STBI_rgb_alpha);

        if (!pixels) {
            LOG_ERROR("Failed to load texture: {}", texPath);
            throw std::runtime_error("Failed to load texture: " + texPath);
        }

        LOG_INFO("Loaded {}.png: {}x{} with {} channels", textureNames[i], width, height, channels);

        // Copy texture to atlas at position (i * textureSize, 0)
        for (uint32_t y = 0; y < textureSize; ++y) {
            for (uint32_t x = 0; x < textureSize; ++x) {
                uint32_t atlasIndex = (y * atlasWidth + (i * textureSize + x)) * 4;
                uint32_t srcIndex = (y * width + x) * 4;
                atlasData[atlasIndex + 0] = pixels[srcIndex + 0];
                atlasData[atlasIndex + 1] = pixels[srcIndex + 1];
                atlasData[atlasIndex + 2] = pixels[srcIndex + 2];
                atlasData[atlasIndex + 3] = pixels[srcIndex + 3];
            }
        }
        stbi_image_free(pixels);
    }

    // Calculate UV coordinates for each block type
    // Atlas layout: [stone | dirt | grass_side | grass_top | cobblestone | wood | sand | brick | snow]
    blockUVs[BlockType::Stone] = calculateUVs(0, numTextures);
    blockUVs[BlockType::Dirt] = calculateUVs(1, numTextures);
    blockUVs[BlockType::GrassSide] = calculateUVs(2, numTextures);
    blockUVs[BlockType::GrassTop] = calculateUVs(3, numTextures);
    blockUVs[BlockType::Cobblestone] = calculateUVs(4, numTextures);
    blockUVs[BlockType::Wood] = calculateUVs(5, numTextures);
    blockUVs[BlockType::Sand] = calculateUVs(6, numTextures);
    blockUVs[BlockType::Brick] = calculateUVs(7, numTextures);
    blockUVs[BlockType::Snow] = calculateUVs(8, numTextures);

    LOG_INFO("Texture atlas created: {}x{}", atlasWidth, atlasHeight);
    for (uint32_t i = 0; i < numTextures; ++i) {
        auto uvs = calculateUVs(i, numTextures);
        LOG_INFO("{} UVs: ({}, {}) to ({}, {})", textureNames[i], uvs.x, uvs.y, uvs.z, uvs.w);
    }

    // Create Vulkan texture from atlas
    createTextureImage(atlasData.data(), atlasWidth, atlasHeight);
    createTextureImageView();
    createTextureSampler();
}

glm::vec4 TextureAtlas::getBlockUVs(BlockType type) const {
    auto it = blockUVs.find(type);
    if (it != blockUVs.end()) {
        return it->second;
    }

    // Fallback to dirt if block type not found
    auto dirtIt = blockUVs.find(BlockType::Dirt);
    if (dirtIt != blockUVs.end()) {
        LOG_WARN("Block type {} not found in texture atlas, using Dirt as fallback", static_cast<int>(type));
        return dirtIt->second;
    }

    // Ultimate fallback if even dirt is missing (should never happen)
    LOG_ERROR("Texture atlas missing both requested block type {} and Dirt fallback!", static_cast<int>(type));
    return glm::vec4(0.0f, 0.0f, 1.0f, 1.0f); // Return full atlas as error indicator
}

void TextureAtlas::createTextureImage(const unsigned char* pixels, uint32_t width, uint32_t height) {
    VkDeviceSize imageSize = width * height * 4;

    // Create staging buffer
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = imageSize;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &stagingBuffer) != VK_SUCCESS) {
        LOG_ERROR("Failed to create staging buffer of size {}", imageSize);
        throw std::runtime_error("Failed to create staging buffer");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, stagingBuffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = VulkanBuffer::findMemoryType(physicalDevice, memRequirements.memoryTypeBits,
                                                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &stagingBufferMemory) != VK_SUCCESS) {
        LOG_ERROR("Failed to allocate staging buffer memory of size {}", allocInfo.allocationSize);
        throw std::runtime_error("Failed to allocate staging buffer memory");
    }

    vkBindBufferMemory(device, stagingBuffer, stagingBufferMemory, 0);

    // Copy pixel data to staging buffer
    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
    std::memcpy(data, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(device, stagingBufferMemory);

    // Create image
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    if (vkCreateImage(device, &imageInfo, nullptr, &textureImage) != VK_SUCCESS) {
        LOG_ERROR("Failed to create texture image of size {}x{}", width, height);
        throw std::runtime_error("Failed to create texture image");
    }

    vkGetImageMemoryRequirements(device, textureImage, &memRequirements);

    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = VulkanBuffer::findMemoryType(physicalDevice, memRequirements.memoryTypeBits,
                                                             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &textureImageMemory) != VK_SUCCESS) {
        LOG_ERROR("Failed to allocate texture image memory of size {}", allocInfo.allocationSize);
        throw std::runtime_error("Failed to allocate texture image memory");
    }

    vkBindImageMemory(device, textureImage, textureImageMemory, 0);

    // Transition image layout and copy from buffer
    transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB,
                         VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copyBufferToImage(stagingBuffer, textureImage, width, height);
    transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // Cleanup staging buffer
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void TextureAtlas::transitionImageLayout(VkImage image, VkFormat format,
                                        VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        throw std::invalid_argument("Unsupported layout transition");
    }

    vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

void TextureAtlas::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

void TextureAtlas::createTextureImageView() {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = textureImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device, &viewInfo, nullptr, &textureImageView) != VK_SUCCESS) {
        LOG_ERROR("Failed to create texture image view");
        throw std::runtime_error("Failed to create texture image view");
    }
}

void TextureAtlas::createTextureSampler() {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_NEAREST;  // Nearest for pixel art
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;

    if (vkCreateSampler(device, &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS) {
        LOG_ERROR("Failed to create texture sampler");
        throw std::runtime_error("Failed to create texture sampler");
    }
}

glm::vec4 TextureAtlas::calculateUVs(uint32_t index, uint32_t totalTextures) const {
    float uMin = static_cast<float>(index * textureSize) / static_cast<float>(atlasWidth);
    float uMax = static_cast<float>((index + 1) * textureSize) / static_cast<float>(atlasWidth);
    float vMin = 0.0f;
    float vMax = 1.0f;
    return glm::vec4(uMin, vMin, uMax, vMax);
}

} // namespace engine
