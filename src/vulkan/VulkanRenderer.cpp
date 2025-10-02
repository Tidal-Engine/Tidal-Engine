#include "vulkan/VulkanRenderer.hpp"
#include "vulkan/VulkanEngine.hpp"
#include "core/Logger.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>
#include <array>
#include <chrono>

namespace engine {

VulkanRenderer::VulkanRenderer(VkDevice device, VkPhysicalDevice physicalDevice,
                               uint32_t graphicsQueueFamily, VkQueue graphicsQueue,
                               VkQueue presentQueue)
    : device(device), physicalDevice(physicalDevice),
      graphicsQueueFamily(graphicsQueueFamily),
      graphicsQueue(graphicsQueue), presentQueue(presentQueue) {
}

void VulkanRenderer::createCommandPool() {
    LOG_DEBUG("Creating command pool");

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = graphicsQueueFamily;

    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        LOG_ERROR("Failed to create command pool");
        throw std::runtime_error("Failed to create command pool");
    }

    LOG_DEBUG("Command pool created");
}

void VulkanRenderer::createDepthResources(VkExtent2D extent) {
    LOG_DEBUG("Creating depth resources");

    VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;

    createImage(extent.width, extent.height, depthFormat,
               VK_IMAGE_TILING_OPTIMAL,
               VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
               depthImage, depthImageMemory);

    depthImageView = createImageView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

    LOG_DEBUG("Depth resources created");
}

void VulkanRenderer::recreateDepthResources(VkExtent2D extent) {
    LOG_DEBUG("Recreating depth resources");

    // Clean up old depth resources
    if (depthImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, depthImageView, nullptr);
    }
    if (depthImage != VK_NULL_HANDLE) {
        vkDestroyImage(device, depthImage, nullptr);
        vkFreeMemory(device, depthImageMemory, nullptr);
    }

    // Create new depth resources
    createDepthResources(extent);

    LOG_DEBUG("Depth resources recreated");
}

void VulkanRenderer::createCommandBuffers(uint32_t count) {
    LOG_DEBUG("Creating {} command buffers", count);

    commandBuffers.resize(count);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

    if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
        LOG_ERROR("Failed to allocate command buffers");
        throw std::runtime_error("Failed to allocate command buffers");
    }

    LOG_DEBUG("Command buffers created");
}

void VulkanRenderer::createSyncObjects(uint32_t maxFramesInFlight) {
    LOG_DEBUG("Creating synchronization objects");

    imageAvailableSemaphores.resize(maxFramesInFlight);
    renderFinishedSemaphores.resize(maxFramesInFlight);
    inFlightFences.resize(maxFramesInFlight);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < maxFramesInFlight; i++) {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
            LOG_ERROR("Failed to create synchronization objects");
            throw std::runtime_error("Failed to create synchronization objects");
        }
    }

    LOG_DEBUG("Synchronization objects created");
}

void VulkanRenderer::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex,
                                        VkRenderPass renderPass, const std::vector<VkFramebuffer>& framebuffers,
                                        VkExtent2D extent, VkPipeline pipeline, VkPipelineLayout pipelineLayout,
                                        VkBuffer vertexBuffer, VkBuffer indexBuffer, uint32_t indexCount,
                                        const std::vector<VkDescriptorSet>& descriptorSets) const {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        LOG_ERROR("Failed to begin recording command buffer");
        throw std::runtime_error("Failed to begin recording command buffer");
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = framebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = extent;

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    VkBuffer vertexBuffers[] = {vertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[currentFrame], 0, nullptr);

    vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);

    vkCmdEndRenderPass(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        LOG_ERROR("Failed to record command buffer");
        throw std::runtime_error("Failed to record command buffer");
    }
}

bool VulkanRenderer::drawFrame(VkSwapchainKHR swapchain, const std::vector<VkFramebuffer>& framebuffers,
                              VkRenderPass renderPass, VkExtent2D extent,
                              VkPipeline pipeline, VkPipelineLayout pipelineLayout,
                              VkBuffer vertexBuffer, VkBuffer indexBuffer, uint32_t indexCount,
                              const std::vector<VkDescriptorSet>& descriptorSets,
                              const std::vector<void*>& uniformBuffersMapped,
                              uint32_t maxFramesInFlight) {
    if (vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
        LOG_ERROR("Failed to wait for fence");
        throw std::runtime_error("Failed to wait for fence");
    }

    uint32_t imageIndex = 0;
    VkResult result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX,
                                           imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        return true; // Swapchain needs recreation
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        LOG_ERROR("Failed to acquire swap chain image");
        throw std::runtime_error("Failed to acquire swap chain image");
    }

    if (vkResetFences(device, 1, &inFlightFences[currentFrame]) != VK_SUCCESS) {
        LOG_ERROR("Failed to reset fence");
        throw std::runtime_error("Failed to reset fence");
    }

    updateUniformBuffer(uniformBuffersMapped[currentFrame]);

    if (vkResetCommandBuffer(commandBuffers[currentFrame], 0) != VK_SUCCESS) {
        LOG_ERROR("Failed to reset command buffer");
        throw std::runtime_error("Failed to reset command buffer");
    }
    recordCommandBuffer(commandBuffers[currentFrame], imageIndex, renderPass, framebuffers,
                       extent, pipeline, pipelineLayout, vertexBuffer, indexBuffer, indexCount,
                       descriptorSets);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers[currentFrame];

    VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
        LOG_ERROR("Failed to submit draw command buffer");
        throw std::runtime_error("Failed to submit draw command buffer");
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = {swapchain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(presentQueue, &presentInfo);

    currentFrame = (currentFrame + 1) % maxFramesInFlight;

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        return true; // Swapchain needs recreation
    }
    if (result != VK_SUCCESS) {
        LOG_ERROR("Failed to present swap chain image");
        throw std::runtime_error("Failed to present swap chain image");
    }

    return false; // Swapchain is fine
}

void VulkanRenderer::updateUniformBuffer(void* uniformBufferMapped) {  // NOLINT(readability-convert-member-functions-to-static)
    static auto startTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    UniformBufferObject ubo{};
    ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    ubo.proj = glm::perspective(glm::radians(45.0f), 800.0f / 600.0f, 0.1f, 10.0f);
    ubo.proj[1][1] *= -1; // GLM was designed for OpenGL, flip Y for Vulkan

    ubo.lightPos = glm::vec4(2.0f, 2.0f, 2.0f, 1.0f);
    ubo.viewPos = glm::vec4(2.0f, 2.0f, 2.0f, 1.0f);

    memcpy(uniformBufferMapped, &ubo, sizeof(ubo));
}

void VulkanRenderer::waitIdle() {
    if (vkDeviceWaitIdle(device) != VK_SUCCESS) {
        LOG_WARN("Failed to wait for device idle");
    }
}

void VulkanRenderer::cleanup() {
    LOG_DEBUG("Cleaning up renderer");

    for (size_t i = 0; i < imageAvailableSemaphores.size(); i++) {
        vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
        vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
        vkDestroyFence(device, inFlightFences[i], nullptr);
    }

    if (depthImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, depthImageView, nullptr);
    }

    if (depthImage != VK_NULL_HANDLE) {
        vkDestroyImage(device, depthImage, nullptr);
        vkFreeMemory(device, depthImageMemory, nullptr);
    }

    if (commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device, commandPool, nullptr);
    }
}

void VulkanRenderer::createImage(uint32_t width, uint32_t height, VkFormat format,
                                VkImageTiling tiling, VkImageUsageFlags usage,
                                VkMemoryPropertyFlags properties, VkImage& image,
                                VkDeviceMemory& imageMemory) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        LOG_ERROR("Failed to create image");
        throw std::runtime_error("Failed to create image");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        LOG_ERROR("Failed to allocate image memory");
        throw std::runtime_error("Failed to allocate image memory");
    }

    if (vkBindImageMemory(device, image, imageMemory, 0) != VK_SUCCESS) {
        vkDestroyImage(device, image, nullptr);
        vkFreeMemory(device, imageMemory, nullptr);
        LOG_ERROR("Failed to bind image memory");
        throw std::runtime_error("Failed to bind image memory");
    }
}

VkImageView VulkanRenderer::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView = VK_NULL_HANDLE;
    if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        LOG_ERROR("Failed to create image view");
        throw std::runtime_error("Failed to create image view");
    }

    return imageView;
}

uint32_t VulkanRenderer::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    LOG_ERROR("Failed to find suitable memory type");
    throw std::runtime_error("Failed to find suitable memory type");
}

} // namespace engine
