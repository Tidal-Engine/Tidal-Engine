#include "client/DebugOverlay.hpp"
#include "client/Camera.hpp"
#include "client/NetworkClient.hpp"
#include "core/PerformanceMetrics.hpp"
#include "core/Logger.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <cmath>

namespace engine {

DebugOverlay::DebugOverlay() {
    fpsHistory.resize(FPS_HISTORY_SIZE, 0.0f);
    LOG_DEBUG("DebugOverlay initialized");
}

void DebugOverlay::toggle() {
    isVisible = !isVisible;
}

void DebugOverlay::render(const Camera* camera,
                         const PerformanceMetrics* metrics,
                         const NetworkClient* networkClient,
                         uint32_t chunksVisible,
                         uint32_t chunksTotal,
                         uint32_t verticesRendered,
                         uint32_t drawCalls) {
    if (!isVisible) {
        return;
    }

    // Set overlay position and style
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.35f); // Transparent background

    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDecoration |
                                   ImGuiWindowFlags_AlwaysAutoResize |
                                   ImGuiWindowFlags_NoSavedSettings |
                                   ImGuiWindowFlags_NoFocusOnAppearing |
                                   ImGuiWindowFlags_NoNav;

    if (ImGui::Begin("Debug Overlay", &isVisible, windowFlags)) {
        ImGui::Text("Tidal Engine Debug (F3)");
        ImGui::Separator();

        renderCameraInfo(camera);
        ImGui::Separator();

        renderRenderingStats(chunksVisible, chunksTotal, verticesRendered, drawCalls);
        ImGui::Separator();

        renderPerformanceInfo(metrics);
        ImGui::Separator();

        renderNetworkInfo(networkClient);
    }
    ImGui::End();
}

void DebugOverlay::renderCameraInfo(const Camera* camera) {
    if (!camera) {
        LOG_WARN("DebugOverlay: Camera pointer is null");
        return;
    }

    ImGui::Text("Camera");

    glm::vec3 pos = camera->getPosition();
    ImGui::Text("  Position: %.2f, %.2f, %.2f", pos.x, pos.y, pos.z);

    glm::vec3 front = camera->getFront();

    // Calculate yaw and pitch from front vector
    float yaw = glm::degrees(std::atan2(front.z, front.x));
    float pitch = glm::degrees(std::asin(-front.y));

    ImGui::Text("  Rotation: Yaw %.1f, Pitch %.1f", yaw, pitch);
    ImGui::Text("  Direction: %.2f, %.2f, %.2f", front.x, front.y, front.z);

    // Chunk coordinates
    int chunkX = static_cast<int>(std::floor(pos.x / 16.0f));
    int chunkY = static_cast<int>(std::floor(pos.y / 16.0f));
    int chunkZ = static_cast<int>(std::floor(pos.z / 16.0f));
    ImGui::Text("  Chunk: %d, %d, %d", chunkX, chunkY, chunkZ);
}

void DebugOverlay::renderRenderingStats(uint32_t chunksVisible, uint32_t chunksTotal,
                                        uint32_t verticesRendered, uint32_t drawCalls) {
    ImGui::Text("Rendering");
    ImGui::Text("  Chunks: %u visible / %u loaded", chunksVisible, chunksTotal);

    if (chunksTotal > 0) {
        float cullPercentage = 100.0f * (1.0f - static_cast<float>(chunksVisible) / static_cast<float>(chunksTotal));
        ImGui::Text("  Culled: %.1f%%", cullPercentage);
    }

    ImGui::Text("  Draw calls: %u", drawCalls);
    ImGui::Text("  Vertices: %s", formatNumber(verticesRendered).c_str());
    ImGui::Text("  Triangles: %s", formatNumber(verticesRendered / 3).c_str());
}

void DebugOverlay::renderPerformanceInfo(const PerformanceMetrics* metrics) {
    if (!metrics) {
        LOG_WARN("DebugOverlay: PerformanceMetrics pointer is null");
        return;
    }

    ImGui::Text("Performance");

    float fps = static_cast<float>(metrics->getFPS());
    ImGui::Text("  FPS: %.1f", fps);

    // Update FPS history for graph
    fpsHistory[fpsHistoryIndex] = fps;
    fpsHistoryIndex = (fpsHistoryIndex + 1) % FPS_HISTORY_SIZE;

    // Draw FPS graph
    ImGui::PlotLines("",
                     fpsHistory.data(),
                     static_cast<int>(fpsHistory.size()),
                     static_cast<int>(fpsHistoryIndex),
                     nullptr,
                     0.0f,
                     300.0f,
                     ImVec2(200, 50));

    ImGui::Text("  Frame time: %.2f ms (avg)", metrics->getAverageFrameTime());
    ImGui::Text("  Min: %.2f ms, Max: %.2f ms",
                metrics->getMinFrameTime(),
                metrics->getMaxFrameTime());
}

void DebugOverlay::renderNetworkInfo(const NetworkClient* networkClient) {
    if (!networkClient) {
        LOG_WARN("DebugOverlay: NetworkClient pointer is null");
        return;
    }

    ImGui::Text("Network");

    if (networkClient->isConnected()) {
        ImGui::Text("  Status: Connected");
        ImGui::Text("  Server: 127.0.0.1:25565");
        // Future: Add ping latency, packets sent/received, bandwidth when needed for debugging
    } else {
        ImGui::Text("  Status: Disconnected");
    }
}

std::string DebugOverlay::formatNumber(uint32_t num) {
    if (num >= 1000000) {
        return std::to_string(num / 1000000) + "." + std::to_string((num / 100000) % 10) + "M";
    } else if (num >= 1000) {
        return std::to_string(num / 1000) + "." + std::to_string((num / 100) % 10) + "K";
    }
    return std::to_string(num);
}

void DebugOverlay::renderCrosshair() {
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);

    ImDrawList* drawList = ImGui::GetForegroundDrawList();

    // Crosshair parameters
    const float crosshairSize = 10.0f;
    const float crosshairThickness = 2.0f;
    const float crosshairGap = 3.0f;
    const ImU32 crosshairColor = IM_COL32(255, 255, 255, 200); // White with slight transparency
    const ImU32 outlineColor = IM_COL32(0, 0, 0, 150); // Black outline

    // Draw outline (thicker, black lines)
    // Horizontal line (left)
    drawList->AddLine(
        ImVec2(center.x - crosshairSize - crosshairGap, center.y),
        ImVec2(center.x - crosshairGap, center.y),
        outlineColor,
        crosshairThickness + 2.0f
    );
    // Horizontal line (right)
    drawList->AddLine(
        ImVec2(center.x + crosshairGap, center.y),
        ImVec2(center.x + crosshairSize + crosshairGap, center.y),
        outlineColor,
        crosshairThickness + 2.0f
    );
    // Vertical line (top)
    drawList->AddLine(
        ImVec2(center.x, center.y - crosshairSize - crosshairGap),
        ImVec2(center.x, center.y - crosshairGap),
        outlineColor,
        crosshairThickness + 2.0f
    );
    // Vertical line (bottom)
    drawList->AddLine(
        ImVec2(center.x, center.y + crosshairGap),
        ImVec2(center.x, center.y + crosshairSize + crosshairGap),
        outlineColor,
        crosshairThickness + 2.0f
    );

    // Draw crosshair (white lines on top)
    // Horizontal line (left)
    drawList->AddLine(
        ImVec2(center.x - crosshairSize - crosshairGap, center.y),
        ImVec2(center.x - crosshairGap, center.y),
        crosshairColor,
        crosshairThickness
    );
    // Horizontal line (right)
    drawList->AddLine(
        ImVec2(center.x + crosshairGap, center.y),
        ImVec2(center.x + crosshairSize + crosshairGap, center.y),
        crosshairColor,
        crosshairThickness
    );
    // Vertical line (top)
    drawList->AddLine(
        ImVec2(center.x, center.y - crosshairSize - crosshairGap),
        ImVec2(center.x, center.y - crosshairGap),
        crosshairColor,
        crosshairThickness
    );
    // Vertical line (bottom)
    drawList->AddLine(
        ImVec2(center.x, center.y + crosshairGap),
        ImVec2(center.x, center.y + crosshairSize + crosshairGap),
        crosshairColor,
        crosshairThickness
    );
}

} // namespace engine
