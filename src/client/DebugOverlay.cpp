#include "client/DebugOverlay.hpp"
#include "client/Camera.hpp"
#include "client/NetworkClient.hpp"
#include "client/Raycaster.hpp"
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

// NOLINTBEGIN(cppcoreguidelines-pro-type-vararg, cppcoreguidelines-pro-type-union-access)
void DebugOverlay::render(const Camera* camera,
                         const PerformanceMetrics* metrics,
                         const NetworkClient* networkClient,
                         uint32_t chunksVisible,
                         uint32_t chunksTotal,
                         uint32_t verticesRendered,
                         uint32_t drawCalls,
                         const std::optional<RaycastHit>* targetedBlock) {
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

        // Targeted block info
        if (targetedBlock && targetedBlock->has_value()) {
            const auto& hit = targetedBlock->value();
            ImGui::Text("Targeted Block");
            ImGui::Text("  Position: %d, %d, %d", hit.blockPos.x, hit.blockPos.y, hit.blockPos.z);
            const char* blockTypeName = "Unknown";
            if (hit.blockType == BlockType::Stone) {
                blockTypeName = "Stone";
            } else if (hit.blockType == BlockType::Dirt) {
                blockTypeName = "Dirt";
            }
            ImGui::Text("  Type: %s", blockTypeName);
            ImGui::Text("  Distance: %.2f", hit.distance);
            ImGui::Text("  Face: %d, %d, %d", hit.normal.x, hit.normal.y, hit.normal.z);
            ImGui::Separator();
        }

        renderRenderingStats(chunksVisible, chunksTotal, verticesRendered, drawCalls);
        ImGui::Separator();

        renderPerformanceInfo(metrics);
        ImGui::Separator();

        renderNetworkInfo(networkClient);
    }
    ImGui::End();
}
// NOLINTEND(cppcoreguidelines-pro-type-vararg, cppcoreguidelines-pro-type-union-access)

// NOLINTBEGIN(cppcoreguidelines-pro-type-vararg, cppcoreguidelines-pro-type-union-access, readability-convert-member-functions-to-static)
void DebugOverlay::renderCameraInfo(const Camera* camera) {
    if (!camera) {
        LOG_WARN("DebugOverlay: Camera pointer is null");
        return;
    }

    ImGui::Text("Camera");

    glm::vec3 pos = camera->getPosition();

    // Block coordinates (whole numbers)
    int blockX = static_cast<int>(std::floor(pos.x));
    int blockY = static_cast<int>(std::floor(pos.y));
    int blockZ = static_cast<int>(std::floor(pos.z));
    ImGui::Text("  Block: %d, %d, %d", blockX, blockY, blockZ);
    ImGui::Text("  Position: %.2f, %.2f, %.2f", pos.x, pos.y, pos.z);

    glm::vec3 front = camera->getFront();

    // Calculate yaw and pitch from front vector
    float yaw = glm::degrees(std::atan2(front.z, front.x));
    float pitch = glm::degrees(std::asin(-front.y));

    ImGui::Text("  Rotation: Yaw %.1f, Pitch %.1f", yaw, pitch);
    ImGui::Text("  Direction: %.2f, %.2f, %.2f", front.x, front.y, front.z);

    // Chunk coordinates (chunks are 32x32x32)
    int chunkX = static_cast<int>(std::floor(pos.x / 32.0f));
    int chunkY = static_cast<int>(std::floor(pos.y / 32.0f));
    int chunkZ = static_cast<int>(std::floor(pos.z / 32.0f));
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

    // Draw FPS graph (use ## to hide label but provide unique ID)
    ImGui::PlotLines("##fpsGraph",
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
    }
    if (num >= 1000) {
        return std::to_string(num / 1000) + "." + std::to_string((num / 100) % 10) + "K";
    }
    return std::to_string(num);
}

void DebugOverlay::renderCrosshair() {
    // NOLINTNEXTLINE(readability-identifier-length)
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);

    ImDrawList* drawList = ImGui::GetForegroundDrawList();

    // Crosshair parameters
    const float CROSSHAIR_SIZE = 10.0f;
    const float CROSSHAIR_THICKNESS = 2.0f;
    const float CROSSHAIR_GAP = 3.0f;
    const ImU32 CROSSHAIR_COLOR = IM_COL32(255, 255, 255, 200); // White with slight transparency
    const ImU32 OUTLINE_COLOR = IM_COL32(0, 0, 0, 150); // Black outline

    // Draw outline (thicker, black lines)
    // Horizontal line (left)
    drawList->AddLine(
        ImVec2(center.x - CROSSHAIR_SIZE - CROSSHAIR_GAP, center.y),
        ImVec2(center.x - CROSSHAIR_GAP, center.y),
        OUTLINE_COLOR,
        CROSSHAIR_THICKNESS + 2.0f
    );
    // Horizontal line (right)
    drawList->AddLine(
        ImVec2(center.x + CROSSHAIR_GAP, center.y),
        ImVec2(center.x + CROSSHAIR_SIZE + CROSSHAIR_GAP, center.y),
        OUTLINE_COLOR,
        CROSSHAIR_THICKNESS + 2.0f
    );
    // Vertical line (top)
    drawList->AddLine(
        ImVec2(center.x, center.y - CROSSHAIR_SIZE - CROSSHAIR_GAP),
        ImVec2(center.x, center.y - CROSSHAIR_GAP),
        OUTLINE_COLOR,
        CROSSHAIR_THICKNESS + 2.0f
    );
    // Vertical line (bottom)
    drawList->AddLine(
        ImVec2(center.x, center.y + CROSSHAIR_GAP),
        ImVec2(center.x, center.y + CROSSHAIR_SIZE + CROSSHAIR_GAP),
        OUTLINE_COLOR,
        CROSSHAIR_THICKNESS + 2.0f
    );

    // Draw crosshair (white lines on top)
    // Horizontal line (left)
    drawList->AddLine(
        ImVec2(center.x - CROSSHAIR_SIZE - CROSSHAIR_GAP, center.y),
        ImVec2(center.x - CROSSHAIR_GAP, center.y),
        CROSSHAIR_COLOR,
        CROSSHAIR_THICKNESS
    );
    // Horizontal line (right)
    drawList->AddLine(
        ImVec2(center.x + CROSSHAIR_GAP, center.y),
        ImVec2(center.x + CROSSHAIR_SIZE + CROSSHAIR_GAP, center.y),
        CROSSHAIR_COLOR,
        CROSSHAIR_THICKNESS
    );
    // Vertical line (top)
    drawList->AddLine(
        ImVec2(center.x, center.y - CROSSHAIR_SIZE - CROSSHAIR_GAP),
        ImVec2(center.x, center.y - CROSSHAIR_GAP),
        CROSSHAIR_COLOR,
        CROSSHAIR_THICKNESS
    );
    // Vertical line (bottom)
    drawList->AddLine(
        ImVec2(center.x, center.y + CROSSHAIR_GAP),
        ImVec2(center.x, center.y + CROSSHAIR_SIZE + CROSSHAIR_GAP),
        CROSSHAIR_COLOR,
        CROSSHAIR_THICKNESS
    );
}
// NOLINTEND(cppcoreguidelines-pro-type-vararg, cppcoreguidelines-pro-type-union-access, readability-convert-member-functions-to-static)

} // namespace engine
