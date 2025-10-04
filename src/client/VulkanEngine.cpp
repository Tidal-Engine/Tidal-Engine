#include "client/VulkanEngine.hpp"
#include "vulkan/VulkanBuffer.hpp"
#include "vulkan/VulkanSwapchain.hpp"
#include "vulkan/VulkanPipeline.hpp"
#include "client/VulkanRenderer.hpp"
#include "client/NetworkClient.hpp"
#include "client/ChunkRenderer.hpp"
#include "client/ChunkMesh.hpp"
#include "client/TextureAtlas.hpp"
#include "client/InputManager.hpp"
#include "client/Camera.hpp"
#include "client/DebugOverlay.hpp"
#include "client/BlockOutlineRenderer.hpp"
#include "vulkan/CubeGeometry.hpp"
#include "core/Logger.hpp"
#include "core/ResourceManager.hpp"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>

#include <stdexcept>
#include <cstring>
#include <chrono>

namespace engine {

VulkanEngine::VulkanEngine()
    : lastPositionUpdate(std::chrono::steady_clock::now()),
      lastSentPosition(0.0f, 0.0f, 0.0f),
      lastBlockBreak(std::chrono::steady_clock::now()),
      wasLeftClickPressed(false) {
}
VulkanEngine::~VulkanEngine() = default;

void VulkanEngine::run() {
    initSDL();
    initVulkan();
    initGeometry();
    initRenderingResources();
    initImGui();
    initNetworking();
    mainLoop();
    cleanup();
}

void VulkanEngine::initSDL() {
    LOG_INFO("Initializing SDL3...");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        LOG_ERROR("Failed to initialize SDL: {}", SDL_GetError());
        throw std::runtime_error("Failed to initialize SDL");
    }

    LOG_INFO("Creating window ({}x{})...", config.windowWidth, config.windowHeight);

    window = SDL_CreateWindow(
        config.windowTitle.c_str(),
        static_cast<int>(config.windowWidth), static_cast<int>(config.windowHeight),
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
    );

    if (!window) {
        LOG_ERROR("Failed to create window: {}", SDL_GetError());
        throw std::runtime_error("Failed to create window");
    }

    LOG_INFO("Window created - click window to capture mouse");

    LOG_INFO("SDL3 initialized successfully");
}

void VulkanEngine::initVulkan() {
    LOG_INFO("Initializing Vulkan...");
    createInstance();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();

    // Initialize resource manager and register assets
    ResourceManager::init(".");
    ResourceManager::registerShader("cube_vert", "shaders/cube_vert.spv");
    ResourceManager::registerShader("cube_frag", "shaders/cube_frag.spv");

    LOG_INFO("Vulkan initialized successfully");
}

void VulkanEngine::initGeometry() {
    LOG_INFO("Loading cube geometry...");
    vertices = CubeGeometry::getVertices();
    indices = CubeGeometry::getIndices();
    LOG_INFO("Loaded {} vertices and {} indices", vertices.size(), indices.size());
}

void VulkanEngine::initRenderingResources() {
    LOG_INFO("Initializing rendering resources...");

    // Get queue family indices
    QueueFamilyIndices queueIndices = findQueueFamilies(physicalDevice);

    // Create subsystems
    bufferManager = std::make_unique<VulkanBuffer>(device, physicalDevice);
    swapchain = std::make_unique<VulkanSwapchain>(device, physicalDevice, surface, window);
    renderer = std::make_unique<VulkanRenderer>(device, physicalDevice,
                                                queueIndices.graphicsFamily,
                                                graphicsQueue, presentQueue);

    // Create swapchain
    swapchain->create();
    swapchain->createImageViews();

    // Create pipeline (needs swapchain extent and format)
    pipeline = std::make_unique<VulkanPipeline>(device, swapchain->getExtent(),
                                                swapchain->getImageFormat());
    pipeline->createRenderPass();
    pipeline->createDescriptorSetLayout();
    pipeline->createGraphicsPipeline(
        ResourceManager::getShaderPath("cube_vert"),
        ResourceManager::getShaderPath("cube_frag"));

    // Create rendering resources
    renderer->createCommandPool();
    renderer->createDepthResources(swapchain->getExtent());

    // Create texture atlas
    textureAtlas = std::make_unique<TextureAtlas>(device, physicalDevice,
                                                   renderer->getCommandPool(),
                                                   graphicsQueue);
    textureAtlas->loadTextures("assets/texturepacks");

    // Create chunk renderer
    chunkRenderer = std::make_unique<ChunkRenderer>(device, physicalDevice,
                                                   renderer->getCommandPool(),
                                                   graphicsQueue,
                                                   textureAtlas.get());

    // Give renderer access to chunk renderer
    renderer->setChunkRenderer(chunkRenderer.get());

    // Create input manager and camera
    inputManager = std::make_unique<InputManager>();
    // Camera at (0, 5, 10) looking toward origin with -20° pitch to see the ground
    camera = std::make_unique<Camera>(glm::vec3(0.0f, 5.0f, 10.0f), glm::vec3(0.0f, 1.0f, 0.0f), -90.0f, -20.0f);

    // Create debug overlay
    debugOverlay = std::make_unique<DebugOverlay>();

    // Create block outline renderer
    blockOutlineRenderer = std::make_unique<BlockOutlineRenderer>(device, physicalDevice,
                                                                  renderer->getCommandPool(),
                                                                  graphicsQueue);
    blockOutlineRenderer->init(pipeline->getRenderPass(), swapchain->getExtent(),
                               pipeline->getDescriptorSetLayout());

    // Give renderer access to block outline renderer
    renderer->setBlockOutlineRenderer(blockOutlineRenderer.get());

    // Create framebuffers
    swapchain->createFramebuffers(pipeline->getRenderPass(), renderer->getDepthImageView());

    // Create buffers
    bufferManager->createVertexBuffer(vertices.data(), sizeof(vertices[0]) * vertices.size(),
                                     renderer->getCommandPool(), graphicsQueue);
    bufferManager->createIndexBuffer(indices.data(), sizeof(indices[0]) * indices.size(),
                                    renderer->getCommandPool(), graphicsQueue);
    bufferManager->createUniformBuffers(EngineConfig::MAX_FRAMES_IN_FLIGHT, sizeof(UniformBufferObject));

    // Create descriptor sets
    pipeline->createDescriptorPool(EngineConfig::MAX_FRAMES_IN_FLIGHT);
    pipeline->createDescriptorSets(bufferManager->getUniformBuffers(), sizeof(UniformBufferObject));

    // Update descriptor sets with texture atlas
    pipeline->updateTextureDescriptors(textureAtlas->getImageView(), textureAtlas->getSampler());

    // Create command buffers and sync objects
    renderer->createCommandBuffers(EngineConfig::MAX_FRAMES_IN_FLIGHT);
    renderer->createSyncObjects(EngineConfig::MAX_FRAMES_IN_FLIGHT);

    LOG_INFO("Rendering resources initialized successfully");
}

void VulkanEngine::initImGui() {
    LOG_INFO("Initializing ImGui...");

    // Create descriptor pool for ImGui
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
    };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1;
    pool_info.poolSizeCount = 1;
    pool_info.pPoolSizes = pool_sizes;

    if (vkCreateDescriptorPool(device, &pool_info, nullptr, &imguiDescriptorPool) != VK_SUCCESS) {
        LOG_ERROR("Failed to create ImGui descriptor pool");
        throw std::runtime_error("Failed to create ImGui descriptor pool");
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Setup ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplSDL3_InitForVulkan(window);

    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = instance;
    init_info.PhysicalDevice = physicalDevice;
    init_info.Device = device;
    init_info.QueueFamily = findQueueFamilies(physicalDevice).graphicsFamily;
    init_info.Queue = graphicsQueue;
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = imguiDescriptorPool;
    init_info.RenderPass = pipeline->getRenderPass();
    init_info.Subpass = 0;
    init_info.MinImageCount = 2;
    init_info.ImageCount = static_cast<uint32_t>(swapchain->getImageViews().size());
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&init_info);

    LOG_INFO("ImGui initialized successfully");
}

void VulkanEngine::initNetworking() {
    LOG_INFO("Initializing networking...");

    // Create network client
    networkClient = std::make_unique<NetworkClient>();

    // Set up callback to queue chunks when received (async processing)
    networkClient->setOnChunkReceived([this](const ChunkCoord& coord) {
        const Chunk* chunk = networkClient->getChunk(coord);
        if (!chunk) return;

        // Helper to queue a chunk with its neighbors
        auto queueChunk = [this](const ChunkCoord& chunkCoord) {
            const Chunk* c = networkClient->getChunk(chunkCoord);
            if (!c) return;

            PendingChunk pending;
            pending.coord = chunkCoord;
            pending.chunk = *c;

            // Copy neighbor chunks if they exist
            const Chunk* neighborNegX = networkClient->getChunk({chunkCoord.x - 1, chunkCoord.y, chunkCoord.z});
            const Chunk* neighborPosX = networkClient->getChunk({chunkCoord.x + 1, chunkCoord.y, chunkCoord.z});
            const Chunk* neighborNegY = networkClient->getChunk({chunkCoord.x, chunkCoord.y - 1, chunkCoord.z});
            const Chunk* neighborPosY = networkClient->getChunk({chunkCoord.x, chunkCoord.y + 1, chunkCoord.z});
            const Chunk* neighborNegZ = networkClient->getChunk({chunkCoord.x, chunkCoord.y, chunkCoord.z - 1});
            const Chunk* neighborPosZ = networkClient->getChunk({chunkCoord.x, chunkCoord.y, chunkCoord.z + 1});

            if (neighborNegX) { pending.neighborNegX = *neighborNegX; pending.hasNegX = true; }
            if (neighborPosX) { pending.neighborPosX = *neighborPosX; pending.hasPosX = true; }
            if (neighborNegY) { pending.neighborNegY = *neighborNegY; pending.hasNegY = true; }
            if (neighborPosY) { pending.neighborPosY = *neighborPosY; pending.hasPosY = true; }
            if (neighborNegZ) { pending.neighborNegZ = *neighborNegZ; pending.hasNegZ = true; }
            if (neighborPosZ) { pending.neighborPosZ = *neighborPosZ; pending.hasPosZ = true; }

            {
                std::lock_guard<std::mutex> lock(pendingChunksMutex);
                pendingChunks.push(pending);
            }
        };

        // Queue the new chunk
        queueChunk(coord);

        // Queue neighboring chunks for re-meshing (they can now cull faces against this chunk)
        queueChunk({coord.x - 1, coord.y, coord.z});
        queueChunk({coord.x + 1, coord.y, coord.z});
        queueChunk({coord.x, coord.y - 1, coord.z});
        queueChunk({coord.x, coord.y + 1, coord.z});
        queueChunk({coord.x, coord.y, coord.z - 1});
        queueChunk({coord.x, coord.y, coord.z + 1});

        LOG_DEBUG("Queued chunk ({}, {}, {}) and neighbors for async mesh generation",
                 coord.x, coord.y, coord.z);
    });

    // Set up callback to remove chunks when unloaded
    networkClient->setOnChunkUnloaded([this](const ChunkCoord& coord) {
        chunkRenderer->removeChunk(coord);
        LOG_INFO("Removed chunk ({}, {}, {}) from GPU | Total chunks: {}",
                 coord.x, coord.y, coord.z, chunkRenderer->getLoadedChunkCount());
    });

    // Connect to localhost (integrated server for now)
    if (!networkClient->connect("127.0.0.1", 25565, 5000)) {
        LOG_ERROR("Failed to connect to server!");
        throw std::runtime_error("Failed to connect to game server");
    }

    LOG_INFO("Connected to server successfully");

    // Process initial messages to receive spawn chunks
    for (int i = 0; i < 50; i++) {
        networkClient->update();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    LOG_INFO("Networking initialized | Received {} chunks",
             networkClient->getChunks().size());
}

void VulkanEngine::createInstance() {
    LOG_DEBUG("Creating Vulkan instance...");
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = config.windowTitle.c_str();
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = EngineConfig::ENGINE_NAME;
    appInfo.engineVersion = VK_MAKE_VERSION(EngineConfig::ENGINE_VERSION_MAJOR,
                                             EngineConfig::ENGINE_VERSION_MINOR,
                                             EngineConfig::ENGINE_VERSION_PATCH);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    // Get SDL required extensions
    uint32_t sdlExtensionCount = 0;
    const char* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&sdlExtensionCount);

    std::vector<const char*> extensions(sdlExtensions, sdlExtensions + sdlExtensionCount);  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

#ifdef ENABLE_VALIDATION_LAYERS
    const std::vector<const char*> VALIDATION_LAYERS = {
        "VK_LAYER_KHRONOS_validation"
    };

    // Check if validation layers are available
    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    bool layersAvailable = true;
    for (const char* layerName : VALIDATION_LAYERS) {
        bool layerFound = false;
        for (const auto& layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }
        if (!layerFound) {
            LOG_WARN("Validation layer not available: {}", layerName);
            layersAvailable = false;
        }
    }

    if (layersAvailable) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(VALIDATION_LAYERS.size());
        createInfo.ppEnabledLayerNames = VALIDATION_LAYERS.data();
        LOG_INFO("Validation layers enabled");
    } else {
        createInfo.enabledLayerCount = 0;
        LOG_WARN("Validation layers requested but not available - continuing without them");
    }
#else
    createInfo.enabledLayerCount = 0;
    LOG_DEBUG("Validation layers disabled");
#endif

    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        LOG_ERROR("Failed to create Vulkan instance");
        throw std::runtime_error("Failed to create Vulkan instance");
    }

    LOG_INFO("Vulkan instance created (API version 1.3)");
}

void VulkanEngine::createSurface() {
    LOG_DEBUG("Creating Vulkan surface...");

    if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface)) {
        LOG_ERROR("Failed to create window surface");
        throw std::runtime_error("Failed to create window surface");
    }

    LOG_DEBUG("Vulkan surface created");
}

void VulkanEngine::pickPhysicalDevice() {
    LOG_DEBUG("Selecting physical device...");
    uint32_t deviceCount = 0;
    if (vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr) != VK_SUCCESS) {
        LOG_ERROR("Failed to enumerate physical devices");
        throw std::runtime_error("Failed to enumerate physical devices");
    }

    if (deviceCount == 0) {
        LOG_ERROR("No GPUs with Vulkan support found");
        throw std::runtime_error("Failed to find GPUs with Vulkan support");
    }

    LOG_DEBUG("Found {} Vulkan-capable device(s)", deviceCount);

    std::vector<VkPhysicalDevice> devices(deviceCount);
    if (vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()) != VK_SUCCESS) {
        LOG_ERROR("Failed to get physical device list");
        throw std::runtime_error("Failed to get physical device list");
    }

    // Score and select the best device
    int bestScore = -1;
    VkPhysicalDevice bestDevice = VK_NULL_HANDLE;

    for (const auto& device : devices) {
        int score = rateDeviceSuitability(device);
        if (score > bestScore) {
            bestScore = score;
            bestDevice = device;
        }
    }

    if (bestDevice == VK_NULL_HANDLE || bestScore < 0) {
        LOG_ERROR("Failed to find suitable GPU");
        throw std::runtime_error("Failed to find suitable GPU");
    }

    physicalDevice = bestDevice;

    // Log device info
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
    LOG_INFO("Selected GPU: {} (score: {})", deviceProperties.deviceName, bestScore);
}

VulkanEngine::QueueFamilyIndices VulkanEngine::findQueueFamilies(VkPhysicalDevice device) {
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    if (queueFamilyCount == 0) {
        LOG_ERROR("No queue families found for device");
        throw std::runtime_error("No queue families found");
    }

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    for (uint32_t i = 0; i < queueFamilies.size(); i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }

        VkBool32 presentSupport = false;
        if (vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport) != VK_SUCCESS) {
            LOG_WARN("Failed to check present support for queue family {}", i);
            continue;
        }

        if (presentSupport) {
            indices.presentFamily = i;
        }

        if (indices.isComplete()) {
            break;
        }
    }

    if (!indices.isComplete()) {
        LOG_ERROR("Failed to find all required queue families");
        throw std::runtime_error("Failed to find required queue families");
    }

    LOG_DEBUG("Found queue families - Graphics: {}, Present: {}",
              indices.graphicsFamily, indices.presentFamily);

    return indices;
}

int VulkanEngine::rateDeviceSuitability(VkPhysicalDevice device) {
    if (!isDeviceSuitable(device)) {
        return -1;
    }

    VkPhysicalDeviceProperties deviceProperties;
    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);
    vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

    int score = 0;

    // Discrete GPUs have a significant performance advantage
    if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        score += 1000;
    }

    // Maximum possible size of textures affects graphics quality
    score += static_cast<int>(deviceProperties.limits.maxImageDimension2D);

    // Prefer devices with geometry shader support
    if (deviceFeatures.geometryShader) {
        score += 100;
    }

    // Prefer devices with anisotropic filtering
    if (deviceFeatures.samplerAnisotropy) {
        score += 50;
    }

    LOG_DEBUG("GPU '{}' rated with score: {}", deviceProperties.deviceName, score);
    return score;
}

bool VulkanEngine::isDeviceSuitable(VkPhysicalDevice device) {
    // Check queue families
    QueueFamilyIndices indices = findQueueFamilies(device);
    if (!indices.isComplete()) {
        LOG_DEBUG("Device missing required queue families");
        return false;
    }

    // Check for required features
    VkPhysicalDeviceFeatures supportedFeatures;
    vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

    // We need geometry shader support for future features
    if (!supportedFeatures.geometryShader) {
        LOG_DEBUG("Device missing geometry shader support");
        return false;
    }

    return true;
}

void VulkanEngine::recreateSwapchain() {
    LOG_INFO("Recreating swapchain due to window resize or out-of-date swapchain");

    // Handle minimization - wait until window is visible again
    int width = 0;
    int height = 0;
    SDL_GetWindowSize(window, &width, &height);
    while (width == 0 || height == 0) {
        SDL_GetWindowSize(window, &width, &height);
        SDL_WaitEvent(nullptr);
    }

    // Wait for device to finish
    vkDeviceWaitIdle(device);

    // Recreate swapchain and dependent resources
    swapchain->recreate();
    renderer->recreateDepthResources(swapchain->getExtent());
    swapchain->createFramebuffers(pipeline->getRenderPass(), renderer->getDepthImageView());

    // Recreate BlockOutlineRenderer with new extent
    blockOutlineRenderer->cleanup();
    blockOutlineRenderer->init(pipeline->getRenderPass(), swapchain->getExtent(),
                              pipeline->getDescriptorSetLayout());

    framebufferResized = false;

    LOG_INFO("Swapchain recreation complete");
}

void VulkanEngine::createLogicalDevice() {
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily, indices.presentFamily};

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};

    const std::vector<const char*> DEVICE_EXTENSIONS = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(DEVICE_EXTENSIONS.size());
    createInfo.ppEnabledExtensionNames = DEVICE_EXTENSIONS.data();
    createInfo.enabledLayerCount = 0;

    if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
        LOG_ERROR("Failed to create logical device");
        throw std::runtime_error("Failed to create logical device");
    }

    vkGetDeviceQueue(device, indices.graphicsFamily, 0, &graphicsQueue);
    vkGetDeviceQueue(device, indices.presentFamily, 0, &presentQueue);

    LOG_DEBUG("Logical device created with graphics and present queues");
}

void VulkanEngine::mainLoop() {
    LOG_INFO("Entering main loop...");

    bool running = true;
    SDL_Event event;
    lastFrameTime = std::chrono::steady_clock::now();

    while (running) {
        performanceMetrics.beginFrame();

        // Calculate delta time
        auto now = std::chrono::steady_clock::now();
        deltaTime = std::chrono::duration<float>(now - lastFrameTime).count();
        lastFrameTime = now;

        // Process SDL events - handle window events, pass input to InputManager
        inputManager->beginFrame();
        while (SDL_PollEvent(&event)) {
            // Let ImGui handle events first (only when mouse is not captured)
            if (!SDL_GetWindowRelativeMouseMode(window)) {
                ImGui_ImplSDL3_ProcessEvent(&event);
            }

            if (event.type == SDL_EVENT_QUIT) {
                LOG_INFO("Quit event received");
                running = false;
            } else if (event.type == SDL_EVENT_WINDOW_RESIZED ||
                       event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED ||
                       event.type == SDL_EVENT_WINDOW_MAXIMIZED) {
                LOG_DEBUG("Window size changed (event type: {})", event.type);
                framebufferResized = true;
            } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                // Enable relative mouse mode when user clicks in the window
                if (!SDL_GetWindowRelativeMouseMode(window)) {
                    SDL_SetWindowRelativeMouseMode(window, true);
                    LOG_INFO("Mouse captured - press ESC to release");
                }
            } else if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) {
                // Release mouse capture on ESC key
                if (SDL_GetWindowRelativeMouseMode(window)) {
                    SDL_SetWindowRelativeMouseMode(window, false);
                    LOG_INFO("Mouse released - click to recapture");
                }
            } else if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_F3) {
                // Toggle debug overlay on F3
                debugOverlay->toggle();
                LOG_DEBUG("Debug overlay toggled: {}", debugOverlay->getVisible() ? "ON" : "OFF");
            }

            // Always pass events to input manager (it will handle them appropriately)
            inputManager->handleEvent(event);
        }

        // Process network messages
        networkClient->update();

        // Process chunk loading asynchronously (up to MAX_CHUNKS_PER_FRAME per frame)
        processPendingChunks();

        // Upload completed meshes to GPU
        uploadCompletedMeshes();

        // Only update camera if mouse is captured
        if (SDL_GetWindowRelativeMouseMode(window)) {
            // Update camera with noclip movement
            // Apply 8x speed boost when Ctrl is held
            float speedMultiplier = inputManager->isKeyPressed(SDL_SCANCODE_LCTRL) ||
                                   inputManager->isKeyPressed(SDL_SCANCODE_RCTRL) ? 8.0f : 1.0f;

            camera->processMovement(
                inputManager->isKeyPressed(SDL_SCANCODE_W),  // Forward
                inputManager->isKeyPressed(SDL_SCANCODE_S),  // Backward
                inputManager->isKeyPressed(SDL_SCANCODE_A),  // Left
                inputManager->isKeyPressed(SDL_SCANCODE_D),  // Right
                inputManager->isKeyPressed(SDL_SCANCODE_SPACE),  // Up
                inputManager->isKeyPressed(SDL_SCANCODE_LSHIFT), // Down
                deltaTime,
                config.cameraSpeed * speedMultiplier
            );

            glm::vec2 mouseDelta = inputManager->getMouseDelta();
            if (mouseDelta.x != 0.0f || mouseDelta.y != 0.0f) {
                // Invert Y for standard FPS controls (not airplane mode)
                camera->processMouseMovement(mouseDelta.x, -mouseDelta.y, config.mouseSensitivity);
            }
        }

        inputManager->endFrame();

        // Send position updates to server (throttled to avoid spam)
        auto currentTime = std::chrono::steady_clock::now();
        auto timeSinceLastUpdate = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastPositionUpdate).count();
        float distanceMoved = glm::distance(camera->getPosition(), lastSentPosition);

        // Send update if moved more than 0.5 blocks OR 100ms has passed since last update
        if (distanceMoved > 0.5f || timeSinceLastUpdate > 100) {
            networkClient->sendPlayerMove(
                camera->getPosition(),
                glm::vec3(0.0f),  // velocity (not used yet)
                0.0f,  // yaw (not used yet)
                0.0f   // pitch (not used yet)
            );
            lastPositionUpdate = currentTime;
            lastSentPosition = camera->getPosition();
        }

        // Perform raycasting to find targeted block
        targetedBlock = Raycaster::cast(
            camera->getPosition(),
            camera->getFront(),
            10.0f,  // 10 block reach distance
            networkClient.get()
        );

        // Handle block breaking (left click)
        // Allows spam clicking at any speed, but limits held clicks to cooldown rate
        bool leftClick = inputManager->isMouseButtonPressed(SDL_BUTTON_LEFT);
        if (leftClick && targetedBlock.has_value()) {
            bool shouldBreak = false;

            if (!wasLeftClickPressed) {
                // New click detected - always allow
                shouldBreak = true;
            } else {
                // Button is still held - check cooldown
                auto timeSinceLastBreak = std::chrono::duration<float>(currentTime - lastBlockBreak).count();
                if (timeSinceLastBreak >= BLOCK_BREAK_COOLDOWN) {
                    shouldBreak = true;
                }
            }

            if (shouldBreak) {
                LOG_INFO("CLIENT: Breaking block at ({}, {}, {})",
                         targetedBlock->blockPos.x, targetedBlock->blockPos.y, targetedBlock->blockPos.z);
                networkClient->sendBlockBreak(
                    targetedBlock->blockPos.x,
                    targetedBlock->blockPos.y,
                    targetedBlock->blockPos.z
                );
                lastBlockBreak = currentTime;
            }
        }
        wasLeftClickPressed = leftClick;

        // Update block outline renderer
        blockOutlineRenderer->update(targetedBlock);

        // Recreate swapchain if needed (after resize or out of date)
        if (framebufferResized) {
            recreateSwapchain();
        }

        // Update uniform buffer with camera matrices
        UniformBufferObject ubo{};
        ubo.model = glm::mat4(1.0f);
        ubo.view = camera->getViewMatrix();
        ubo.proj = camera->getProjectionMatrix(
            static_cast<float>(swapchain->getExtent().width) / static_cast<float>(swapchain->getExtent().height),
            config.fov,
            EngineConfig::NEAR_PLANE,
            EngineConfig::FAR_PLANE
        );
        ubo.lightPos = glm::vec4(10.0f, 10.0f, 10.0f, 1.0f);
        ubo.viewPos = glm::vec4(camera->getPosition(), 1.0f);

        // Copy UBO to mapped uniform buffer for current frame
        uint32_t currentFrame = renderer->getCurrentFrame();
        std::memcpy(bufferManager->getUniformBuffersMapped()[currentFrame], &ubo, sizeof(ubo));

        // Start ImGui frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        // Disable ImGui input when mouse is captured
        ImGuiIO& io = ImGui::GetIO();
        if (SDL_GetWindowRelativeMouseMode(window)) {
            io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
        } else {
            io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
        }

        // Render debug overlay (updates ImGui)
        // Note: Without frustum culling, all loaded chunks are visible
        uint32_t chunksVisible = chunkRenderer->getChunkCount();
        uint32_t chunksTotal = chunkRenderer->getChunkCount();
        uint32_t verticesRendered = chunkRenderer->getTotalVertices();
        uint32_t drawCalls = (chunksVisible > 0) ? 1 : 0; // Batched rendering = 1 draw call for all chunks

        debugOverlay->render(camera.get(), &performanceMetrics, networkClient.get(),
                            chunksVisible, chunksTotal, verticesRendered, drawCalls, &targetedBlock);

        // Render crosshair
        debugOverlay->renderCrosshair();

        // Finalize ImGui rendering
        ImGui::Render();

        // Draw frame (simplified - just render chunks for now)
        bool needsRecreation = renderer->drawFrame(swapchain->getSwapchain(), swapchain->getFramebuffers(),
                          pipeline->getRenderPass(), swapchain->getExtent(),
                          pipeline->getPipeline(), pipeline->getPipelineLayout(),
                          VK_NULL_HANDLE, VK_NULL_HANDLE, 0,  // No cube geometry
                          pipeline->getDescriptorSets(),
                          bufferManager->getUniformBuffersMapped(),
                          EngineConfig::MAX_FRAMES_IN_FLIGHT);

        if (needsRecreation) {
            framebufferResized = true;
        }

        performanceMetrics.endFrame();
    }

    renderer->waitIdle();
    LOG_INFO("Exited main loop - Total frames: {}, Average FPS: {:.1f}",
             performanceMetrics.getFrameCount(), performanceMetrics.getFPS());
}

void VulkanEngine::processPendingChunks() {
    // Clean up completed tasks
    meshGenerationTasks.erase(
        std::remove_if(meshGenerationTasks.begin(), meshGenerationTasks.end(),
            [](const std::future<void>& f) {
                return f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
            }),
        meshGenerationTasks.end()
    );

    // Process up to MAX_CHUNKS_PER_FRAME chunks per frame
    size_t processed = 0;
    while (processed < MAX_CHUNKS_PER_FRAME) {
        PendingChunk pending;

        // Get next pending chunk
        {
            std::lock_guard<std::mutex> lock(pendingChunksMutex);
            if (pendingChunks.empty()) break;

            pending = std::move(pendingChunks.front());
            pendingChunks.pop();
        }

        // Launch async mesh generation
        auto task = std::async(std::launch::async, [this, p = pending]() {
            CompletedMesh completed;
            completed.coord = p.coord;

            // Generate mesh on background thread
            ChunkMesh::generateMesh(
                p.chunk,
                completed.vertices,
                completed.indices,
                textureAtlas.get(),
                p.hasNegX ? &p.neighborNegX : nullptr,
                p.hasPosX ? &p.neighborPosX : nullptr,
                p.hasNegY ? &p.neighborNegY : nullptr,
                p.hasPosY ? &p.neighborPosY : nullptr,
                p.hasNegZ ? &p.neighborNegZ : nullptr,
                p.hasPosZ ? &p.neighborPosZ : nullptr
            );

            // Queue completed mesh for upload
            {
                std::lock_guard<std::mutex> lock(completedMeshesMutex);
                completedMeshes.push(std::move(completed));
            }
        });

        meshGenerationTasks.push_back(std::move(task));
        processed++;
    }
}

void VulkanEngine::uploadCompletedMeshes() {
    std::lock_guard<std::mutex> lock(completedMeshesMutex);

    while (!completedMeshes.empty()) {
        CompletedMesh& completed = completedMeshes.front();

        // Upload mesh to GPU (this is fast, just creating buffers)
        if (!completed.vertices.empty() && !completed.indices.empty()) {
            chunkRenderer->uploadChunkMesh(completed.coord, completed.vertices, completed.indices);
            LOG_DEBUG("Uploaded mesh for chunk ({}, {}, {}) | {} vertices, {} indices",
                     completed.coord.x, completed.coord.y, completed.coord.z,
                     completed.vertices.size(), completed.indices.size());
        }

        completedMeshes.pop();
    }
}

void VulkanEngine::cleanup() {
    LOG_INFO("Cleaning up resources...");

    // Wait for all async mesh generation tasks to complete before cleaning up GPU resources
    LOG_DEBUG("Waiting for {} async mesh generation tasks to complete", meshGenerationTasks.size());
    for (auto& task : meshGenerationTasks) {
        if (task.valid()) {
            task.wait();
        }
    }
    meshGenerationTasks.clear();

    // Clear any remaining queues
    {
        std::lock_guard<std::mutex> lock(pendingChunksMutex);
        while (!pendingChunks.empty()) {
            pendingChunks.pop();
        }
    }
    {
        std::lock_guard<std::mutex> lock(completedMeshesMutex);
        while (!completedMeshes.empty()) {
            completedMeshes.pop();
        }
    }
    LOG_DEBUG("Async tasks and queues cleared");

    cleanupImGui();

    // Clean up chunk and block outline renderers before destroying device
    if (blockOutlineRenderer) {
        LOG_DEBUG("Cleaning up block outline renderer");
        blockOutlineRenderer->cleanup();
    }

    if (chunkRenderer) {
        LOG_DEBUG("Cleaning up chunk renderer");
        chunkRenderer->cleanup();
    }

    if (renderer) {
        renderer->cleanup();
    }

    if (bufferManager) {
        bufferManager->cleanup();
    }

    if (pipeline) {
        pipeline->cleanup();
    }

    if (swapchain) {
        swapchain->cleanup();
    }

    if (device != VK_NULL_HANDLE) {
        LOG_DEBUG("Destroying logical device");
        vkDestroyDevice(device, nullptr);
    }

    if (surface != VK_NULL_HANDLE) {
        LOG_DEBUG("Destroying surface");
        vkDestroySurfaceKHR(instance, surface, nullptr);
    }

    if (instance != VK_NULL_HANDLE) {
        LOG_DEBUG("Destroying Vulkan instance");
        vkDestroyInstance(instance, nullptr);
    }

    if (window) {
        LOG_DEBUG("Destroying window");
        SDL_DestroyWindow(window);
    }

    LOG_DEBUG("Shutting down SDL");
    SDL_Quit();

    LOG_INFO("Cleanup complete");
}

void VulkanEngine::cleanupImGui() {
    if (imguiDescriptorPool != VK_NULL_HANDLE) {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();

        vkDestroyDescriptorPool(device, imguiDescriptorPool, nullptr);
        imguiDescriptorPool = VK_NULL_HANDLE;
    }
}

} // namespace engine
