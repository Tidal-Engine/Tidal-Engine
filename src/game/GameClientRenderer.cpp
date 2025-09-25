#include "game/GameClientRenderer.h"
#include "game/GameClient.h"
#include "game/GameClientUI.h"
#include "game/Chunk.h" // For ChunkVertex

#include <stdexcept>
#include <fstream>
#include <array>
#include <iostream>
#include <cstring>

// Static helper function
static std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file: " + filename);
    }

    size_t fileSize = (size_t) file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

GameClientRenderer::GameClientRenderer(GameClient& gameClient, GLFWwindow* window, VulkanDevice& device)
    : m_gameClient(gameClient), m_window(window), m_device(device) {
    // Get vertices and indices from GameClient
    // These will need to be passed in or accessed differently after refactoring
}

GameClientRenderer::~GameClientRenderer() {
    shutdown();
}

bool GameClientRenderer::initialize() {
    try {
        // Create Vulkan renderer
        m_renderer = std::make_unique<VulkanRenderer>(m_window, m_device);

        // Get texture manager from GameClient
        m_textureManager = m_gameClient.m_textureManager.get();

        // Initialize vertex data (temporary - should be passed in)
        // Static vertex data for basic cube rendering
        static const std::vector<Vertex> vertices = {
            // Front face (+Z) - CCW from outside: (-0.5,-0.5,0.5)→(0.5,-0.5,0.5)→(0.5,0.5,0.5)→(-0.5,0.5,0.5)
            {{-0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
            {{ 0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
            {{ 0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
            {{-0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},

            // Back face (-Z) - CCW from outside: (0.5,-0.5,-0.5)→(-0.5,-0.5,-0.5)→(-0.5,0.5,-0.5)→(0.5,0.5,-0.5)
            {{ 0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}},
            {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}},
            {{-0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}},
            {{ 0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}},

            // Additional faces would go here...
        };

        static const std::vector<uint32_t> indices = {
            0, 1, 2, 2, 3, 0,  // Front face
            4, 5, 6, 6, 7, 4,  // Back face
            // Additional indices would go here...
        };

        m_vertices = vertices;
        m_indices = indices;

        // Initialize rendering pipeline
        createRenderPass();
        createDescriptorSetLayout();
        createGraphicsPipeline();
        createWireframePipeline();
        createFramebuffers();
        createCommandPool();
        createVertexBuffer();
        createIndexBuffer();
        createUniformBuffers();
        createDescriptorPool();
        createDescriptorSets();
        createCommandBuffers();

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize GameClientRenderer: " << e.what() << std::endl;
        return false;
    }
}

void GameClientRenderer::shutdown() {
    cleanupVulkan();
}

void GameClientRenderer::drawFrame() {
    if (!m_renderer) return;

    // Begin frame
    VkCommandBuffer commandBuffer = m_renderer->beginFrame();
    if (commandBuffer == VK_NULL_HANDLE) {
        // Frame couldn't be started (e.g., during resize), ensure frame flag is cleared
        if (m_gameClient.m_chunkManager) {
            m_gameClient.m_chunkManager->setFrameInProgress(false);
        }
        return; // Skip frame
    }

    // Mark frame as in progress to defer mesh updates
    if (m_gameClient.m_chunkManager) {
        m_gameClient.m_chunkManager->setFrameInProgress(true);
    }

    // Begin render pass with a clear screen
    m_renderer->beginSwapchainRenderPass(commandBuffer);

    // Render world geometry if in game mode
    if (m_gameClient.getGameState() == GameState::IN_GAME && m_gameClient.m_chunkManager) {
        // Update uniform buffer for this frame
        int frameIndex = m_renderer->getFrameIndex();
        updateUniformBuffer(frameIndex);

        // Bind the graphics pipeline and descriptor sets
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSets[frameIndex], 0, nullptr);

        // Set up lighting push constants
        PushConstants pushConstants{
            .lightPos = glm::vec3(50.0f, 100.0f, 50.0f),
            .lightColor = glm::vec3(1.0f, 1.0f, 1.0f),
            .viewPos = m_gameClient.m_camera.getPosition(),
            .voxelPosition = glm::vec3(0.0f),
            .voxelColor = glm::vec3(1.0f)
        };
        vkCmdPushConstants(commandBuffer, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pushConstants);

        // Calculate frustum for culling
        glm::mat4 viewMatrix = m_gameClient.m_camera.getViewMatrix();
        glm::mat4 projMatrix = glm::perspective(glm::radians(m_gameClient.m_camera.getZoom()), m_renderer->getAspectRatio(), 0.1f, 1000.0f);
        projMatrix[1][1] *= -1; // Flip Y coordinate for Vulkan
        Frustum frustum = m_gameClient.m_camera.calculateFrustum(projMatrix, viewMatrix);

        // Render the voxel world with frustum culling
        size_t renderedChunks = m_gameClient.m_chunkManager->renderVisibleChunks(commandBuffer, frustum);

        // Update debug info with rendering statistics
        static int debugUpdateCounter = 0;
        if ((m_gameClient.getUI() && m_gameClient.getUI()->isShowingDebugWindow()) ||
            (m_gameClient.getUI() && m_gameClient.getUI()->isShowingRenderingHUD())) {
            if (++debugUpdateCounter % 10 == 0) { // Only update every 10 frames
                size_t totalLoadedChunks = m_gameClient.m_chunkManager->getLoadedChunkCount();
                size_t totalFaces = m_gameClient.m_chunkManager->getTotalFaceCount();
                size_t totalVoxels = totalLoadedChunks * CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;
                size_t culledChunks = totalLoadedChunks - renderedChunks;

                m_gameClient.m_debugInfo.drawCalls = static_cast<uint32_t>(renderedChunks);
                m_gameClient.m_debugInfo.totalVoxels = static_cast<uint32_t>(totalVoxels);
                m_gameClient.m_debugInfo.renderedVoxels = static_cast<uint32_t>(totalFaces);
                m_gameClient.m_debugInfo.culledVoxels = static_cast<uint32_t>(culledChunks * CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE);
            }
        } else {
            debugUpdateCounter = 0;
        }

        // Render chunk boundaries if debug mode is enabled
        if (m_gameClient.getUI() && m_gameClient.getUI()->isShowingChunkBoundaries()) {
            m_gameClient.m_chunkManager->renderChunkBoundaries(commandBuffer, m_wireframePipeline, m_gameClient.m_camera.getPosition(), m_pipelineLayout, m_descriptorSets[frameIndex]);
        }
    }

    // Render ImGui UI (works for all game states)
    if (m_gameClient.getUI()) {
        m_gameClient.getUI()->renderImGui(commandBuffer);
    }

    // End render pass and frame
    m_renderer->endSwapchainRenderPass(commandBuffer);
    m_renderer->endFrame();

    // Mark frame as complete - safe to process deferred mesh updates
    if (m_gameClient.m_chunkManager) {
        m_gameClient.m_chunkManager->setFrameInProgress(false);
    }
}

void GameClientRenderer::updateUniformBuffer(uint32_t currentImage) {
    UniformBufferObject ubo{};
    ubo.model = glm::mat4(1.0f);
    ubo.view = m_gameClient.m_camera.getViewMatrix();
    ubo.proj = glm::perspective(glm::radians(m_gameClient.m_camera.getZoom()), m_renderer->getAspectRatio(), 0.1f, 1000.0f);
    ubo.proj[1][1] *= -1; // Flip Y coordinate for Vulkan
    m_uniformBuffers[currentImage]->writeToBuffer(&ubo);
}

void GameClientRenderer::recreateSwapChain() {
    // This would typically be handled by the VulkanRenderer itself
    if (m_renderer) {
        // Trigger renderer recreation if needed
    }
}

void GameClientRenderer::recreateGraphicsPipeline() {
    vkDeviceWaitIdle(m_device.getDevice());

    if (m_graphicsPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_device.getDevice(), m_graphicsPipeline, nullptr);
        m_graphicsPipeline = VK_NULL_HANDLE;
    }

    if (m_wireframePipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_device.getDevice(), m_wireframePipeline, nullptr);
        m_wireframePipeline = VK_NULL_HANDLE;
    }

    createGraphicsPipeline();
    createWireframePipeline();
}

void GameClientRenderer::createRenderPass() {
    // Use the renderer's render pass for now
    m_renderPass = m_renderer->getSwapChainRenderPass();
}

void GameClientRenderer::createDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.pImmutableSamplers = nullptr;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutBinding samplerLayoutBinding{};
    samplerLayoutBinding.binding = 1;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.pImmutableSamplers = nullptr;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    std::array<VkDescriptorSetLayoutBinding, 2> bindings = {uboLayoutBinding, samplerLayoutBinding};
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(m_device.getDevice(), &layoutInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout!");
    }
}

void GameClientRenderer::createGraphicsPipeline() {
    auto vertShaderCode = readFile("shaders/compiled/vertex.vert.spv");
    auto fragShaderCode = readFile("shaders/compiled/fragment.frag.spv");

    VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

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

    auto bindingDescription = ChunkVertex::getBindingDescription();
    auto attributeDescriptions = ChunkVertex::getAttributeDescriptions();

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

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = m_wireframeMode ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
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
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(PushConstants);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(m_device.getDevice(), &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create pipeline layout!");
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
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_pipelineLayout;
    pipelineInfo.renderPass = m_renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    if (vkCreateGraphicsPipelines(m_device.getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_graphicsPipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create graphics pipeline!");
    }

    vkDestroyShaderModule(m_device.getDevice(), fragShaderModule, nullptr);
    vkDestroyShaderModule(m_device.getDevice(), vertShaderModule, nullptr);
}

void GameClientRenderer::createWireframePipeline() {
    auto vertShaderCode = readFile("shaders/compiled/vertex.vert.spv");
    auto fragShaderCode = readFile("shaders/compiled/fragment.frag.spv");

    VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

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

    auto bindingDescription = ChunkVertex::getBindingDescription();
    auto attributeDescriptions = ChunkVertex::getAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 2.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_FALSE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

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
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_pipelineLayout;
    pipelineInfo.renderPass = m_renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    if (vkCreateGraphicsPipelines(m_device.getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_wireframePipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create wireframe pipeline!");
    }

    vkDestroyShaderModule(m_device.getDevice(), fragShaderModule, nullptr);
    vkDestroyShaderModule(m_device.getDevice(), vertShaderModule, nullptr);
}

void GameClientRenderer::createFramebuffers() {
    // Framebuffers are created by the renderer
}

void GameClientRenderer::createCommandPool() {
    QueueFamilyIndices queueFamilyIndices = m_device.findPhysicalQueueFamilies();

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

    if (vkCreateCommandPool(m_device.getDevice(), &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create command pool!");
    }
}

void GameClientRenderer::createVertexBuffer() {
    VkDeviceSize bufferSize = sizeof(m_vertices[0]) * m_vertices.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    m_device.createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(m_device.getDevice(), stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, m_vertices.data(), (size_t) bufferSize);
    vkUnmapMemory(m_device.getDevice(), stagingBufferMemory);

    m_vertexBuffer = std::make_unique<VulkanBuffer>(
        m_device,
        sizeof(m_vertices[0]),
        static_cast<uint32_t>(m_vertices.size()),
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    m_device.copyBuffer(stagingBuffer, m_vertexBuffer->getBuffer(), bufferSize);

    vkDestroyBuffer(m_device.getDevice(), stagingBuffer, nullptr);
    vkFreeMemory(m_device.getDevice(), stagingBufferMemory, nullptr);
}

void GameClientRenderer::createIndexBuffer() {
    VkDeviceSize bufferSize = sizeof(m_indices[0]) * m_indices.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    m_device.createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(m_device.getDevice(), stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, m_indices.data(), (size_t) bufferSize);
    vkUnmapMemory(m_device.getDevice(), stagingBufferMemory);

    m_indexBuffer = std::make_unique<VulkanBuffer>(
        m_device,
        sizeof(m_indices[0]),
        static_cast<uint32_t>(m_indices.size()),
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    m_device.copyBuffer(stagingBuffer, m_indexBuffer->getBuffer(), bufferSize);

    vkDestroyBuffer(m_device.getDevice(), stagingBuffer, nullptr);
    vkFreeMemory(m_device.getDevice(), stagingBufferMemory, nullptr);
}

void GameClientRenderer::createUniformBuffers() {
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);

    m_uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        m_uniformBuffers[i] = std::make_unique<VulkanBuffer>(
            m_device,
            bufferSize,
            1,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        m_uniformBuffers[i]->map();
    }
}

void GameClientRenderer::createDescriptorPool() {
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

    if (vkCreateDescriptorPool(m_device.getDevice(), &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool!");
    }
}

void GameClientRenderer::createDescriptorSets() {
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, m_descriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    allocInfo.pSetLayouts = layouts.data();

    m_descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    if (vkAllocateDescriptorSets(m_device.getDevice(), &allocInfo, m_descriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = m_uniformBuffers[i]->getBuffer();
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);

        // Bind texture atlas from TextureManager
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = m_textureManager->getAtlasImageView();
        imageInfo.sampler = m_textureManager->getAtlasSampler();

        std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = m_descriptorSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &bufferInfo;

        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = m_descriptorSets[i];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(m_device.getDevice(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }
}

void GameClientRenderer::createCommandBuffers() {
    m_commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_commandPool;
    allocInfo.commandBufferCount = static_cast<uint32_t>(m_commandBuffers.size());

    if (vkAllocateCommandBuffers(m_device.getDevice(), &allocInfo, m_commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers!");
    }
}

VkShaderModule GameClientRenderer::createShaderModule(const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(m_device.getDevice(), &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("failed to create shader module!");
    }

    return shaderModule;
}

void GameClientRenderer::cleanupVulkan() {
    std::cout << "Starting GameClientRenderer Vulkan cleanup..." << std::endl;

    // Cleanup Vulkan resources
    if (m_device.getDevice() == VK_NULL_HANDLE) {
        std::cout << "No device to cleanup, exiting." << std::endl;
        return; // Nothing to cleanup
    }

    // Store device handle before we potentially reset the device wrapper
    VkDevice device = m_device.getDevice();

    if (device != VK_NULL_HANDLE) {
        std::cout << "Waiting for device idle..." << std::endl;
        vkDeviceWaitIdle(device);
        std::cout << "Device idle." << std::endl;
    }

    // Clear buffers first
    std::cout << "Clearing buffers..." << std::endl;
    m_uniformBuffers.clear();
    m_indexBuffer.reset();
    m_vertexBuffer.reset();
    std::cout << "Buffers cleared." << std::endl;

    // Destroy Vulkan handles with proper null checking and error handling
    if (device != VK_NULL_HANDLE) {
        try {
            if (m_commandPool != VK_NULL_HANDLE) {
                std::cout << "Destroying command pool..." << std::endl;
                vkDestroyCommandPool(device, m_commandPool, nullptr);
                m_commandPool = VK_NULL_HANDLE;
                std::cout << "Command pool destroyed." << std::endl;
            }
        } catch (...) {
            std::cout << "Error destroying command pool" << std::endl;
        }

        try {
            if (m_descriptorPool != VK_NULL_HANDLE) {
                std::cout << "Destroying descriptor pool..." << std::endl;
                vkDestroyDescriptorPool(device, m_descriptorPool, nullptr);
                m_descriptorPool = VK_NULL_HANDLE;
                std::cout << "Descriptor pool destroyed." << std::endl;
            }
        } catch (...) {
            std::cout << "Error destroying descriptor pool" << std::endl;
        }

        try {
            if (m_descriptorSetLayout != VK_NULL_HANDLE) {
                std::cout << "Destroying descriptor set layout..." << std::endl;
                vkDestroyDescriptorSetLayout(device, m_descriptorSetLayout, nullptr);
                m_descriptorSetLayout = VK_NULL_HANDLE;
                std::cout << "Descriptor set layout destroyed." << std::endl;
            }
        } catch (...) {
            std::cout << "Error destroying descriptor set layout" << std::endl;
        }

        try {
            if (m_graphicsPipeline != VK_NULL_HANDLE) {
                std::cout << "Destroying graphics pipeline..." << std::endl;
                vkDestroyPipeline(device, m_graphicsPipeline, nullptr);
                m_graphicsPipeline = VK_NULL_HANDLE;
                std::cout << "Graphics pipeline destroyed." << std::endl;
            }
        } catch (...) {
            std::cout << "Error destroying graphics pipeline" << std::endl;
        }

        try {
            if (m_wireframePipeline != VK_NULL_HANDLE) {
                std::cout << "Destroying wireframe pipeline..." << std::endl;
                vkDestroyPipeline(device, m_wireframePipeline, nullptr);
                m_wireframePipeline = VK_NULL_HANDLE;
                std::cout << "Wireframe pipeline destroyed." << std::endl;
            }
        } catch (...) {
            std::cout << "Error destroying wireframe pipeline" << std::endl;
        }

        try {
            if (m_pipelineLayout != VK_NULL_HANDLE) {
                std::cout << "Destroying pipeline layout..." << std::endl;
                vkDestroyPipelineLayout(device, m_pipelineLayout, nullptr);
                m_pipelineLayout = VK_NULL_HANDLE;
                std::cout << "Pipeline layout destroyed." << std::endl;
            }
        } catch (...) {
            std::cout << "Error destroying pipeline layout" << std::endl;
        }

        // Skip render pass destruction entirely - it's likely already destroyed by VulkanRenderer
        if (m_renderPass != VK_NULL_HANDLE) {
            std::cout << "Render pass handle present, but skipping destruction (handled by renderer)." << std::endl;
            m_renderPass = VK_NULL_HANDLE;
        }
    }

    std::cout << "GameClientRenderer Vulkan cleanup complete." << std::endl;
}