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
    static std::vector<Vertex> getVertices();

    /**
     * @brief Get cube index data for triangle assembly
     * @return std::vector<uint32_t> Vector of 36 indices (12 triangles, 2 per face)
     */
    static std::vector<uint32_t> getIndices();
};

} // namespace engine
