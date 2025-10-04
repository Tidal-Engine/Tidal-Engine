#include "client/CreativeMenu.hpp"
#include "client/Inventory.hpp"
#include "client/ItemRegistry.hpp"
#include "core/Logger.hpp"
#include <backends/imgui_impl_vulkan.h>
#include "../../external/stb/stb_image.h"
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <cstring>

namespace engine {

CreativeMenu::CreativeMenu(Inventory* inv, VkDevice dev, VkPhysicalDevice physDev,
                           VkCommandPool cmdPool, VkQueue queue)
    : inventory(inv), device(dev), physicalDevice(physDev),
      commandPool(cmdPool), graphicsQueue(queue) {
}

CreativeMenu::~CreativeMenu() {
    cleanupTextures();
}

void CreativeMenu::init() {
    // Load block textures for all placeable items
    std::string basePath = "assets/texturepacks/default/blocks/";

    loadBlockTexture(ItemType::Stone, basePath + "stone.png");
    loadBlockTexture(ItemType::Dirt, basePath + "dirt.png");
    loadBlockTexture(ItemType::Cobblestone, basePath + "cobblestone.png");
    loadBlockTexture(ItemType::Wood, basePath + "wood.png");
    loadBlockTexture(ItemType::Sand, basePath + "sand.png");
    loadBlockTexture(ItemType::Brick, basePath + "brick.png");
    loadBlockTexture(ItemType::Snow, basePath + "snow.png");
    loadBlockTexture(ItemType::Grass, basePath + "grass_side.png");  // Use side texture for grass icon

    LOG_INFO("CreativeMenu textures loaded");
}

void CreativeMenu::toggle() {
    isOpen = !isOpen;
    if (isOpen) {
        LOG_DEBUG("Creative menu opened");
    } else {
        LOG_DEBUG("Creative menu closed");
        searchFilter.clear();
    }
}

void CreativeMenu::render() {
    if (!isOpen) {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();

    // Centered box (like Minecraft) - 70% of screen size
    float width = io.DisplaySize.x * 0.7f;
    float height = io.DisplaySize.y * 0.7f;
    float posX = (io.DisplaySize.x - width) * 0.5f;
    float posY = (io.DisplaySize.y - height) * 0.5f;

    ImGui::SetNextWindowPos(ImVec2(posX, posY));
    ImGui::SetNextWindowSize(ImVec2(width, height));
    ImGui::SetNextWindowBgAlpha(0.95f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize |
                            ImGuiWindowFlags_NoMove |
                            ImGuiWindowFlags_NoCollapse |
                            ImGuiWindowFlags_NoSavedSettings;

    if (ImGui::Begin("Creative Inventory", &isOpen, flags)) {
        renderSearchBar();
        ImGui::Separator();

        renderItemGrid();
    }
    ImGui::End();
}

void CreativeMenu::renderSearchBar() {
    ImGui::Text("Search:");
    ImGui::SameLine();

    char buffer[256];
    std::strncpy(buffer, searchFilter.c_str(), sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    if (ImGui::InputText("##search", buffer, sizeof(buffer))) {
        searchFilter = buffer;
    }
}

void CreativeMenu::renderItemGrid() {
    // Get all available blocks
    std::vector<ItemType> allBlocks = ItemRegistry::instance().getAllBlocks();

    // Filter by search if needed
    std::vector<ItemType> filteredItems;
    if (searchFilter.empty()) {
        filteredItems = allBlocks;
    } else {
        // Case-insensitive search
        std::string lowerSearch = searchFilter;
        std::transform(lowerSearch.begin(), lowerSearch.end(), lowerSearch.begin(),
                      [](unsigned char c) { return std::tolower(c); });

        for (ItemType type : allBlocks) {
            const ItemProperties* props = ItemRegistry::instance().getItem(type);
            if (props) {
                std::string lowerName = props->name;
                std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
                             [](unsigned char c) { return std::tolower(c); });

                if (lowerName.find(lowerSearch) != std::string::npos) {
                    filteredItems.push_back(type);
                }
            }
        }
    }

    // Render items in a grid with automatic wrapping
    ImGui::BeginChild("ItemGrid", ImVec2(0, 0), true);

    // Calculate how many items can fit per row based on window width
    float windowWidth = ImGui::GetContentRegionAvail().x;
    int itemsPerRow = static_cast<int>((windowWidth + ITEM_PADDING) / (ITEM_BUTTON_SIZE + ITEM_PADDING));
    if (itemsPerRow < 1) itemsPerRow = 1;

    int itemsInRow = 0;
    for (ItemType type : filteredItems) {
        if (itemsInRow > 0) {
            ImGui::SameLine(0.0f, ITEM_PADDING);
        }

        if (renderItemButton(type)) {
            // Item was clicked - add to inventory
            const ItemProperties* props = ItemRegistry::instance().getItem(type);
            if (props) {
                bool success = inventory->addItem(type, 64);  // Give full stack in creative mode
                if (success) {
                    LOG_INFO("Added 64x {} to inventory (first empty slot)", props->displayName);
                } else {
                    LOG_WARN("Failed to add {} - inventory full?", props->displayName);
                }

                // Also try setting to first hotbar slot if it's empty
                if (inventory->getHotbarSlot(0).isEmpty()) {
                    inventory->setSlot(0, type, 64);
                    LOG_INFO("Set hotbar slot 1 to 64x {}", props->displayName);
                }
            }
        }

        itemsInRow++;
        if (itemsInRow >= itemsPerRow) {
            itemsInRow = 0;
        }
    }

    ImGui::EndChild();
}

bool CreativeMenu::renderItemButton(ItemType itemType) {
    const ItemProperties* props = ItemRegistry::instance().getItem(itemType);
    if (!props) {
        return false;
    }

    // Draw button
    bool clicked = ImGui::Button(("##item_" + props->name).c_str(),
                                 ImVec2(ITEM_BUTTON_SIZE, ITEM_BUTTON_SIZE));

    // Draw block texture on the button if available
    auto textureIt = blockTextures.find(itemType);
    if (textureIt != blockTextures.end()) {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 buttonMin = ImGui::GetItemRectMin();
        ImVec2 buttonMax = ImGui::GetItemRectMax();

        // Draw texture with small padding
        const float iconPadding = 4.0f;
        ImVec2 iconMin = ImVec2(buttonMin.x + iconPadding, buttonMin.y + iconPadding);
        ImVec2 iconMax = ImVec2(buttonMax.x - iconPadding, buttonMax.y - iconPadding);
        drawList->AddImage((ImTextureID)textureIt->second, iconMin, iconMax);
    }

    // Show tooltip on hover
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", props->displayName.c_str());
    }

    return clicked;
}

void CreativeMenu::loadBlockTexture(ItemType itemType, const std::string& texturePath) {
    // Load image using stb_image
    int width, height, channels;
    unsigned char* pixels = stbi_load(texturePath.c_str(), &width, &height, &channels, STBI_rgb_alpha);

    if (!pixels) {
        LOG_ERROR("Failed to load texture: {}", texturePath);
        return;
    }

    VkDeviceSize imageSize = width * height * 4;  // RGBA

    // Create staging buffer
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = imageSize;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vkCreateBuffer(device, &bufferInfo, nullptr, &stagingBuffer);

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, stagingBuffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits,
                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    vkAllocateMemory(device, &allocInfo, nullptr, &stagingBufferMemory);
    vkBindBufferMemory(device, stagingBuffer, stagingBufferMemory, 0);

    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
    std::memcpy(data, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(device, stagingBufferMemory);

    stbi_image_free(pixels);

    // Create image
    TextureData texData;

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
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vkCreateImage(device, &imageInfo, nullptr, &texData.image);

    vkGetImageMemoryRequirements(device, texData.image, &memRequirements);

    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    vkAllocateMemory(device, &allocInfo, nullptr, &texData.memory);
    vkBindImageMemory(device, texData.image, texData.memory, 0);

    // Transition and copy using command buffer
    VkCommandBufferAllocateInfo cmdAllocInfo{};
    cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAllocInfo.commandPool = commandPool;
    cmdAllocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &cmdAllocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    // Transition to TRANSFER_DST
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = texData.image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                        0, 0, nullptr, 0, nullptr, 1, &barrier);

    // Copy buffer to image
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};

    vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, texData.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    // Transition to SHADER_READ_ONLY
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                        0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);

    // Create image view
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = texData.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    vkCreateImageView(device, &viewInfo, nullptr, &texData.view);

    // Create sampler
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_NEAREST;  // Pixelated look for blocks
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;

    vkCreateSampler(device, &samplerInfo, nullptr, &texData.sampler);

    // Add descriptor set to ImGui
    VkDescriptorSet descriptorSet = ImGui_ImplVulkan_AddTexture(texData.sampler, texData.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // Store resources
    textureResources[itemType] = texData;
    blockTextures[itemType] = descriptorSet;

    LOG_TRACE("Loaded block texture for item type {}: {}", static_cast<int>(itemType), texturePath);
}

void CreativeMenu::cleanupTextures() {
    for (auto& [itemType, texData] : textureResources) {
        if (texData.sampler != VK_NULL_HANDLE) {
            vkDestroySampler(device, texData.sampler, nullptr);
        }
        if (texData.view != VK_NULL_HANDLE) {
            vkDestroyImageView(device, texData.view, nullptr);
        }
        if (texData.image != VK_NULL_HANDLE) {
            vkDestroyImage(device, texData.image, nullptr);
        }
        if (texData.memory != VK_NULL_HANDLE) {
            vkFreeMemory(device, texData.memory, nullptr);
        }
    }
    textureResources.clear();
    blockTextures.clear();
}

uint32_t CreativeMenu::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type");
}

} // namespace engine
