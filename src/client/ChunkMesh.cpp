#include "client/ChunkMesh.hpp"
#include "client/TextureAtlas.hpp"
#include "core/Logger.hpp"
#include <array>

namespace engine {

// Helper struct for greedy meshing mask
struct MaskCell {
    BlockType blockType = BlockType::Air;
    bool processed = false;
};

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
size_t ChunkMesh::generateMesh(const Chunk& chunk,
                               std::vector<Vertex>& vertices,
                               std::vector<uint32_t>& indices,
                               const TextureAtlas* atlas,
                               const Chunk* neighborNegX,
                               const Chunk* neighborPosX,
                               const Chunk* neighborNegY,
                               const Chunk* neighborPosY,
                               const Chunk* neighborNegZ,
                               const Chunk* neighborPosZ) {
    vertices.clear();
    indices.clear();

    const glm::vec3 CHUNK_ORIGIN = chunk.getCoord().toWorldPos();

    // Helper lambda to get block with cross-chunk support
    // NOLINTNEXTLINE(readability-identifier-length)
    auto getBlock = [&](int32_t x, int32_t y, int32_t z) -> const Block* {
        // Handle cross-chunk boundaries
        if (x < 0) {
            return neighborNegX ? &neighborNegX->getBlock(CHUNK_SIZE - 1, y, z) : nullptr;
        }
        if (x >= static_cast<int32_t>(CHUNK_SIZE)) {
            return neighborPosX ? &neighborPosX->getBlock(0, y, z) : nullptr;
        }
        if (y < 0) {
            return neighborNegY ? &neighborNegY->getBlock(x, CHUNK_SIZE - 1, z) : nullptr;
        }
        if (y >= static_cast<int32_t>(CHUNK_SIZE)) {
            return neighborPosY ? &neighborPosY->getBlock(x, 0, z) : nullptr;
        }
        if (z < 0) {
            return neighborNegZ ? &neighborNegZ->getBlock(x, y, CHUNK_SIZE - 1) : nullptr;
        }
        if (z >= static_cast<int32_t>(CHUNK_SIZE)) {
            return neighborPosZ ? &neighborPosZ->getBlock(x, y, 0) : nullptr;
        }
        return &chunk.getBlock(static_cast<uint32_t>(x), static_cast<uint32_t>(y), static_cast<uint32_t>(z));
    };

    // Greedy meshing algorithm - sweep in 6 directions
    // We'll process each axis (X, Y, Z) and each direction (negative, positive)

    // For each axis
    for (int axis = 0; axis < 3; axis++) {
        // NOLINTNEXTLINE(readability-identifier-length)
        const int U = (axis + 1) % 3;  // First tangent axis
        // NOLINTNEXTLINE(readability-identifier-length)
        const int V = (axis + 2) % 3;  // Second tangent axis

        // For each direction along this axis (backward = -1, forward = +1)
        for (int dir = -1; dir <= 1; dir += 2) {
            // Sweep through slices perpendicular to this axis
            // NOLINTNEXTLINE(readability-identifier-length)
            for (int32_t d = 0; d < static_cast<int32_t>(CHUNK_SIZE); d++) {
                // Build mask for this slice
                std::array<MaskCell, static_cast<std::size_t>(CHUNK_SIZE) * CHUNK_SIZE> mask{};

                // Scan the slice and build mask
                for (uint32_t j = 0; j < CHUNK_SIZE; j++) {
                    for (uint32_t i = 0; i < CHUNK_SIZE; i++) {
                        // Compute position in chunk space
                        int32_t pos[3] = {0, 0, 0};
                        pos[axis] = d;
                        pos[U] = static_cast<int32_t>(i);
                        pos[V] = static_cast<int32_t>(j);

                        const Block* current = getBlock(pos[0], pos[1], pos[2]);

                        // Position of neighbor in the direction we're checking
                        int32_t neighborPos[3] = {pos[0], pos[1], pos[2]};
                        neighborPos[axis] += dir;
                        const Block* neighbor = getBlock(neighborPos[0], neighborPos[1], neighborPos[2]);

                        // Determine if we need a face here
                        bool needsFace = false;
                        BlockType faceBlockType = BlockType::Air;

                        // NOLINTBEGIN(bugprone-branch-clone)
                        if (current != nullptr && current->isSolid()) {
                            // We have a solid block, check if neighbor blocks it
                            if (neighbor == nullptr || !neighbor->isSolid()) {
                                // Neighbor is air or doesn't exist - need face
                                needsFace = true;
                                faceBlockType = current->type;
                            } else if (neighbor->type != current->type) {
                                // Neighbor is different type - need face
                                needsFace = true;
                                faceBlockType = current->type;
                            }
                        }
                        // NOLINTEND(bugprone-branch-clone)

                        if (needsFace) {
                            mask[i + (j * CHUNK_SIZE)].blockType = faceBlockType;
                        }
                    }
                }

                // Greedy merge the mask into quads
                for (uint32_t j = 0; j < CHUNK_SIZE; j++) {
                    for (uint32_t i = 0; i < CHUNK_SIZE;) {
                        MaskCell& cell = mask[i + (j * CHUNK_SIZE)];

                        if (cell.blockType == BlockType::Air || cell.processed) {
                            i++;
                            continue;
                        }

                        // Found a face to mesh - determine width
                        uint32_t width = 1;
                        while (i + width < CHUNK_SIZE) {
                            MaskCell& nextCell = mask[(i + width) + (j * CHUNK_SIZE)];
                            if (nextCell.blockType != cell.blockType || nextCell.processed) {
                                break;
                            }
                            width++;
                        }

                        // Determine height with this width
                        uint32_t height = 1;
                        bool done = false;
                        while (j + height < CHUNK_SIZE && !done) {
                            // Check if entire row matches
                            // NOLINTNEXTLINE(readability-identifier-length)
                            for (uint32_t k = 0; k < width; k++) {
                                MaskCell& checkCell = mask[(i + k) + ((j + height) * CHUNK_SIZE)];
                                if (checkCell.blockType != cell.blockType || checkCell.processed) {
                                    done = true;
                                    break;
                                }
                            }
                            if (!done) {
                                height++;
                            }
                        }

                        // Mark all cells in this quad as processed
                        // NOLINTNEXTLINE(readability-identifier-length)
                        for (uint32_t h = 0; h < height; h++) {
                            // NOLINTNEXTLINE(readability-identifier-length)
                            for (uint32_t w = 0; w < width; w++) {
                                mask[(i + w) + ((j + h) * CHUNK_SIZE)].processed = true;
                            }
                        }

                        // Generate the merged quad
                        int32_t pos[3] = {0, 0, 0};
                        pos[axis] = d;
                        pos[U] = static_cast<int32_t>(i);
                        pos[V] = static_cast<int32_t>(j);

                        // Adjust position for forward-facing quads
                        if (dir > 0) {
                            pos[axis] += 1;
                        }

                        glm::vec3 quadPos = CHUNK_ORIGIN + glm::vec3(static_cast<float>(pos[0]), static_cast<float>(pos[1]), static_cast<float>(pos[2]));
                        glm::vec3 size(0, 0, 0);
                        size[U] = static_cast<float>(width);
                        size[V] = static_cast<float>(height);

                        glm::vec3 normal(0, 0, 0);
                        normal[axis] = static_cast<float>(dir);

                        glm::vec3 color = getBlockColor(cell.blockType, normal);

                        addQuad(vertices, indices, quadPos, size, normal, color, cell.blockType, atlas);

                        i += width;
                    }
                }
            }
        }
    }

    LOG_TRACE("Generated greedy mesh for chunk ({}, {}, {}) | {} vertices, {} indices",
              chunk.getCoord().x, chunk.getCoord().y, chunk.getCoord().z,
              vertices.size(), indices.size());

    return vertices.size();
}

glm::vec3 ChunkMesh::getBlockColor(BlockType type, const glm::vec3& normal) {
    // Special handling for grass blocks - only tint the top face
    // NOLINTBEGIN(cppcoreguidelines-pro-type-union-access)
    if (type == BlockType::Grass && normal.y > 0.5f) {
        // Top face: green tint for grass_top texture
        return glm::vec3(0.4f, 0.8f, 0.3f);  // Green
    }
    // NOLINTEND(cppcoreguidelines-pro-type-union-access)

    // All other blocks use white (no tinting)
    return glm::vec3(1.0f, 1.0f, 1.0f);
}

void ChunkMesh::addQuad(std::vector<Vertex>& vertices,
                        std::vector<uint32_t>& indices,
                        const glm::vec3& position,
                        const glm::vec3& size,
                        const glm::vec3& normal,
                        const glm::vec3& color,
                        BlockType blockType,
                        const TextureAtlas* atlas) {
    uint32_t baseIndex = static_cast<uint32_t>(vertices.size());

    // Determine tangent and bitangent from normal
    // NOLINTBEGIN(cppcoreguidelines-pro-type-union-access)
    glm::vec3 tangent;
    glm::vec3 bitangent;
    if (std::abs(normal.x) > 0.5f) {
        // X-facing
        tangent = glm::vec3(0, size.y, 0);
        bitangent = glm::vec3(0, 0, size.z);
    } else if (std::abs(normal.y) > 0.5f) {
        // Y-facing
        tangent = glm::vec3(size.x, 0, 0);
        bitangent = glm::vec3(0, 0, size.z);
    } else {
        // Z-facing
        tangent = glm::vec3(size.x, 0, 0);
        bitangent = glm::vec3(0, size.y, 0);
    }

    // Get base UV coordinates from atlas
    glm::vec2 uvMin;
    glm::vec2 uvMax;
    glm::vec2 uvBlockSize;
    if (atlas != nullptr) {
        BlockType texBlockType = blockType;

        // Special handling for grass blocks
        if (blockType == BlockType::Grass) {
            if (normal.y > 0.5f) {
                // Top face (+Y): use grass_top texture
                texBlockType = BlockType::GrassTop;
            } else if (normal.y < -0.5f) {
                // Bottom face (-Y): use dirt texture
                texBlockType = BlockType::Dirt;
            } else {
                // Side faces: use grass_side texture
                texBlockType = BlockType::GrassSide;
            }
        }

        glm::vec4 uvs = atlas->getBlockUVs(texBlockType);
        uvMin = glm::vec2(uvs.x, uvs.y);
        uvMax = glm::vec2(uvs.z, uvs.w);
        uvBlockSize = uvMax - uvMin; // Size of one block's texture in UV space
    } else {
        // Default UV coordinates
        uvMin = glm::vec2(0.0f, 0.0f);
        uvMax = glm::vec2(1.0f, 1.0f);
        uvBlockSize = glm::vec2(1.0f, 1.0f);
    }

    // Calculate the width and height of this quad in blocks
    float widthInBlocks = 0.0f;
    float heightInBlocks = 0.0f;

    if (std::abs(normal.x) > 0.5f) {
        // X-facing (rotated 90°): texture width maps to horizontal (Z), height to vertical (Y)
        widthInBlocks = size.z;
        heightInBlocks = size.y;
    } else if (std::abs(normal.y) > 0.5f) {
        // Y-facing: width=X, height=Z
        widthInBlocks = size.x;
        heightInBlocks = size.z;
    } else {
        // Z-facing: width=X, height=Y
        widthInBlocks = size.x;
        heightInBlocks = size.y;
    }

    // For greedy meshing: pass normalized UVs (0 to width, 0 to height)
    // The fragment shader will use fract() to tile within the atlas region
    glm::vec2 uvTiled = glm::vec2(widthInBlocks, heightInBlocks);

    // Rotate UVs 90 degrees for X-facing sides
    bool rotateUVs = std::abs(normal.x) > 0.5f;
    // NOLINTEND(cppcoreguidelines-pro-type-union-access)

    // Create quad vertices with tiled UVs and atlas info
    // The fragment shader will remap: atlasOffset + fract(texCoord) * atlasSize
    // NOLINTBEGIN(cppcoreguidelines-pro-type-union-access)
    if (rotateUVs) {
        // Rotated UVs for X-facing: U along bitangent (horizontal), V along tangent (vertical)
        // Flip V coordinate to fix upside-down textures
        vertices.push_back({position, color, normal, glm::vec2(0.0f, uvTiled.y), uvMin, uvBlockSize});
        vertices.push_back({position + tangent, color, normal, glm::vec2(0.0f, 0.0f), uvMin, uvBlockSize});
        vertices.push_back({position + tangent + bitangent, color, normal, glm::vec2(uvTiled.x, 0.0f), uvMin, uvBlockSize});
        vertices.push_back({position + bitangent, color, normal, uvTiled, uvMin, uvBlockSize});
    } else {
        // Flip V coordinate to fix upside-down textures
        vertices.push_back({position, color, normal, glm::vec2(0.0f, uvTiled.y), uvMin, uvBlockSize});
        vertices.push_back({position + tangent, color, normal, uvTiled, uvMin, uvBlockSize});
        vertices.push_back({position + tangent + bitangent, color, normal, glm::vec2(uvTiled.x, 0.0f), uvMin, uvBlockSize});
        vertices.push_back({position + bitangent, color, normal, glm::vec2(0.0f, 0.0f), uvMin, uvBlockSize});
    }
    // NOLINTEND(cppcoreguidelines-pro-type-union-access)

    // Create two triangles (counter-clockwise winding)
    indices.push_back(baseIndex);
    indices.push_back(baseIndex + 1);
    indices.push_back(baseIndex + 2);

    indices.push_back(baseIndex);
    indices.push_back(baseIndex + 2);
    indices.push_back(baseIndex + 3);
}

} // namespace engine
