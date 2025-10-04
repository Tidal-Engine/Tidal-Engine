#pragma once

#include "shared/Chunk.hpp"
#include "vulkan/Vertex.hpp"
#include <vector>
#include <glm/glm.hpp>
#include <functional>

namespace engine {

// Forward declare
class TextureAtlas;

/**
 * @brief Generates renderable mesh from chunk block data
 *
 * Uses greedy meshing algorithm to minimize vertex count by merging
 * adjacent faces of the same block type into larger quads.
 */
class ChunkMesh {
public:
    /**
     * @brief Generate mesh from chunk data
     * @param chunk Chunk to generate mesh for
     * @param vertices Output vertex buffer
     * @param indices Output index buffer
     * @param atlas Texture atlas for UV coordinates (optional)
     * @param neighborNegX Neighboring chunk in -X direction (optional, for cross-chunk culling)
     * @param neighborPosX Neighboring chunk in +X direction (optional, for cross-chunk culling)
     * @param neighborNegY Neighboring chunk in -Y direction (optional, for cross-chunk culling)
     * @param neighborPosY Neighboring chunk in +Y direction (optional, for cross-chunk culling)
     * @param neighborNegZ Neighboring chunk in -Z direction (optional, for cross-chunk culling)
     * @param neighborPosZ Neighboring chunk in +Z direction (optional, for cross-chunk culling)
     * @return Number of vertices generated
     */
    static size_t generateMesh(const Chunk& chunk,
                              std::vector<Vertex>& vertices,
                              std::vector<uint32_t>& indices,
                              const TextureAtlas* atlas = nullptr,
                              const Chunk* neighborNegX = nullptr,
                              const Chunk* neighborPosX = nullptr,
                              const Chunk* neighborNegY = nullptr,
                              const Chunk* neighborPosY = nullptr,
                              const Chunk* neighborNegZ = nullptr,
                              const Chunk* neighborPosZ = nullptr);

    /**
     * @brief Get color tint for a block type (used for vertex colors alongside textures)
     * @param type Block type to get color for
     * @param normal Face normal direction (used for grass top face detection)
     */
    static glm::vec3 getBlockColor(BlockType type, const glm::vec3& normal);

private:
    /**
     * @brief Add a quad face to the mesh
     */
    static void addQuad(std::vector<Vertex>& vertices,
                       std::vector<uint32_t>& indices,
                       const glm::vec3& position,
                       const glm::vec3& size,
                       const glm::vec3& normal,
                       const glm::vec3& color,
                       BlockType blockType,
                       const TextureAtlas* atlas);
};

} // namespace engine
