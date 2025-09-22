# Technical Specification: Vulkan Rotating Cube Demo

> Spec: Tidal Engine - Vulkan Rotating Cube Demo
> Created: 2025-09-22
> Status: Ready for Implementation
> Target: C++23, Vulkan 1.3

## Overview

This specification defines the implementation of a Vulkan-based Tidal Engine demo featuring a rotating textured cube. The demo serves as a foundational proof-of-concept for the engine's modern Vulkan rendering capabilities and establishes advanced architecture patterns for high-performance graphics applications.

The deliverable is a cross-platform application that displays a 3D textured cube rotating in real-time within a window, utilizing Vulkan's explicit graphics API for optimal performance, maintaining 60+ FPS with multiple frames in flight.

## Project Structure

### Directory Layout
```
Tidal-Engine/
├── CMakeLists.txt                         # Root CMake configuration
├── vcpkg.json                             # Dependencies manifest
├── src/                                   # Source code
│   ├── main.cpp                           # Application entry point
│   ├── Application.cpp/.h                 # Main application class
│   ├── vulkan/                            # Vulkan abstraction layer
│   │   ├── VulkanDevice.cpp/.h            # Device management
│   │   ├── VulkanRenderer.cpp/.h          # Core rendering system
│   │   ├── VulkanBuffer.cpp/.h            # Buffer abstraction
│   │   ├── VulkanImage.cpp/.h             # Image/texture abstraction
│   │   ├── VulkanPipeline.cpp/.h          # Graphics pipeline
│   │   ├── VulkanSwapchain.cpp/.h         # Swapchain management
│   │   └── VulkanUtils.cpp/.h             # Utility functions
│   ├── Camera.cpp/.h                      # Camera management
│   ├── Mesh.cpp/.h                        # Geometry management
│   └── Window.cpp/.h                      # Window/surface management
├── include/                               # Public headers
│   └── TidalEngine/                       # Engine headers namespace
├── shaders/                               # GLSL shader sources
│   ├── vertex.vert                        # Vertex shader (GLSL)
│   ├── fragment.frag                      # Fragment shader (GLSL)
│   └── compiled/                          # SPIR-V compiled shaders
│       ├── vertex.spv                     # Compiled vertex shader
│       └── fragment.spv                   # Compiled fragment shader
├── assets/                                # Game assets
│   └── textures/                          # Texture files
├── build/                                 # Build output (git ignored)
└── .gitignore                             # Git ignore rules
```

### CMake Build System

#### Root CMakeLists.txt Structure
```cmake
cmake_minimum_required(VERSION 3.25)
project(TidalEngine VERSION 1.0.0 LANGUAGES CXX)

# C++23 Standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find Vulkan SDK
find_package(Vulkan REQUIRED)

# vcpkg integration
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(stb CONFIG REQUIRED)

# Vulkan validation layers setup for debug builds
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(ENABLE_VALIDATION_LAYERS)
endif()

# SPIR-V shader compilation
find_program(GLSL_VALIDATOR glslangValidator HINTS ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE})

function(compile_shader target shader_file)
    get_filename_component(file_name ${shader_file} NAME)
    set(spirv_file "${CMAKE_CURRENT_BINARY_DIR}/shaders/compiled/${file_name}.spv")

    add_custom_command(
        OUTPUT ${spirv_file}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/shaders/compiled/"
        COMMAND ${GLSL_VALIDATOR} -V ${shader_file} -o ${spirv_file}
        DEPENDS ${shader_file}
        COMMENT "Compiling ${file_name} to SPIR-V")

    target_sources(${target} PRIVATE ${spirv_file})
endfunction()

# Executable target with source files
# Shader compilation and copying to build directory
# Asset copying to build directory
```

### Dependencies Management (vcpkg.json)
```json
{
  "name": "tidal-engine",
  "version": "1.0.0",
  "dependencies": [
    "vulkan",
    "glfw3",
    "glm",
    "stb"
  ],
  "builtin-baseline": "latest"
}
```

## Vulkan Core Systems Architecture

### 1. Vulkan Device Management

**Class: `VulkanDevice`**
- **Responsibilities:**
  - Vulkan instance creation with validation layers
  - Physical device selection and capability querying
  - Logical device creation with required queues
  - Extension and feature management

**Key Methods:**
```cpp
class VulkanDevice {
public:
    VulkanDevice(Window& window);
    ~VulkanDevice();

    // Instance management
    void createInstance();
    void setupDebugMessenger();
    void createSurface(GLFWwindow* window);

    // Device selection and creation
    void pickPhysicalDevice();
    void createLogicalDevice();

    // Queue management
    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;
        bool isComplete() const;
    };

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);

    // Getters
    VkDevice getDevice() const { return m_device; }
    VkPhysicalDevice getPhysicalDevice() const { return m_physicalDevice; }
    VkQueue getGraphicsQueue() const { return m_graphicsQueue; }
    VkQueue getPresentQueue() const { return m_presentQueue; }
    VkSurfaceKHR getSurface() const { return m_surface; }

private:
    VkInstance m_instance;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device;
    VkSurfaceKHR m_surface;

    VkQueue m_graphicsQueue;
    VkQueue m_presentQueue;

    VkDebugUtilsMessengerEXT m_debugMessenger;

    const std::vector<const char*> m_validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };

    const std::vector<const char*> m_deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };
};
```

### 2. Vulkan Swapchain Management

**Class: `VulkanSwapchain`**
- **Responsibilities:**
  - Swapchain creation and recreation
  - Image and image view management
  - Presentation format selection
  - Synchronization with surface capabilities

**Key Methods:**
```cpp
class VulkanSwapchain {
public:
    VulkanSwapchain(VulkanDevice& device, VkExtent2D windowExtent);
    ~VulkanSwapchain();

    void createSwapchain();
    void recreateSwapchain();

    VkResult acquireNextImage(uint32_t* imageIndex);
    VkResult submitCommandBuffers(const VkCommandBuffer* buffers, uint32_t* imageIndex);

    VkFormat getSwapchainImageFormat() const { return m_swapchainImageFormat; }
    VkExtent2D getSwapchainExtent() const { return m_swapchainExtent; }
    uint32_t getImageCount() const { return static_cast<uint32_t>(m_swapchainImages.size()); }

private:
    struct SwapchainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    SwapchainSupportDetails querySwapchainSupport(VkPhysicalDevice device);
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

    VulkanDevice& m_device;
    VkExtent2D m_windowExtent;

    VkSwapchainKHR m_swapchain;
    std::vector<VkImage> m_swapchainImages;
    std::vector<VkImageView> m_swapchainImageViews;

    VkFormat m_swapchainImageFormat;
    VkExtent2D m_swapchainExtent;

    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    std::vector<VkFence> m_inFlightFences;

    size_t m_currentFrame = 0;
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
};
```

### 3. Vulkan Buffer Management

**Class: `VulkanBuffer`**
- **Responsibilities:**
  - Buffer creation and memory allocation
  - Memory mapping and data transfer
  - Buffer type abstraction (vertex, index, uniform)

**Key Methods:**
```cpp
class VulkanBuffer {
public:
    VulkanBuffer(
        VulkanDevice& device,
        VkDeviceSize instanceSize,
        uint32_t instanceCount,
        VkBufferUsageFlags usageFlags,
        VkMemoryPropertyFlags memoryPropertyFlags,
        VkDeviceSize minOffsetAlignment = 1);
    ~VulkanBuffer();

    VkResult map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    void unmap();

    void writeToBuffer(void* data, VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    VkResult flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    VkDescriptorBufferInfo descriptorInfo(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);

    VkBuffer getBuffer() const { return m_buffer; }
    void* getMappedMemory() const { return m_mapped; }
    uint32_t getInstanceCount() const { return m_instanceCount; }
    VkDeviceSize getInstanceSize() const { return m_instanceSize; }

private:
    static VkDeviceSize getAlignment(VkDeviceSize instanceSize, VkDeviceSize minOffsetAlignment);

    VulkanDevice& m_device;
    void* m_mapped = nullptr;
    VkBuffer m_buffer = VK_NULL_HANDLE;
    VkDeviceMemory m_memory = VK_NULL_HANDLE;

    VkDeviceSize m_bufferSize;
    uint32_t m_instanceCount;
    VkDeviceSize m_instanceSize;
    VkDeviceSize m_alignmentSize;
    VkBufferUsageFlags m_usageFlags;
    VkMemoryPropertyFlags m_memoryPropertyFlags;
};
```

### 4. Vulkan Graphics Pipeline

**Class: `VulkanPipeline`**
- **Responsibilities:**
  - Graphics pipeline creation and management
  - Shader module loading and compilation
  - Pipeline layout and descriptor set layout management
  - Render pass configuration

**Key Methods:**
```cpp
class VulkanPipeline {
public:
    struct PipelineConfigInfo {
        PipelineConfigInfo() = default;
        PipelineConfigInfo(const PipelineConfigInfo&) = delete;
        PipelineConfigInfo& operator=(const PipelineConfigInfo&) = delete;

        std::vector<VkVertexInputBindingDescription> bindingDescriptions{};
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};
        VkPipelineViewportStateCreateInfo viewportInfo;
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo;
        VkPipelineRasterizationStateCreateInfo rasterizationInfo;
        VkPipelineMultisampleStateCreateInfo multisampleInfo;
        VkPipelineColorBlendAttachmentState colorBlendAttachment;
        VkPipelineColorBlendStateCreateInfo colorBlendInfo;
        VkPipelineDepthStencilStateCreateInfo depthStencilInfo;
        std::vector<VkDynamicState> dynamicStateEnables;
        VkPipelineDynamicStateCreateInfo dynamicStateInfo;
        VkPipelineLayout pipelineLayout = nullptr;
        VkRenderPass renderPass = nullptr;
        uint32_t subpass = 0;
    };

    VulkanPipeline(
        VulkanDevice& device,
        const std::string& vertFilepath,
        const std::string& fragFilepath,
        const PipelineConfigInfo& configInfo);
    ~VulkanPipeline();

    void bind(VkCommandBuffer commandBuffer);

    static void defaultPipelineConfigInfo(PipelineConfigInfo& configInfo);
    static void enableAlphaBlending(PipelineConfigInfo& configInfo);

private:
    static std::vector<char> readFile(const std::string& filepath);
    void createGraphicsPipeline(
        const std::string& vertFilepath,
        const std::string& fragFilepath,
        const PipelineConfigInfo& configInfo);
    void createShaderModule(const std::vector<char>& code, VkShaderModule* shaderModule);

    VulkanDevice& m_device;
    VkPipeline m_graphicsPipeline;
    VkShaderModule m_vertShaderModule;
    VkShaderModule m_fragShaderModule;
};
```

### 5. Vulkan Renderer System

**Class: `VulkanRenderer`**
- **Responsibilities:**
  - Command buffer management and recording
  - Render pass execution
  - Frame synchronization
  - Resource binding and drawing commands

**Key Methods:**
```cpp
class VulkanRenderer {
public:
    VulkanRenderer(Window& window, VulkanDevice& device);
    ~VulkanRenderer();

    VkRenderPass getSwapchainRenderPass() const { return m_swapchain->getRenderPass(); }
    float getAspectRatio() const { return m_swapchain->extentAspectRatio(); }
    bool isFrameInProgress() const { return m_isFrameStarted; }

    VkCommandBuffer getCurrentCommandBuffer() const;
    int getFrameIndex() const;

    VkCommandBuffer beginFrame();
    void endFrame();
    void beginSwapchainRenderPass(VkCommandBuffer commandBuffer);
    void endSwapchainRenderPass(VkCommandBuffer commandBuffer);

private:
    void createCommandBuffers();
    void freeCommandBuffers();
    void recreateSwapchain();

    Window& m_window;
    VulkanDevice& m_device;
    std::unique_ptr<VulkanSwapchain> m_swapchain;
    std::vector<VkCommandBuffer> m_commandBuffers;

    uint32_t m_currentImageIndex;
    int m_currentFrameIndex{0};
    bool m_isFrameStarted{false};
};
```

## Vulkan Rendering Pipeline Specification

### 1. Vertex Shader (vertex.vert)
```glsl
#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoord;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragTexCoord;

void main() {
    vec4 worldPosition = ubo.model * vec4(position, 1.0);
    fragPos = worldPosition.xyz;
    fragNormal = mat3(transpose(inverse(ubo.model))) * normal;
    fragTexCoord = texCoord;

    gl_Position = ubo.proj * ubo.view * worldPosition;
}
```

### 2. Fragment Shader (fragment.frag)
```glsl
#version 450

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;

layout(set = 0, binding = 1) uniform sampler2D texSampler;

layout(push_constant) uniform PushConstants {
    vec3 lightPos;
    vec3 lightColor;
    vec3 viewPos;
} pushConstants;

layout(location = 0) out vec4 outColor;

void main() {
    // Ambient lighting
    float ambientStrength = 0.1;
    vec3 ambient = ambientStrength * pushConstants.lightColor;

    // Diffuse lighting
    vec3 norm = normalize(fragNormal);
    vec3 lightDir = normalize(pushConstants.lightPos - fragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * pushConstants.lightColor;

    // Specular lighting
    float specularStrength = 0.5;
    vec3 viewDir = normalize(pushConstants.viewPos - fragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * pushConstants.lightColor;

    vec3 result = (ambient + diffuse + specular) * texture(texSampler, fragTexCoord).rgb;
    outColor = vec4(result, 1.0);
}
```

### 3. Descriptor Set Layout and Management

**Uniform Buffer Object Structure:**
```cpp
struct UniformBufferObject {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

struct PushConstants {
    alignas(16) glm::vec3 lightPos;
    alignas(16) glm::vec3 lightColor;
    alignas(16) glm::vec3 viewPos;
};
```

**Descriptor Set Layout Creation:**
```cpp
void VulkanRenderer::createDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
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
```

## Vulkan Resource Management

### 1. Memory Allocation Strategy

**Memory Types and Usage:**
- **Device Local Memory:** For vertex buffers, index buffers, and textures
- **Host Visible Memory:** For uniform buffers and staging buffers
- **Host Coherent Memory:** For frequently updated uniform data

**Buffer Creation Helper:**
```cpp
void VulkanDevice::createBuffer(
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties,
    VkBuffer& buffer,
    VkDeviceMemory& bufferMemory) {

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(m_device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to create buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(m_device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate buffer memory!");
    }

    vkBindBufferMemory(m_device, buffer, bufferMemory, 0);
}
```

### 2. Texture and Image Management

**Class: `VulkanImage`**
```cpp
class VulkanImage {
public:
    VulkanImage(
        VulkanDevice& device,
        uint32_t width,
        uint32_t height,
        VkFormat format,
        VkImageTiling tiling,
        VkImageUsageFlags usage,
        VkMemoryPropertyFlags properties);
    ~VulkanImage();

    void createImageView(VkFormat format, VkImageAspectFlags aspectFlags);
    void transitionImageLayout(VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
    void copyBufferToImage(VkBuffer buffer);

    VkImage getImage() const { return m_image; }
    VkImageView getImageView() const { return m_imageView; }
    VkSampler getSampler() const { return m_sampler; }

private:
    void createTextureSampler();

    VulkanDevice& m_device;
    VkImage m_image;
    VkDeviceMemory m_imageMemory;
    VkImageView m_imageView;
    VkSampler m_sampler;
    uint32_t m_width, m_height;
};
```

### 3. Synchronization Objects

**Frame-in-Flight Management:**
```cpp
class FrameManager {
public:
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

    FrameManager(VulkanDevice& device);
    ~FrameManager();

    void waitForFence(size_t frameIndex);
    void resetFence(size_t frameIndex);
    VkSemaphore getImageAvailableSemaphore(size_t frameIndex);
    VkSemaphore getRenderFinishedSemaphore(size_t frameIndex);
    VkFence getInFlightFence(size_t frameIndex);

private:
    VulkanDevice& m_device;
    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    std::vector<VkFence> m_inFlightFences;
};
```

## Main Application Integration

### 1. Application Class Structure

**Class: `Application`**
```cpp
class Application {
public:
    Application();
    ~Application();

    void run();

private:
    void initVulkan();
    void createDescriptorPool();
    void createDescriptorSets();
    void createUniformBuffers();
    void createVertexBuffer();
    void createIndexBuffer();
    void createTextureImage();

    void updateUniformBuffer(uint32_t currentImage);
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

    void processInput(float deltaTime);
    void updateTransforms(float deltaTime);

    static constexpr int WIDTH = 800;
    static constexpr int HEIGHT = 600;

    Window m_window{WIDTH, HEIGHT, "Vulkan Cube Demo"};
    VulkanDevice m_device{m_window};
    VulkanRenderer m_renderer{m_window, m_device};

    std::unique_ptr<VulkanPipeline> m_pipeline;
    VkPipelineLayout m_pipelineLayout;
    VkDescriptorSetLayout m_descriptorSetLayout;
    VkDescriptorPool m_descriptorPool;
    std::vector<VkDescriptorSet> m_descriptorSets;

    std::unique_ptr<VulkanBuffer> m_vertexBuffer;
    std::unique_ptr<VulkanBuffer> m_indexBuffer;
    std::vector<std::unique_ptr<VulkanBuffer>> m_uniformBuffers;
    std::unique_ptr<VulkanImage> m_textureImage;

    Camera m_camera;
    glm::mat4 m_cubeTransform{1.0f};
    float m_rotationSpeed = 1.0f;
    float m_lastFrameTime = 0.0f;
};
```

### 2. Main Render Loop

**Vulkan Render Loop Implementation:**
```cpp
void Application::run() {
    while (!m_window.shouldClose()) {
        glfwPollEvents();

        // Calculate delta time
        float currentTime = static_cast<float>(glfwGetTime());
        float deltaTime = currentTime - m_lastFrameTime;
        m_lastFrameTime = currentTime;

        // Process input and update transforms
        processInput(deltaTime);
        updateTransforms(deltaTime);

        // Begin frame
        auto commandBuffer = m_renderer.beginFrame();
        if (commandBuffer != nullptr) {
            int frameIndex = m_renderer.getFrameIndex();

            // Update uniform buffer
            updateUniformBuffer(frameIndex);

            // Record command buffer
            recordCommandBuffer(commandBuffer, m_renderer.getCurrentImageIndex());

            // End frame
            m_renderer.endFrame();
        }
    }

    // Wait for device to finish before cleanup
    vkDeviceWaitIdle(m_device.getDevice());
}
```

### 3. Command Buffer Recording

**Command Buffer Recording Implementation:**
```cpp
void Application::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
    m_renderer.beginSwapchainRenderPass(commandBuffer);

    m_pipeline->bind(commandBuffer);

    VkBuffer vertexBuffers[] = {m_vertexBuffer->getBuffer()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

    vkCmdBindIndexBuffer(commandBuffer, m_indexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);

    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_pipelineLayout,
        0, 1,
        &m_descriptorSets[imageIndex],
        0, nullptr);

    PushConstants pushConstants{
        .lightPos = glm::vec3(2.0f, 2.0f, 2.0f),
        .lightColor = glm::vec3(1.0f, 1.0f, 1.0f),
        .viewPos = m_camera.getPosition()
    };

    vkCmdPushConstants(
        commandBuffer,
        m_pipelineLayout,
        VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        sizeof(PushConstants),
        &pushConstants);

    vkCmdDrawIndexed(commandBuffer, 36, 1, 0, 0, 0);  // 36 indices for cube

    m_renderer.endSwapchainRenderPass(commandBuffer);
}
```

## Technical Requirements

### Performance Requirements
- **Target Frame Rate:** 60+ FPS minimum
- **Memory Usage:** < 200MB for demo (including Vulkan overhead)
- **Startup Time:** < 3 seconds (including Vulkan initialization)
- **GPU Memory:** < 100MB VRAM usage
- **Multiple Frames in Flight:** 2-3 frames for optimal performance

### Platform Compatibility
- **Windows:** Windows 10+ with Vulkan 1.3 drivers
- **Linux:** Modern distributions with Mesa 22.0+ or proprietary drivers
- **macOS:** macOS 10.15+ with MoltenVK for Vulkan support

### Compiler Requirements
- **C++ Standard:** C++23
- **Compilers:**
  - MSVC 19.35+ (Visual Studio 2022 17.5+)
  - GCC 13+
  - Clang 16+

### Vulkan Requirements
- **Version:** Vulkan 1.3
- **Extensions Required:**
  - VK_KHR_swapchain
  - VK_EXT_debug_utils (debug builds only)
- **Features Used:**
  - Multiple descriptor sets
  - Push constants
  - Dynamic viewport and scissor
  - Depth testing
  - MSAA (optional)

### Validation Layers (Debug Builds)
- **VK_LAYER_KHRONOS_validation:** Complete validation coverage
- **Debug callback:** Custom message severity filtering
- **Performance warnings:** Enabled for optimization insights

## Success Criteria

### Functional Requirements
1. **Vulkan Initialization:** Successful instance, device, and swapchain creation
2. **Cube Rendering:** Textured cube visible with proper Vulkan pipeline
3. **Rotation Animation:** Smooth continuous rotation at 60+ FPS
4. **Multiple Frames:** Proper frame-in-flight synchronization
5. **Resource Management:** Efficient buffer and image handling
6. **Cross-Platform:** Builds and runs on Windows, Linux, and macOS (with MoltenVK)

### Quality Requirements
1. **Performance:** Maintains 60+ FPS with multiple frames in flight
2. **Memory Efficiency:** Optimal buffer usage and memory allocation
3. **Validation Layer Clean:** No validation errors in debug builds
4. **Code Quality:** Modern C++23 with RAII patterns
5. **Build System:** One-command build with automatic shader compilation

### Validation Tests
1. **Vulkan Validation:** Clean validation layer output
2. **Performance Test:** Frame rate monitoring with tools like RenderDoc
3. **Memory Test:** Vulkan memory allocation tracking
4. **Pipeline Test:** Graphics pipeline creation and binding verification
5. **Synchronization Test:** Proper semaphore and fence usage verification

This specification provides a comprehensive foundation for implementing a modern Vulkan-based Tidal Engine demo, establishing advanced patterns and architecture for high-performance graphics applications using explicit GPU APIs.