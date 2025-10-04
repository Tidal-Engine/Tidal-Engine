#include "client/BlockOutlineRenderer.hpp"
#include "core/Logger.hpp"
#include "core/ResourceManager.hpp"
#include <array>
#include <fstream>
#include <cstring>

namespace engine {

VkVertexInputBindingDescription LineVertex::getBindingDescription() {
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(LineVertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return bindingDescription;
}

std::array<VkVertexInputAttributeDescription, 2> LineVertex::getAttributeDescriptions() {
    std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};

    // Position
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(LineVertex, position);

    // Color
    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(LineVertex, color);

    return attributeDescriptions;
}

BlockOutlineRenderer::BlockOutlineRenderer(VkDevice device, VkPhysicalDevice physicalDevice,
                                          VkCommandPool commandPool, VkQueue graphicsQueue)
    : device(device), physicalDevice(physicalDevice),
      commandPool(commandPool), graphicsQueue(graphicsQueue) {
}

BlockOutlineRenderer::~BlockOutlineRenderer() {
    cleanup();
}

void BlockOutlineRenderer::init(VkRenderPass renderPass, VkExtent2D swapchainExtent,
                                VkDescriptorSetLayout descriptorSetLayout) {
    LOG_DEBUG("Initializing BlockOutlineRenderer");

    // Register shaders
    ResourceManager::registerShader("line_vert", "shaders/line_vert.spv");
    ResourceManager::registerShader("line_frag", "shaders/line_frag.spv");

    createVertexBuffer();
    createPipeline(renderPass, swapchainExtent, descriptorSetLayout);

    LOG_DEBUG("BlockOutlineRenderer initialized");
}

void BlockOutlineRenderer::update(const std::optional<RaycastHit>& targetedBlock) {
    if (targetedBlock.has_value()) {
        updateVertexBuffer(targetedBlock->blockPos);
        shouldRender = true;
    } else {
        shouldRender = false;
    }
}

void BlockOutlineRenderer::draw(VkCommandBuffer commandBuffer, VkDescriptorSet descriptorSet) {
    if (!shouldRender) {
        return;
    }

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    // Bind descriptor sets (UBO)
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                           pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

    // Bind vertex buffer
    VkBuffer vertexBuffers[] = {vertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

    // Draw 24 vertices as lines (12 edges * 2 vertices)
    vkCmdDraw(commandBuffer, 24, 1, 0, 0);
}

void BlockOutlineRenderer::cleanup() {
    if (vertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, vertexBuffer, nullptr);
        vertexBuffer = VK_NULL_HANDLE;
    }

    if (vertexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, vertexBufferMemory, nullptr);
        vertexBufferMemory = VK_NULL_HANDLE;
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

void BlockOutlineRenderer::createPipeline(VkRenderPass renderPass, VkExtent2D swapchainExtent,
                                         VkDescriptorSetLayout descriptorSetLayout) {
    // Load shaders
    auto vertShaderCode = readFile(ResourceManager::getShaderPath("line_vert"));
    auto fragShaderCode = readFile(ResourceManager::getShaderPath("line_frag"));

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

    // Vertex input
    auto bindingDescription = LineVertex::getBindingDescription();
    auto attributeDescriptions = LineVertex::getAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    // Input assembly - LINE_LIST for wireframe
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // Viewport and scissor
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

    // Rasterizer
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;  // Lines don't use polygonMode
    rasterizer.lineWidth = 1.0f;  // 2.0 requires wideLines GPU feature
    rasterizer.cullMode = VK_CULL_MODE_NONE;  // No culling for lines
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Depth testing
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_FALSE;  // Don't write to depth buffer
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;  // Draw slightly in front
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    // Color blending
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    // Pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        LOG_ERROR("Failed to create line pipeline layout");
        throw std::runtime_error("Failed to create line pipeline layout");
    }

    // Create pipeline
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
        LOG_ERROR("Failed to create line graphics pipeline");
        throw std::runtime_error("Failed to create line graphics pipeline");
    }

    LOG_DEBUG("Line rendering pipeline created");
}

void BlockOutlineRenderer::createVertexBuffer() {
    VkDeviceSize bufferSize = sizeof(LineVertex) * 24;

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = bufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &vertexBuffer) != VK_SUCCESS) {
        LOG_ERROR("Failed to create vertex buffer");
        throw std::runtime_error("Failed to create vertex buffer");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, vertexBuffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits,
                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &vertexBufferMemory) != VK_SUCCESS) {
        LOG_ERROR("Failed to allocate vertex buffer memory");
        throw std::runtime_error("Failed to allocate vertex buffer memory");
    }

    vkBindBufferMemory(device, vertexBuffer, vertexBufferMemory, 0);
}

void BlockOutlineRenderer::updateVertexBuffer(const glm::ivec3& blockPos) {
    constexpr float EXPAND = 0.003f;  // Slight expansion to prevent z-fighting
    glm::vec3 min = glm::vec3(blockPos) - glm::vec3(EXPAND);
    glm::vec3 max = glm::vec3(blockPos) + glm::vec3(1.0f + EXPAND);

    // White color for all vertices
    glm::vec3 color(1.0f, 1.0f, 1.0f);

    // Define 12 edges (24 vertices total)
    // NOLINTBEGIN(cppcoreguidelines-pro-type-union-access)
    std::array<LineVertex, 24> vertices = {{
        // Bottom face (4 edges)
        {{min.x, min.y, min.z}, color}, {{max.x, min.y, min.z}, color},  // Bottom-front
        {{max.x, min.y, min.z}, color}, {{max.x, min.y, max.z}, color},  // Bottom-right
        {{max.x, min.y, max.z}, color}, {{min.x, min.y, max.z}, color},  // Bottom-back
        {{min.x, min.y, max.z}, color}, {{min.x, min.y, min.z}, color},  // Bottom-left

        // Top face (4 edges)
        {{min.x, max.y, min.z}, color}, {{max.x, max.y, min.z}, color},  // Top-front
        {{max.x, max.y, min.z}, color}, {{max.x, max.y, max.z}, color},  // Top-right
        {{max.x, max.y, max.z}, color}, {{min.x, max.y, max.z}, color},  // Top-back
        {{min.x, max.y, max.z}, color}, {{min.x, max.y, min.z}, color},  // Top-left

        // Vertical edges (4 edges)
        {{min.x, min.y, min.z}, color}, {{min.x, max.y, min.z}, color},  // Front-left
        {{max.x, min.y, min.z}, color}, {{max.x, max.y, min.z}, color},  // Front-right
        {{max.x, min.y, max.z}, color}, {{max.x, max.y, max.z}, color},  // Back-right
        {{min.x, min.y, max.z}, color}, {{min.x, max.y, max.z}, color}   // Back-left
    }};
    // NOLINTEND(cppcoreguidelines-pro-type-union-access)

    // Map and copy vertices
    void* data = nullptr;
    vkMapMemory(device, vertexBufferMemory, 0, sizeof(vertices), 0, &data);
    memcpy(data, vertices.data(), sizeof(vertices));
    vkUnmapMemory(device, vertexBufferMemory);
}

std::vector<char> BlockOutlineRenderer::readFile(const std::string& filename) {
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

VkShaderModule BlockOutlineRenderer::createShaderModule(const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data()); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

    VkShaderModule shaderModule = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        LOG_ERROR("Failed to create shader module");
        throw std::runtime_error("Failed to create shader module");
    }

    return shaderModule;
}

uint32_t BlockOutlineRenderer::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    LOG_ERROR("Failed to find suitable memory type");
    throw std::runtime_error("Failed to find suitable memory type");
}

} // namespace engine
