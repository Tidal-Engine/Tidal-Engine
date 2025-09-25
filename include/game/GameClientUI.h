#pragma once

#include "game/GameClient.h" // For GameState enum and other types
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <vulkan/vulkan.h>

#include <vector>
#include <string>

// Forward declarations
class GameClient;
class VulkanDevice;

class GameClientUI {
public:
    GameClientUI(GameClient& gameClient, GLFWwindow* window, VulkanDevice& device);
    ~GameClientUI();

    // Initialization
    bool initialize();
    void shutdown();

    // Rendering
    void renderImGui(VkCommandBuffer commandBuffer);
    void renderMainMenu();
    void renderWorldSelection();
    void renderPauseMenu();
    void renderLoadingScreen();
    void renderDebugWindow();
    void renderCrosshair();
    void renderHotbar();
    void renderPerformanceHUD();
    void renderRenderingHUD();
    void renderCameraHUD();

    // UI state management
    void setShowDebugWindow(bool show) { m_showDebugWindow = show; }
    bool isShowingDebugWindow() const { return m_showDebugWindow; }
    void toggleDebugWindow() { m_showDebugWindow = !m_showDebugWindow; }

    void setShowPerformanceHUD(bool show) { m_showPerformanceHUD = show; }
    bool isShowingPerformanceHUD() const { return m_showPerformanceHUD; }

    void setShowRenderingHUD(bool show) { m_showRenderingHUD = show; }
    bool isShowingRenderingHUD() const { return m_showRenderingHUD; }

    void setShowCameraHUD(bool show) { m_showCameraHUD = show; }
    bool isShowingCameraHUD() const { return m_showCameraHUD; }

    void setShowChunkBoundaries(bool show) { m_showChunkBoundaries = show; }
    bool isShowingChunkBoundaries() const { return m_showChunkBoundaries; }

    void setShowCreateWorldDialog(bool show) { m_showCreateWorldDialog = show; }
    bool isShowingCreateWorldDialog() const { return m_showCreateWorldDialog; }

    // Atlas texture descriptor management
    void createAtlasTextureDescriptor();
    VkDescriptorSet getAtlasTextureDescriptor() const { return m_atlasTextureDescriptor; }

    // World management UI state
    char* getNewWorldNameBuffer() { return m_newWorldName; }
    static constexpr size_t NEW_WORLD_NAME_SIZE = 64;

private:
    GameClient& m_gameClient;
    GLFWwindow* m_window;
    VulkanDevice& m_device;

    // ImGui
    VkDescriptorPool m_imguiDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_atlasTextureDescriptor = VK_NULL_HANDLE;

    // UI state
    bool m_showDebugWindow = false;
    bool m_showPerformanceHUD = false;
    bool m_showRenderingHUD = false;
    bool m_showCameraHUD = false;
    bool m_showChunkBoundaries = false;
    bool m_showCreateWorldDialog = false;

    // World management
    char m_newWorldName[NEW_WORLD_NAME_SIZE] = {};

    // Initialization methods
    void initImGui();

    // Cleanup
    void cleanupImGui();
};