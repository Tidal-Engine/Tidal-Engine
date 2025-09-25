#include "game/GameClientUI.h"
#include "game/GameClient.h"
#include "game/GameClientInput.h"
#include "game/GameClientRenderer.h"
#include "vulkan/VulkanDevice.h"
#include "graphics/TextureManager.h"
#include "system/UserDataManager.h"

#include <iostream>
#include <stdexcept>
#include <filesystem>
#include <cstring>

GameClientUI::GameClientUI(GameClient& gameClient, GLFWwindow* window, VulkanDevice& device)
    : m_gameClient(gameClient), m_window(window), m_device(device) {
}

GameClientUI::~GameClientUI() {
    shutdown();
}

bool GameClientUI::initialize() {
    initImGui();
    return true;
}

void GameClientUI::shutdown() {
    cleanupImGui();
}

void GameClientUI::initImGui() {
    // Create descriptor pool for ImGui
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = std::size(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;

    if (vkCreateDescriptorPool(m_device.getDevice(), &pool_info, nullptr, &m_imguiDescriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create ImGui descriptor pool!");
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForVulkan(m_window, true);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = m_device.getInstance();
    init_info.PhysicalDevice = m_device.getPhysicalDevice();
    init_info.Device = m_device.getDevice();
    init_info.QueueFamily = m_device.findPhysicalQueueFamilies().graphicsFamily.value();
    init_info.Queue = m_device.getGraphicsQueue();
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = m_imguiDescriptorPool;
    init_info.Subpass = 0;
    init_info.MinImageCount = 2;
    init_info.ImageCount = 2;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.Allocator = nullptr;
    init_info.CheckVkResultFn = nullptr;

    // Get renderer from GameClient to access render pass
    ImGui_ImplVulkan_Init(&init_info, m_gameClient.getRenderer()->getSwapChainRenderPass());

    // Upload fonts
    VkCommandBuffer command_buffer = m_device.beginSingleTimeCommands();
    ImGui_ImplVulkan_CreateFontsTexture();
    m_device.endSingleTimeCommands(command_buffer);
    vkDeviceWaitIdle(m_device.getDevice());
    ImGui_ImplVulkan_DestroyFontsTexture();

    // Create atlas texture descriptor for ImGui (will be set when texture manager is ready)
    m_atlasTextureDescriptor = VK_NULL_HANDLE;
}

void GameClientUI::cleanupImGui() {
    // Reset atlas texture descriptor (ImGui manages its cleanup)
    m_atlasTextureDescriptor = VK_NULL_HANDLE;

    if (m_imguiDescriptorPool != VK_NULL_HANDLE) {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        // Safely destroy ImGui descriptor pool
        vkDestroyDescriptorPool(m_device.getDevice(), m_imguiDescriptorPool, nullptr);
        m_imguiDescriptorPool = VK_NULL_HANDLE;
    }
}

void GameClientUI::createAtlasTextureDescriptor() {
    // Get texture manager
    TextureManager* textureManager = m_gameClient.getTextureManager();
    if (!textureManager) {
        m_atlasTextureDescriptor = VK_NULL_HANDLE;
        return;
    }

    // Create descriptor set for the atlas texture using ImGui's helper function
    m_atlasTextureDescriptor = ImGui_ImplVulkan_AddTexture(
        textureManager->getAtlasSampler(),
        textureManager->getAtlasImageView(),
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );
}

void GameClientUI::renderImGui(VkCommandBuffer commandBuffer) {
    // Start the Dear ImGui frame
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Render UI based on game state
    GameState gameState = m_gameClient.getGameState();
    if (gameState == GameState::MAIN_MENU) {
        renderMainMenu();
    } else if (gameState == GameState::WORLD_SELECTION) {
        renderWorldSelection();
    } else if (gameState == GameState::LOADING) {
        renderLoadingScreen();
        // Don't render other UI during loading, but continue to frame end
    } else if (gameState == GameState::PAUSED) {
        renderPauseMenu();
    } else {
        // Render debug window if enabled and in game
        if (m_showDebugWindow) {
            renderDebugWindow();
        }

        // Render modular HUD boxes
        if (m_showPerformanceHUD) {
            renderPerformanceHUD();
        }
        if (m_showRenderingHUD) {
            renderRenderingHUD();
        }
        if (m_showCameraHUD) {
            renderCameraHUD();
        }

        // Render crosshair overlay
        renderCrosshair();

        // Render hotbar
        renderHotbar();
    }

    // Render ImGui
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
}

void GameClientUI::renderMainMenu() {
    // Center the main menu window
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x + viewport->Size.x * 0.5f, viewport->Pos.y + viewport->Size.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));

    ImGui::Begin("Tidal Engine", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::Text("Welcome to Tidal Engine!");
    ImGui::Separator();

    // Single Player button
    if (ImGui::Button("Single Player", ImVec2(200, 40))) {
        std::cout << "Opening world selection..." << std::endl;
        m_gameClient.setGameState(GameState::WORLD_SELECTION);
    }

    // Multiplayer button
    if (ImGui::Button("Multiplayer", ImVec2(200, 40))) {
        std::cout << "Multiplayer not implemented yet!" << std::endl;
        // Show a simple popup for now
        ImGui::OpenPopup("Multiplayer Info");
    }

    // Multiplayer info popup
    if (ImGui::BeginPopupModal("Multiplayer Info", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Multiplayer is not implemented yet!");
        ImGui::Text("For now, use command line options:");
        ImGui::Text("  ./TidalEngine --server");
        ImGui::Text("  ./TidalEngine --client <host>");
        ImGui::Separator();
        if (ImGui::Button("OK", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // Settings button
    if (ImGui::Button("Settings", ImVec2(200, 40))) {
        std::cout << "Opening settings..." << std::endl;
        // TODO: Open settings menu
        m_gameClient.setGameState(GameState::SETTINGS);
    }

    // Quit button
    if (ImGui::Button("Quit", ImVec2(200, 40))) {
        std::cout << "Quitting..." << std::endl;
        // Need to add a method to quit the game through GameClient
        m_gameClient.requestQuit();
    }

    ImGui::End();
}

void GameClientUI::renderWorldSelection() {
    // Center the world selection window
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x + viewport->Size.x * 0.5f, viewport->Pos.y + viewport->Size.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));

    ImGui::Begin("Select World", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::Text("Select or Create a World");
    ImGui::Separator();

    ImGui::Text("Existing Worlds:");

    const auto& availableWorlds = m_gameClient.getAvailableWorlds();
    if (availableWorlds.empty()) {
        ImGui::Text("No worlds found. Create a new world to get started!");
    } else {
        for (size_t i = 0; i < availableWorlds.size(); i++) {
            char buttonLabel[128];
            snprintf(buttonLabel, sizeof(buttonLabel), "%s##%zu", availableWorlds[i].c_str(), i);

            if (ImGui::Button(buttonLabel, ImVec2(300, 30))) {
                std::cout << "Loading world: " << availableWorlds[i] << std::endl;
                m_gameClient.startSingleplayerGame(availableWorlds[i]);
            }
        }
    }

    ImGui::Separator();

    // Create New World button
    if (ImGui::Button("Create New World", ImVec2(300, 40))) {
        m_showCreateWorldDialog = true;
        std::strcpy(m_newWorldName, ""); // Clear the input
    }

    // Create New World Dialog
    if (m_showCreateWorldDialog) {
        ImGui::OpenPopup("Create New World");
    }

    if (ImGui::BeginPopupModal("Create New World", &m_showCreateWorldDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Enter a name for your new world:");
        ImGui::InputText("World Name", m_newWorldName, sizeof(m_newWorldName));

        ImGui::Separator();

        bool canCreate = strlen(m_newWorldName) > 0;

        if (!canCreate) {
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
        }

        if (ImGui::Button("Create", ImVec2(120, 0)) && canCreate) {
            if (m_gameClient.createNewWorld(m_newWorldName)) {
                m_gameClient.startSingleplayerGame(m_newWorldName);
                m_showCreateWorldDialog = false;
            }
        }

        if (!canCreate) {
            ImGui::PopStyleVar();
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_showCreateWorldDialog = false;
        }

        ImGui::EndPopup();
    }

    ImGui::Separator();

    // Back button
    if (ImGui::Button("Back to Main Menu", ImVec2(300, 30))) {
        m_gameClient.setGameState(GameState::MAIN_MENU);
    }

    ImGui::End();
}

void GameClientUI::renderPauseMenu() {
    // Center the pause menu window
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x + viewport->Size.x * 0.5f, viewport->Pos.y + viewport->Size.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));

    ImGui::Begin("Game Paused", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::Text("Game Paused");
    ImGui::Separator();

    // Back to Game button
    if (ImGui::Button("Back to Game", ImVec2(200, 40))) {
        m_gameClient.setGameState(GameState::IN_GAME);
        if (m_gameClient.getInput()) {
            m_gameClient.getInput()->setMouseCaptured(true);
        }
    }

    // Open to LAN button (placeholder)
    if (ImGui::Button("Open to LAN", ImVec2(200, 40))) {
        std::cout << "Open to LAN not implemented yet!" << std::endl;
        // TODO: Implement LAN functionality
    }

    // Options button
    if (ImGui::Button("Options", ImVec2(200, 40))) {
        std::cout << "Opening options..." << std::endl;
        m_gameClient.setGameState(GameState::SETTINGS);
    }

    // Save and Quit to Title button
    if (ImGui::Button("Save and Quit to Title", ImVec2(200, 40))) {
        std::cout << "Saving and returning to main menu..." << std::endl;

        // Save the world and disconnect through GameClient
        m_gameClient.saveAndQuitToTitle();
    }

    ImGui::End();
}

void GameClientUI::renderLoadingScreen() {
    // Create a full-screen overlay
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);

    ImGui::Begin("LoadingOverlay", nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoInputs);

    // Get the draw list for custom drawing
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Fill the entire screen with a semi-transparent dark background
    drawList->AddRectFilled(
        ImVec2(0, 0),
        io.DisplaySize,
        IM_COL32(0, 0, 0, 200)  // Semi-transparent black
    );

    // Center the loading text and animation
    float centerX = io.DisplaySize.x * 0.5f;
    float centerY = io.DisplaySize.y * 0.5f;

    // Draw "Loading..." text
    const char* loadingText = "Loading...";
    float fontSize = ImGui::GetFontSize() * 2.0f;  // 2x size
    ImVec2 textSize = ImGui::CalcTextSize(loadingText);
    textSize.x *= 2.0f;  // Account for larger font size
    textSize.y *= 2.0f;

    // Position text above center with proper spacing
    ImVec2 textPos = ImVec2(
        centerX - textSize.x * 0.5f,
        centerY - 80.0f  // Move text higher above the animation
    );

    drawList->AddText(
        ImGui::GetFont(),
        fontSize,
        textPos,
        IM_COL32(255, 255, 255, 255),  // White text
        loadingText
    );

    // Add a simple spinning animation
    static float rotation = 0.0f;
    rotation += io.DeltaTime * 2.0f;  // Rotate speed
    if (rotation > 6.28f) rotation -= 6.28f;  // Keep within 2π

    // Draw a simple rotating circle - positioned below the text
    float circleRadius = 30.0f;
    ImVec2 circleCenter = ImVec2(centerX, centerY + 20.0f);  // Closer to center

    for (int i = 0; i < 8; i++) {
        float angle = rotation + (i * 6.28f / 8.0f);
        float alpha = (sin(angle) + 1.0f) * 0.5f;  // Fade effect
        ImVec2 dotPos = ImVec2(
            circleCenter.x + cos(angle) * circleRadius,
            circleCenter.y + sin(angle) * circleRadius
        );
        drawList->AddCircleFilled(
            dotPos, 4.0f,
            IM_COL32(255, 255, 255, (uint8_t)(alpha * 255))
        );
    }

    ImGui::End();
}

void GameClientUI::renderDebugWindow() {
    ImGui::Begin("Debug Control Panel", &m_showDebugWindow);

    ImGui::Text("Tidal Engine Client Debug Control");
    ImGui::Separator();

    // HUD toggles
    ImGui::Text("HUD Overlays:");
    ImGui::Checkbox("Performance Stats", &m_showPerformanceHUD);
    ImGui::Checkbox("Rendering Stats", &m_showRenderingHUD);
    ImGui::Checkbox("Camera Info", &m_showCameraHUD);
    ImGui::Separator();

    ImGui::Separator();

    // Client state
    ImGui::Text("Client Status:");
    ImGui::Text("  Connection: %s", m_gameClient.isConnected() ? "Connected" : "Disconnected");
    ImGui::Text("  Game State: %s", m_gameClient.getGameState() == GameState::IN_GAME ? "In Game" : "Menu");
    if (m_gameClient.getChunkManager()) {
        ImGui::Text("  Loaded Chunks: %zu", m_gameClient.getChunkManager()->getLoadedChunkCount());
    }
    ImGui::Separator();

    ImGui::Text("Controls:");
    ImGui::Text("  F1: Toggle this debug window");
    ImGui::Text("  F2: Toggle cursor capture (%s)", (m_gameClient.getInput() && m_gameClient.getInput()->isMouseCaptured()) ? "CAPTURED" : "FREE");
    ImGui::Text("  F3: Toggle wireframe mode (%s)", m_gameClient.isWireframeMode() ? "ON" : "OFF");
    ImGui::Text("  F4: Toggle chunk boundaries (%s)", m_showChunkBoundaries ? "ON" : "OFF");
    ImGui::Text("  ESC: Quit game");
    ImGui::Text("  WASD + Space/Shift: Move camera");
    ImGui::Text("  Ctrl + WASD: Fast movement (5x speed)");

    ImGui::End();
}

void GameClientUI::renderCrosshair() {
    // Only show crosshair when in game
    if (m_gameClient.getGameState() != GameState::IN_GAME) return;

    // Get the viewport size
    ImGuiIO& io = ImGui::GetIO();
    float centerX = io.DisplaySize.x * 0.5f;
    float centerY = io.DisplaySize.y * 0.5f;

    // Create an invisible window that covers the entire screen
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("CrosshairOverlay", nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoBackground);

    // Get the draw list
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Draw crosshair lines
    const float crosshairSize = 10.0f;
    const float thickness = 2.0f;
    const ImU32 color = IM_COL32(255, 255, 255, 200); // Semi-transparent white

    // Horizontal line
    drawList->AddLine(
        ImVec2(centerX - crosshairSize, centerY),
        ImVec2(centerX + crosshairSize, centerY),
        color, thickness);

    // Vertical line
    drawList->AddLine(
        ImVec2(centerX, centerY - crosshairSize),
        ImVec2(centerX, centerY + crosshairSize),
        color, thickness);

    ImGui::End();
}

void GameClientUI::renderHotbar() {
    // Get the viewport size
    ImGuiIO& io = ImGui::GetIO();
    float screenWidth = io.DisplaySize.x;
    float screenHeight = io.DisplaySize.y;

    // Hotbar dimensions
    const float slotSize = 50.0f;
    const float slotSpacing = 5.0f;
    const float windowPadding = 32.0f; // Account for ImGui window padding (increased)
    const int hotbarSlots = m_gameClient.getHotbarSlots();
    const float hotbarWidth = hotbarSlots * slotSize + (hotbarSlots - 1) * slotSpacing + windowPadding;
    const float hotbarHeight = slotSize + 20.0f; // Extra space for slot numbers

    // Position hotbar at bottom center of screen
    float hotbarX = (screenWidth - hotbarWidth) * 0.5f;
    float hotbarY = screenHeight - hotbarHeight - 20.0f; // 20px from bottom

    // Create hotbar window
    ImGui::SetNextWindowPos(ImVec2(hotbarX, hotbarY));
    ImGui::SetNextWindowSize(ImVec2(hotbarWidth, hotbarHeight));
    ImGui::Begin("Hotbar", nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBackground);

    // Get hotbar data
    const BlockType* hotbarBlocks = m_gameClient.getHotbarBlocks();
    int selectedSlot = m_gameClient.getSelectedHotbarSlot();
    TextureManager* textureManager = m_gameClient.getTextureManager();

    // Render hotbar slots
    for (int i = 0; i < hotbarSlots; i++) {
        if (i > 0) ImGui::SameLine();

        // Push style for selected slot
        bool isSelected = (i == selectedSlot);
        if (isSelected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.8f, 0.2f, 0.8f)); // Yellow highlight
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.9f, 0.3f, 0.8f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 0.8f)); // Dark gray
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5f, 0.5f, 0.5f, 0.8f));
        }

        // Get texture info for this block type
        BlockType blockType = hotbarBlocks[i];
        TextureInfo texInfo = textureManager->getBlockTexture(blockType, 0); // Use face 0

        // Create image button with UV coordinates from atlas
        ImVec2 uv0(texInfo.uvMin.x, texInfo.uvMin.y);
        ImVec2 uv1(texInfo.uvMax.x, texInfo.uvMax.y);

        std::string buttonId = "##hotbar_" + std::to_string(i);
        if (ImGui::ImageButton(buttonId.c_str(), (ImTextureID)getAtlasTextureDescriptor(), ImVec2(slotSize, slotSize), uv0, uv1)) {
            std::cout << "Hotbar clicked: changed from " << selectedSlot << " to " << i << std::endl;
            m_gameClient.setSelectedHotbarSlotFromUI(i);
        }

        // Pop style
        ImGui::PopStyleColor(2);

        // Show tooltip with block name on hover
        if (ImGui::IsItemHovered()) {
            BlockType blockType = hotbarBlocks[i];
            const char* blockName = "Unknown";

            switch (blockType) {
                case BlockType::STONE: blockName = "Stone"; break;
                case BlockType::DIRT: blockName = "Dirt"; break;
                case BlockType::GRASS: blockName = "Grass"; break;
                case BlockType::WOOD: blockName = "Wood"; break;
                case BlockType::SAND: blockName = "Sand"; break;
                case BlockType::BRICK: blockName = "Brick"; break;
                case BlockType::COBBLESTONE: blockName = "Cobblestone"; break;
                case BlockType::SNOW: blockName = "Snow"; break;
                default: break;
            }

            ImGui::SetTooltip("%s", blockName);
        }
    }

    ImGui::End();
}

void GameClientUI::renderPerformanceHUD() {
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.8f);

    ImGui::Begin("Performance", &m_showPerformanceHUD,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);

    const auto& debugInfo = m_gameClient.getDebugInfo();
    ImGui::Text("FPS: %.1f", debugInfo.fps);
    ImGui::Text("Frame Time: %.2f ms", debugInfo.frameTime);

    ImGui::End();
}

void GameClientUI::renderRenderingHUD() {
    ImGui::SetNextWindowPos(ImVec2(10, 100), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.8f);

    ImGui::Begin("Rendering", &m_showRenderingHUD,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);

    const auto& debugInfo = m_gameClient.getDebugInfo();
    ImGui::Text("Draw Calls: %u", debugInfo.drawCalls);
    ImGui::Text("Total Voxels: %u", debugInfo.totalVoxels);
    ImGui::Text("Rendered Voxels: %u", debugInfo.renderedVoxels);
    ImGui::Text("Culled Voxels: %u", debugInfo.culledVoxels);

    float cullingEfficiency = debugInfo.totalVoxels > 0 ?
        (float)debugInfo.culledVoxels / debugInfo.totalVoxels * 100.0f : 0.0f;
    ImGui::Text("Culling Efficiency: %.1f%%", cullingEfficiency);

    ImGui::End();
}

void GameClientUI::renderCameraHUD() {
    ImGui::SetNextWindowPos(ImVec2(10, 220), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.8f);

    ImGui::Begin("Camera", &m_showCameraHUD,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);

    const auto& camera = m_gameClient.getCamera();
    glm::vec3 pos = camera.getPosition();
    ImGui::Text("Position: (%.1f, %.1f, %.1f)", pos.x, pos.y, pos.z);
    ImGui::Text("Yaw: %.1f", camera.Yaw);
    ImGui::Text("Pitch: %.1f", camera.Pitch);

    ImGui::End();
}