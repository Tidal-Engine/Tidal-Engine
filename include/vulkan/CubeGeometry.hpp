#pragma once

#include "vulkan/Vertex.hpp"
#include <vector>

namespace engine {

/**
 * @brief Provides static geometry data for a rainbow-colored cube
 *
 * Contains vertex positions, colors, normals, and triangle indices
 * for rendering a 3D cube with different colors on each face.
 */
class CubeGeometry {
public:
    /**
     * @brief Get cube vertex data
     * @return std::vector<Vertex> Vector of 24 vertices (4 per face) with rainbow colors
     */
    static std::vector<Vertex> getVertices() {
        return {
            // Front face (red)
            {{-0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
            {{ 0.5f, -0.5f,  0.5f}, {1.0f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}},
            {{ 0.5f,  0.5f,  0.5f}, {1.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
            {{-0.5f,  0.5f,  0.5f}, {0.5f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},

            // Back face (green)
            {{ 0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
            {{-0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.5f}, {0.0f, 0.0f, -1.0f}},
            {{-0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
            {{ 0.5f,  0.5f, -0.5f}, {0.0f, 0.5f, 1.0f}, {0.0f, 0.0f, -1.0f}},

            // Top face (blue)
            {{-0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
            {{ 0.5f,  0.5f,  0.5f}, {0.5f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
            {{ 0.5f,  0.5f, -0.5f}, {1.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
            {{-0.5f,  0.5f, -0.5f}, {1.0f, 0.0f, 0.5f}, {0.0f, 1.0f, 0.0f}},

            // Bottom face (cyan)
            {{-0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}},
            {{ 0.5f, -0.5f, -0.5f}, {0.5f, 1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}},
            {{ 0.5f, -0.5f,  0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}},
            {{-0.5f, -0.5f,  0.5f}, {1.0f, 0.5f, 1.0f}, {0.0f, -1.0f, 0.0f}},

            // Right face (magenta)
            {{ 0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
            {{ 0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.5f}, {1.0f, 0.0f, 0.0f}},
            {{ 0.5f,  0.5f, -0.5f}, {0.5f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
            {{ 0.5f,  0.5f,  0.5f}, {0.5f, 0.5f, 1.0f}, {1.0f, 0.0f, 0.0f}},

            // Left face (yellow)
            {{-0.5f, -0.5f, -0.5f}, {1.0f, 1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}},
            {{-0.5f, -0.5f,  0.5f}, {1.0f, 1.0f, 0.5f}, {-1.0f, 0.0f, 0.0f}},
            {{-0.5f,  0.5f,  0.5f}, {0.5f, 1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},
            {{-0.5f,  0.5f, -0.5f}, {0.5f, 0.5f, 1.0f}, {-1.0f, 0.0f, 0.0f}},
        };
    }

    /**
     * @brief Get cube index data for triangle assembly
     * @return std::vector<uint16_t> Vector of 36 indices (12 triangles, 2 per face)
     */
    static std::vector<uint16_t> getIndices() {
        return {
            // Front
            0, 1, 2, 2, 3, 0,
            // Back
            4, 5, 6, 6, 7, 4,
            // Top
            8, 9, 10, 10, 11, 8,
            // Bottom
            12, 13, 14, 14, 15, 12,
            // Right
            16, 17, 18, 18, 19, 16,
            // Left
            20, 21, 22, 22, 23, 20
        };
    }
};

} // namespace engine
