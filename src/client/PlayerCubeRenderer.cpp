#include "client/PlayerCubeRenderer.hpp"
#include "vulkan/Vertex.hpp"
#include "vulkan/VulkanBuffer.hpp"
#include "core/Logger.hpp"
#include "core/ResourceManager.hpp"

#include "../../external/stb/stb_image.h"

#include <cstring>
#include <cmath>
#include <stdexcept>
#include <fstream>
#include <chrono>

namespace engine {

PlayerCubeRenderer::PlayerCubeRenderer(VkDevice device, VkPhysicalDevice physicalDevice,
                                      VkCommandPool commandPool, VkQueue graphicsQueue)
    : device(device), physicalDevice(physicalDevice),
      commandPool(commandPool), graphicsQueue(graphicsQueue) {
}

PlayerCubeRenderer::~PlayerCubeRenderer() {
    cleanup();
}

void PlayerCubeRenderer::init(VkRenderPass renderPass, VkExtent2D swapchainExtent,
                             VkDescriptorSetLayout uboDescriptorSetLayout) {
    // Register shaders
    ResourceManager::registerShader("player_cube_vert", "shaders/player_cube_vert.spv");
    ResourceManager::registerShader("player_cube_frag", "shaders/player_cube_frag.spv");

    loadFaceTexture();
    createTextureDescriptors();
    createBuffers();
    createPipeline(renderPass, swapchainExtent, uboDescriptorSetLayout);
    LOG_DEBUG("PlayerCubeRenderer initialized");
}

void PlayerCubeRenderer::draw(VkCommandBuffer commandBuffer, VkDescriptorSet uboDescriptorSet) {
    if (cubes.empty()) {
        return;
    }

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    // Bind both descriptor sets: UBO at set 0, texture at set 1
    VkDescriptorSet descriptorSets[] = {uboDescriptorSet, textureDescriptorSet};
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                           pipelineLayout, 0, 2, descriptorSets, 0, nullptr);

    VkBuffer vertexBuffers[] = {vertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    // Draw each player cube
    for (const auto& cube : cubes) {
        // Push constants for position, color, and rotation
        struct PushConstants {
            glm::vec3 position;
            float yaw;
            glm::vec3 color;
            float pitch;
        } pushConstants{};

        pushConstants.position = cube.position;
        pushConstants.yaw = cube.yaw;
        pushConstants.color = cube.color;
        pushConstants.pitch = cube.pitch;

        vkCmdPushConstants(commandBuffer, pipelineLayout,
                          VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                          0, sizeof(PushConstants), &pushConstants);

        vkCmdDrawIndexed(commandBuffer, 36, 1, 0, 0, 0);  // 36 indices for a cube
    }
}

void PlayerCubeRenderer::recreatePipeline(VkRenderPass renderPass, VkExtent2D swapchainExtent,
                                          VkDescriptorSetLayout descriptorSetLayout) {
    // Destroy old pipeline
    if (pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, pipeline, nullptr);
        pipeline = VK_NULL_HANDLE;
    }
    if (pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        pipelineLayout = VK_NULL_HANDLE;
    }

    // Recreate with new extent
    createPipeline(renderPass, swapchainExtent, descriptorSetLayout);
}

void PlayerCubeRenderer::cleanup() {
    if (descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        descriptorPool = VK_NULL_HANDLE;
    }
    if (textureDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, textureDescriptorSetLayout, nullptr);
        textureDescriptorSetLayout = VK_NULL_HANDLE;
    }
    if (faceTextureSampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, faceTextureSampler, nullptr);
        faceTextureSampler = VK_NULL_HANDLE;
    }
    if (faceTextureImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, faceTextureImageView, nullptr);
        faceTextureImageView = VK_NULL_HANDLE;
    }
    if (faceTextureImage != VK_NULL_HANDLE) {
        vkDestroyImage(device, faceTextureImage, nullptr);
        faceTextureImage = VK_NULL_HANDLE;
    }
    if (faceTextureMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, faceTextureMemory, nullptr);
        faceTextureMemory = VK_NULL_HANDLE;
    }
    if (vertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, vertexBuffer, nullptr);
        vertexBuffer = VK_NULL_HANDLE;
    }
    if (vertexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, vertexBufferMemory, nullptr);
        vertexBufferMemory = VK_NULL_HANDLE;
    }
    if (indexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, indexBuffer, nullptr);
        indexBuffer = VK_NULL_HANDLE;
    }
    if (indexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, indexBufferMemory, nullptr);
        indexBufferMemory = VK_NULL_HANDLE;
    }
    if (pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, pipeline, nullptr);
        pipeline = VK_NULL_HANDLE;
    }
    if (pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        pipelineLayout = VK_NULL_HANDLE;
    }
    if (vertShaderModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, vertShaderModule, nullptr);
        vertShaderModule = VK_NULL_HANDLE;
    }
    if (fragShaderModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, fragShaderModule, nullptr);
        fragShaderModule = VK_NULL_HANDLE;
    }
}

void PlayerCubeRenderer::createPipeline(VkRenderPass renderPass, VkExtent2D swapchainExtent,
                                       VkDescriptorSetLayout descriptorSetLayout) {
    // Load shaders
    auto vertShaderCode = readFile(ResourceManager::getShaderPath("player_cube_vert"));
    auto fragShaderCode = readFile(ResourceManager::getShaderPath("player_cube_frag"));

    vertShaderModule = createShaderModule(vertShaderCode);
    fragShaderModule = createShaderModule(fragShaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    // Vertex input (using standard Vertex format)
    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapchainExtent.width);
    viewport.height = static_cast<float>(swapchainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapchainExtent;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                         VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    // Push constants for position and color
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(glm::vec3) * 2 + sizeof(float) * 2;

    // Need two descriptor sets: UBO (binding 0) and texture (binding 1)
    VkDescriptorSetLayout setLayouts[] = {descriptorSetLayout, textureDescriptorSetLayout};

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 2;
    pipelineLayoutInfo.pSetLayouts = setLayouts;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create player cube pipeline layout");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create player cube graphics pipeline");
    }
}

void PlayerCubeRenderer::createBuffers() {
    // Create a cube (0.5 unit size for player head)
    // Vertex format: position, color, normal, texCoord, atlasOffset, atlasSize
    std::vector<Vertex> vertices = {
        // Front face (Z+)
        {{-0.25f, -0.25f,  0.25f}, {1.0f, 1.0f, 1.0f}, { 0.0f,  0.0f,  1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 1.0f}},
        {{ 0.25f, -0.25f,  0.25f}, {1.0f, 1.0f, 1.0f}, { 0.0f,  0.0f,  1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 1.0f}},
        {{ 0.25f,  0.25f,  0.25f}, {1.0f, 1.0f, 1.0f}, { 0.0f,  0.0f,  1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 1.0f}},
        {{-0.25f,  0.25f,  0.25f}, {1.0f, 1.0f, 1.0f}, { 0.0f,  0.0f,  1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 1.0f}},

        // Back face (Z-)
        {{ 0.25f, -0.25f, -0.25f}, {1.0f, 1.0f, 1.0f}, { 0.0f,  0.0f, -1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 1.0f}},
        {{-0.25f, -0.25f, -0.25f}, {1.0f, 1.0f, 1.0f}, { 0.0f,  0.0f, -1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 1.0f}},
        {{-0.25f,  0.25f, -0.25f}, {1.0f, 1.0f, 1.0f}, { 0.0f,  0.0f, -1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 1.0f}},
        {{ 0.25f,  0.25f, -0.25f}, {1.0f, 1.0f, 1.0f}, { 0.0f,  0.0f, -1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 1.0f}},

        // Left face (X-)
        {{-0.25f, -0.25f, -0.25f}, {1.0f, 1.0f, 1.0f}, {-1.0f,  0.0f,  0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 1.0f}},
        {{-0.25f, -0.25f,  0.25f}, {1.0f, 1.0f, 1.0f}, {-1.0f,  0.0f,  0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 1.0f}},
        {{-0.25f,  0.25f,  0.25f}, {1.0f, 1.0f, 1.0f}, {-1.0f,  0.0f,  0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 1.0f}},
        {{-0.25f,  0.25f, -0.25f}, {1.0f, 1.0f, 1.0f}, {-1.0f,  0.0f,  0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 1.0f}},

        // Right face (X+)
        {{ 0.25f, -0.25f,  0.25f}, {1.0f, 1.0f, 1.0f}, { 1.0f,  0.0f,  0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 1.0f}},
        {{ 0.25f, -0.25f, -0.25f}, {1.0f, 1.0f, 1.0f}, { 1.0f,  0.0f,  0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 1.0f}},
        {{ 0.25f,  0.25f, -0.25f}, {1.0f, 1.0f, 1.0f}, { 1.0f,  0.0f,  0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 1.0f}},
        {{ 0.25f,  0.25f,  0.25f}, {1.0f, 1.0f, 1.0f}, { 1.0f,  0.0f,  0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 1.0f}},

        // Top face (Y+)
        {{-0.25f,  0.25f,  0.25f}, {1.0f, 1.0f, 1.0f}, { 0.0f,  1.0f,  0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 1.0f}},
        {{ 0.25f,  0.25f,  0.25f}, {1.0f, 1.0f, 1.0f}, { 0.0f,  1.0f,  0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 1.0f}},
        {{ 0.25f,  0.25f, -0.25f}, {1.0f, 1.0f, 1.0f}, { 0.0f,  1.0f,  0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 1.0f}},
        {{-0.25f,  0.25f, -0.25f}, {1.0f, 1.0f, 1.0f}, { 0.0f,  1.0f,  0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 1.0f}},

        // Bottom face (Y-)
        {{-0.25f, -0.25f, -0.25f}, {1.0f, 1.0f, 1.0f}, { 0.0f, -1.0f,  0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 1.0f}},
        {{ 0.25f, -0.25f, -0.25f}, {1.0f, 1.0f, 1.0f}, { 0.0f, -1.0f,  0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 1.0f}},
        {{ 0.25f, -0.25f,  0.25f}, {1.0f, 1.0f, 1.0f}, { 0.0f, -1.0f,  0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 1.0f}},
        {{-0.25f, -0.25f,  0.25f}, {1.0f, 1.0f, 1.0f}, { 0.0f, -1.0f,  0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 1.0f}},
    };

    std::vector<uint32_t> indices = {
        0,  1,  2,  2,  3,  0,   // Front
        4,  5,  6,  6,  7,  4,   // Back
        8,  9,  10, 10, 11, 8,   // Left
        12, 13, 14, 14, 15, 12,  // Right
        16, 17, 18, 18, 19, 16,  // Top
        20, 21, 22, 22, 23, 20,  // Bottom
    };

    // Create vertex buffer
    VkDeviceSize bufferSize = sizeof(Vertex) * vertices.size();

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = bufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &vertexBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create player cube vertex buffer");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, vertexBuffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits,
                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &vertexBufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate player cube vertex buffer memory");
    }

    vkBindBufferMemory(device, vertexBuffer, vertexBufferMemory, 0);

    void* data = nullptr;
    vkMapMemory(device, vertexBufferMemory, 0, bufferSize, 0, &data);
    std::memcpy(data, vertices.data(), bufferSize);
    vkUnmapMemory(device, vertexBufferMemory);

    // Create index buffer
    bufferSize = sizeof(uint32_t) * indices.size();

    bufferInfo.size = bufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &indexBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create player cube index buffer");
    }

    vkGetBufferMemoryRequirements(device, indexBuffer, &memRequirements);

    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits,
                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &indexBufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate player cube index buffer memory");
    }

    vkBindBufferMemory(device, indexBuffer, indexBufferMemory, 0);

    vkMapMemory(device, indexBufferMemory, 0, bufferSize, 0, &data);
    std::memcpy(data, indices.data(), bufferSize);
    vkUnmapMemory(device, indexBufferMemory);
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
std::vector<char> PlayerCubeRenderer::readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        LOG_ERROR("Failed to open shader file: {}", filename);
        throw std::runtime_error("Failed to open shader file: " + filename);
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(fileSize));
    file.close();

    return buffer;
}

VkShaderModule PlayerCubeRenderer::createShaderModule(const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module");
    }

    return shaderModule;
}

uint32_t PlayerCubeRenderer::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type");
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
glm::vec3 PlayerCubeRenderer::getRainbowColor(uint32_t playerId, float time) {
    // Generate rainbow color that cycles over time
    // Start hue based on player ID for uniqueness, then animate
    float baseHue = std::fmod(static_cast<float>(playerId) * 137.508f, 360.0f);  // Golden angle for good distribution
    float hue = std::fmod(baseHue + (time * 60.0f), 360.0f);   // Cycle through rainbow (60 deg/sec)

    // Convert HSV to RGB (S=1, V=1 for vibrant colors)
    float hue60 = hue / 60.0f;
    float chromaX = 1.0f - std::abs(std::fmod(hue60, 2.0f) - 1.0f);

    glm::vec3 rgb;
    if (hue60 < 1.0f) {
        rgb = glm::vec3(1.0f, chromaX, 0.0f);
    } else if (hue60 < 2.0f) {
        rgb = glm::vec3(chromaX, 1.0f, 0.0f);
    } else if (hue60 < 3.0f) {
        rgb = glm::vec3(0.0f, 1.0f, chromaX);
    } else if (hue60 < 4.0f) {
        rgb = glm::vec3(0.0f, chromaX, 1.0f);
    } else if (hue60 < 5.0f) {
        rgb = glm::vec3(chromaX, 0.0f, 1.0f);
    } else {
        rgb = glm::vec3(1.0f, 0.0f, chromaX);
    }

    return rgb;
}

void PlayerCubeRenderer::createTextureDescriptors() {
    // Create descriptor set layout for texture
    VkDescriptorSetLayoutBinding samplerLayoutBinding{};
    samplerLayoutBinding.binding = 1;  // binding = 1 in shader
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.pImmutableSamplers = nullptr;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &samplerLayoutBinding;

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &textureDescriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create texture descriptor set layout");
    }

    // Create descriptor pool
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor pool");
    }

    // Allocate descriptor set
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &textureDescriptorSetLayout;

    if (vkAllocateDescriptorSets(device, &allocInfo, &textureDescriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor set");
    }

    // Update descriptor set with texture
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = faceTextureImageView;
    imageInfo.sampler = faceTextureSampler;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = textureDescriptorSet;
    descriptorWrite.dstBinding = 1;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
}

void PlayerCubeRenderer::loadFaceTexture() {
    std::string facePath = "assets/texturepacks/misc/playerface.png";

    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* pixels = stbi_load(facePath.c_str(), &width, &height, &channels, STBI_rgb_alpha);

    if (!pixels) {
        LOG_ERROR("Failed to load player face texture: {}", facePath);
        throw std::runtime_error("Failed to load player face texture");
    }

    LOG_INFO("Loaded playerface.png: {}x{} with {} channels", width, height, channels);

    createTextureImage(pixels, width, height);
    stbi_image_free(pixels);

    createTextureImageView();
    createTextureSampler();
}

void PlayerCubeRenderer::createTextureImage(const unsigned char* pixels, uint32_t width, uint32_t height) {
    VkDeviceSize imageSize = static_cast<VkDeviceSize>(width) * static_cast<VkDeviceSize>(height) * 4;  // RGBA

    // Create staging buffer
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingBufferMemory = VK_NULL_HANDLE;

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = imageSize;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &stagingBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create staging buffer");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, stagingBuffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits,
                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &stagingBufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate staging buffer memory");
    }

    vkBindBufferMemory(device, stagingBuffer, stagingBufferMemory, 0);

    void* data = nullptr;
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
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device, &imageInfo, nullptr, &faceTextureImage) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create texture image");
    }

    vkGetImageMemoryRequirements(device, faceTextureImage, &memRequirements);

    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &faceTextureMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate texture image memory");
    }

    vkBindImageMemory(device, faceTextureImage, faceTextureMemory, 0);

    // Transition and copy
    transitionImageLayout(faceTextureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copyBufferToImage(stagingBuffer, faceTextureImage, width, height);
    transitionImageLayout(faceTextureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void PlayerCubeRenderer::createTextureImageView() {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = faceTextureImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device, &viewInfo, nullptr, &faceTextureImageView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create texture image view");
    }
}

void PlayerCubeRenderer::createTextureSampler() {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    if (vkCreateSampler(device, &samplerInfo, nullptr, &faceTextureSampler) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create texture sampler");
    }
}

void PlayerCubeRenderer::transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
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

    VkPipelineStageFlags sourceStage = 0;
    VkPipelineStageFlags destinationStage = 0;

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

void PlayerCubeRenderer::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
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

} // namespace engine
