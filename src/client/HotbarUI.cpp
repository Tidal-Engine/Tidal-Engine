#include "client/HotbarUI.hpp"
#include "client/Inventory.hpp"
#include "client/InputManager.hpp"
#include "client/ItemRegistry.hpp"
#include "core/Logger.hpp"
#include <SDL3/SDL.h>
#include <backends/imgui_impl_vulkan.h>
#include "../../external/stb/stb_image.h"
#include <stdexcept>

namespace engine {

HotbarUI::HotbarUI(Inventory* inventory, VkDevice device, VkPhysicalDevice physicalDevice,
                   VkCommandPool commandPool, VkQueue graphicsQueue)
    : inventory(inventory), device(device), physicalDevice(physicalDevice),
      commandPool(commandPool), graphicsQueue(graphicsQueue) {
}

HotbarUI::~HotbarUI() {
    cleanupTextures();
}

void HotbarUI::init() {
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

    LOG_INFO("HotbarUI textures loaded");
}

void HotbarUI::render() {
    ImGuiIO& imguiIo = ImGui::GetIO();
    ImVec2 screenSize = imguiIo.DisplaySize;

    // Calculate hotbar dimensions
    float totalWidth = ((SLOT_SIZE + SLOT_PADDING) * Inventory::HOTBAR_SIZE) - SLOT_PADDING;
    float startX = (screenSize.x - totalWidth) * 0.5f;
    float startY = screenSize.y - SLOT_SIZE - HOTBAR_Y_OFFSET;

    // Create invisible window for hotbar
    ImGui::SetNextWindowPos(ImVec2(startX, startY));
    ImGui::SetNextWindowSize(ImVec2(totalWidth, SLOT_SIZE + 30.0f));
    ImGui::SetNextWindowBgAlpha(0.0f);  // Fully transparent background

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
                            ImGuiWindowFlags_NoResize |
                            ImGuiWindowFlags_NoMove |
                            ImGuiWindowFlags_NoScrollbar |
                            ImGuiWindowFlags_NoSavedSettings |
                            ImGuiWindowFlags_NoFocusOnAppearing |
                            ImGuiWindowFlags_NoBringToFrontOnFocus |
                            ImGuiWindowFlags_NoInputs;

    if (ImGui::Begin("##Hotbar", nullptr, flags)) {
        for (size_t i = 0; i < Inventory::HOTBAR_SIZE; i++) {
            if (i > 0) {
                ImGui::SameLine(0.0f, SLOT_PADDING);
            }
            renderSlot(i, i == inventory->getSelectedHotbarIndex());
        }
    }
    ImGui::End();

    ImGui::PopStyleVar(2);
}

void HotbarUI::renderSlot(size_t index, bool selected) {
    const ItemStack& stack = inventory->getHotbarSlot(index);

    // Draw slot background
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();

    // Background color
    ImU32 bgColor = selected ? IM_COL32(100, 100, 100, 200) : IM_COL32(50, 50, 50, 200);
    drawList->AddRectFilled(pos, ImVec2(pos.x + SLOT_SIZE, pos.y + SLOT_SIZE), bgColor);

    // Border
    ImU32 borderColor = selected ? IM_COL32(255, 255, 255, 255) : IM_COL32(150, 150, 150, 255);
    float borderThickness = selected ? 3.0f : 1.0f;
    drawList->AddRect(pos, ImVec2(pos.x + SLOT_SIZE, pos.y + SLOT_SIZE), borderColor, 0.0f, 0, borderThickness);

    // Draw item icon if slot is not empty
    if (!stack.isEmpty()) {
        auto textureIt = blockTextures.find(stack.type);
        if (textureIt != blockTextures.end()) {
            // Draw block texture (with small padding)
            constexpr float ICON_PADDING = 4.0f;
            ImVec2 iconMin = ImVec2(pos.x + ICON_PADDING, pos.y + ICON_PADDING);
            ImVec2 iconMax = ImVec2(pos.x + SLOT_SIZE - ICON_PADDING, pos.y + SLOT_SIZE - ICON_PADDING);
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
            drawList->AddImage(reinterpret_cast<ImTextureID>(textureIt->second), iconMin, iconMax);
        }
    }

    // Draw slot number (top-left corner)
    std::string slotNum = std::to_string(index + 1);
    drawList->AddText(ImVec2(pos.x + 4.0f, pos.y + 2.0f), IM_COL32(200, 200, 200, 200), slotNum.c_str());

    // Invisible button for spacing
    ImGui::InvisibleButton(("##slot" + std::to_string(index)).c_str(), ImVec2(SLOT_SIZE, SLOT_SIZE));
}

void HotbarUI::handleInput(const InputManager* input) {
    // Number keys 1-9 to select hotbar slots
    for (size_t i = 0; i < Inventory::HOTBAR_SIZE; i++) {
        SDL_Scancode key = static_cast<SDL_Scancode>(SDL_SCANCODE_1 + i);
        if (input->isKeyJustPressed(key)) {
            inventory->setSelectedHotbarIndex(i);
            break;
        }
    }

    // Mouse scroll wheel to cycle through hotbar
    float wheelDelta = input->getMouseWheelDelta();
    if (wheelDelta != 0.0f) {
        int currentSlot = static_cast<int>(inventory->getSelectedHotbarIndex());

        // Scroll down = next slot, scroll up = previous slot
        currentSlot -= static_cast<int>(wheelDelta);

        // Wrap around
        if (currentSlot < 0) {
            currentSlot = static_cast<int>(Inventory::HOTBAR_SIZE) - 1;
        } else if (currentSlot >= static_cast<int>(Inventory::HOTBAR_SIZE)) {
            currentSlot = 0;
        }

        inventory->setSelectedHotbarIndex(static_cast<size_t>(currentSlot));
    }
}

void HotbarUI::loadBlockTexture(ItemType itemType, const std::string& texturePath) {
    // Load image using stb_image
    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* pixels = stbi_load(texturePath.c_str(), &width, &height, &channels, STBI_rgb_alpha);

    if (!pixels) {
        LOG_ERROR("Failed to load texture: {}", texturePath);
        return;
    }

    VkDeviceSize imageSize = static_cast<VkDeviceSize>(width) * static_cast<VkDeviceSize>(height) * 4;  // RGBA

    // Create staging buffer
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingBufferMemory = VK_NULL_HANDLE;

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

    void* data = nullptr;
    vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, pixels, static_cast<size_t>(imageSize));
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

    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
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

void HotbarUI::cleanupTextures() {
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

uint32_t HotbarUI::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
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
