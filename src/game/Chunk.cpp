#include "Chunk.h"
#include "ChunkManager.h"
#include "TextureManager.h"
#include <cstring>
#include <array>

// ChunkVertex static methods (moved from Application.h)
VkVertexInputBindingDescription ChunkVertex::getBindingDescription() {
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(ChunkVertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return bindingDescription;
}

std::array<VkVertexInputAttributeDescription, 5> ChunkVertex::getAttributeDescriptions() {
    std::array<VkVertexInputAttributeDescription, 5> attributeDescriptions{};

    // Position
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(ChunkVertex, pos);

    // Normal
    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(ChunkVertex, normal);

    // Texture coordinates
    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[2].offset = offsetof(ChunkVertex, texCoord);

    // World offset
    attributeDescriptions[3].binding = 0;
    attributeDescriptions[3].location = 3;
    attributeDescriptions[3].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[3].offset = offsetof(ChunkVertex, worldOffset);

    // Color
    attributeDescriptions[4].binding = 0;
    attributeDescriptions[4].location = 4;
    attributeDescriptions[4].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[4].offset = offsetof(ChunkVertex, color);

    return attributeDescriptions;
}

Chunk::Chunk(const ChunkPos& position, VulkanDevice& device, ChunkManager* chunkManager, TextureManager* textureManager)
    : m_position(position), m_device(device), m_chunkManager(chunkManager), m_textureManager(textureManager) {
    // Initialize all voxels to AIR (empty)
    std::memset(m_voxels, static_cast<uint8_t>(BlockType::AIR), sizeof(m_voxels));

    // Generate initial terrain
    generateTerrain();
}

void Chunk::generateTerrain() {
    // Generate simple flat world like Minecraft flatland
    for (int x = 0; x < SIZE; x++) {
        for (int y = 0; y < SIZE; y++) {
            for (int z = 0; z < SIZE; z++) {
                // Create terrain based on absolute world Y position
                int worldY = m_position.y * SIZE + y;

                if (worldY <= 16) {
                    // Solid ground - stone for now
                    m_voxels[x][y][z] = BlockType::STONE;
                } else {
                    // Air above Y=16
                    m_voxels[x][y][z] = BlockType::AIR;
                }
            }
        }
    }
    m_meshDirty = true;
}

bool Chunk::isVoxelSolid(int x, int y, int z) const {
    // Check bounds
    if (x < 0 || x >= SIZE || y < 0 || y >= SIZE || z < 0 || z >= SIZE) {
        return false;  // Out of bounds is considered air
    }
    return m_voxels[x][y][z] != BlockType::AIR;
}

BlockType Chunk::getVoxelType(int x, int y, int z) const {
    // Check bounds
    if (x < 0 || x >= SIZE || y < 0 || y >= SIZE || z < 0 || z >= SIZE) {
        return BlockType::AIR;  // Out of bounds is considered air
    }
    return m_voxels[x][y][z];
}

void Chunk::setVoxel(int x, int y, int z, BlockType blockType) {
    if (x >= 0 && x < SIZE && y >= 0 && y < SIZE && z >= 0 && z < SIZE) {
        if (m_voxels[x][y][z] != blockType) {
            m_voxels[x][y][z] = blockType;
            m_meshDirty = true;
        }
    }
}

void Chunk::setVoxel(int x, int y, int z, bool solid) {
    // Legacy compatibility - use stone for solid, air for empty
    setVoxel(x, y, z, solid ? BlockType::STONE : BlockType::AIR);
}

glm::vec2 Chunk::getTextureCoords(BlockType blockType) const {
    if (m_textureManager) {
        // Use TextureManager for proper atlas mapping
        TextureInfo texInfo = m_textureManager->getBlockTexture(blockType);
        return texInfo.uvMin;
    }

    // Fallback to old hardcoded mapping if no TextureManager
    const float texSize = 0.25f;
    int texIndex = static_cast<int>(blockType) - 1;
    if (texIndex < 0) texIndex = 0;

    int texX = texIndex % 4;
    int texY = texIndex / 4;

    return glm::vec2(texX * texSize, texY * texSize);
}

glm::vec3 Chunk::getWorldOffset() const {
    // Convert chunk position to world coordinates
    return glm::vec3(
        static_cast<float>(m_position.x * SIZE),
        static_cast<float>(m_position.y * SIZE),
        static_cast<float>(m_position.z * SIZE)
    );
}

AABB Chunk::getBoundingBox() const {
    glm::vec3 worldOffset = getWorldOffset();
    return AABB(
        worldOffset,                    // min corner
        worldOffset + glm::vec3(SIZE)   // max corner
    );
}

bool Chunk::shouldRenderFace(int x, int y, int z, int faceDir) const {
    // Calculate neighbor position based on face direction
    int nx = x, ny = y, nz = z;
    switch (faceDir) {
        case 0: nx++; break; // +X
        case 1: nx--; break; // -X
        case 2: ny++; break; // +Y
        case 3: ny--; break; // -Y
        case 4: nz++; break; // +Z
        case 5: nz--; break; // -Z
    }

    // Check if neighbor is within this chunk
    if (nx >= 0 && nx < SIZE && ny >= 0 && ny < SIZE && nz >= 0 && nz < SIZE) {
        // Neighbor is within this chunk
        return !isVoxelSolid(nx, ny, nz);
    }

    // Neighbor is outside this chunk - check with ChunkManager
    if (!m_chunkManager) {
        return true; // No chunk manager, assume face should be rendered
    }

    // Convert local neighbor coordinates to world coordinates
    glm::vec3 chunkWorldOffset = getWorldOffset();
    int worldX = static_cast<int>(chunkWorldOffset.x) + nx;
    int worldY = static_cast<int>(chunkWorldOffset.y) + ny;
    int worldZ = static_cast<int>(chunkWorldOffset.z) + nz;

    // Check if neighbor voxel in adjacent chunk is solid
    return !m_chunkManager->isVoxelSolidAtWorldPosition(worldX, worldY, worldZ);
}

void Chunk::regenerateMesh() {
    if (!m_meshDirty) return;

    m_vertices.clear();
    m_indices.clear();

    // Generate mesh with proper per-voxel face culling
    generateVoxelMesh();

    // Create/update buffers
    createBuffers();

    m_meshDirty = false;
}

void Chunk::generateVoxelMesh() {
    // Generate mesh by checking each voxel and its faces individually
    for (int x = 0; x < SIZE; x++) {
        for (int y = 0; y < SIZE; y++) {
            for (int z = 0; z < SIZE; z++) {
                if (isVoxelSolid(x, y, z)) {
                    BlockType blockType = getVoxelType(x, y, z);

                    // Check each face of this voxel
                    for (int faceDir = 0; faceDir < 6; faceDir++) {
                        if (shouldRenderFace(x, y, z, faceDir)) {
                            addVoxelFace(x, y, z, faceDir, blockType);
                        }
                    }
                }
            }
        }
    }
}

void Chunk::addVoxelFace(int x, int y, int z, int faceDir, BlockType blockType) {
    glm::vec3 worldOffset = getWorldOffset();

    // Calculate voxel position in world coordinates
    glm::vec3 voxelWorldPos = worldOffset + glm::vec3(x, y, z);

    // Get texture coordinates for this block type
    glm::vec2 texOffset, texSize;
    if (m_textureManager) {
        TextureInfo texInfo = m_textureManager->getBlockTexture(blockType);
        texOffset = texInfo.uvMin;
        texSize = texInfo.uvMax - texInfo.uvMin;
    } else {
        // Fallback to old system
        texOffset = getTextureCoords(blockType);
        texSize = glm::vec2(0.25f, 0.25f);
    }

    uint32_t startIndex = static_cast<uint32_t>(m_vertices.size());

    // White color since we're using textures now
    glm::vec3 color = glm::vec3(1.0f, 1.0f, 1.0f);

    // Define face vertices with consistent CCW winding when viewed from outside
    // Face directions: 0=+X, 1=-X, 2=+Y, 3=-Y, 4=+Z, 5=-Z
    switch (faceDir) {
        case 0: // +X face (right) - CCW from outside: (1,0,0)→(1,0,1)→(1,1,1)→(1,1,0)
            m_vertices.push_back({{1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {texOffset.x + texSize.x, texOffset.y}, voxelWorldPos, color});
            m_vertices.push_back({{1.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {texOffset.x, texOffset.y}, voxelWorldPos, color});
            m_vertices.push_back({{1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {texOffset.x, texOffset.y + texSize.y}, voxelWorldPos, color});
            m_vertices.push_back({{1.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {texOffset.x + texSize.x, texOffset.y + texSize.y}, voxelWorldPos, color});
            break;
        case 1: // -X face (left) - CCW from outside: (0,0,1)→(0,0,0)→(0,1,0)→(0,1,1)
            m_vertices.push_back({{0.0f, 0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {texOffset.x, texOffset.y}, voxelWorldPos, color});
            m_vertices.push_back({{0.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, {texOffset.x + texSize.x, texOffset.y}, voxelWorldPos, color});
            m_vertices.push_back({{0.0f, 1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, {texOffset.x + texSize.x, texOffset.y + texSize.y}, voxelWorldPos, color});
            m_vertices.push_back({{0.0f, 1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {texOffset.x, texOffset.y + texSize.y}, voxelWorldPos, color});
            break;
        case 2: // +Y face (top) - CCW from outside: (0,1,0)→(1,1,0)→(1,1,1)→(0,1,1)
            m_vertices.push_back({{0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {texOffset.x, texOffset.y + texSize.y}, voxelWorldPos, color});
            m_vertices.push_back({{1.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {texOffset.x + texSize.x, texOffset.y + texSize.y}, voxelWorldPos, color});
            m_vertices.push_back({{1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {texOffset.x + texSize.x, texOffset.y}, voxelWorldPos, color});
            m_vertices.push_back({{0.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {texOffset.x, texOffset.y}, voxelWorldPos, color});
            break;
        case 3: // -Y face (bottom) - CCW from outside: (0,0,1)→(1,0,1)→(1,0,0)→(0,0,0)
            m_vertices.push_back({{0.0f, 0.0f, 1.0f}, {0.0f, -1.0f, 0.0f}, {texOffset.x, texOffset.y}, voxelWorldPos, color});
            m_vertices.push_back({{1.0f, 0.0f, 1.0f}, {0.0f, -1.0f, 0.0f}, {texOffset.x + texSize.x, texOffset.y}, voxelWorldPos, color});
            m_vertices.push_back({{1.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, {texOffset.x + texSize.x, texOffset.y + texSize.y}, voxelWorldPos, color});
            m_vertices.push_back({{0.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, {texOffset.x, texOffset.y + texSize.y}, voxelWorldPos, color});
            break;
        case 4: // +Z face (front) - CCW from outside: (0,0,1)→(1,0,1)→(1,1,1)→(0,1,1)
            m_vertices.push_back({{0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {texOffset.x, texOffset.y}, voxelWorldPos, color});
            m_vertices.push_back({{1.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {texOffset.x + texSize.x, texOffset.y}, voxelWorldPos, color});
            m_vertices.push_back({{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {texOffset.x + texSize.x, texOffset.y + texSize.y}, voxelWorldPos, color});
            m_vertices.push_back({{0.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {texOffset.x, texOffset.y + texSize.y}, voxelWorldPos, color});
            break;
        case 5: // -Z face (back) - CCW from outside: (1,0,0)→(0,0,0)→(0,1,0)→(1,1,0)
            m_vertices.push_back({{1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {texOffset.x, texOffset.y}, voxelWorldPos, color});
            m_vertices.push_back({{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {texOffset.x + texSize.x, texOffset.y}, voxelWorldPos, color});
            m_vertices.push_back({{0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {texOffset.x + texSize.x, texOffset.y + texSize.y}, voxelWorldPos, color});
            m_vertices.push_back({{1.0f, 1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {texOffset.x, texOffset.y + texSize.y}, voxelWorldPos, color});
            break;
    }

    // Add indices for the quad (2 triangles)
    m_indices.push_back(startIndex + 0);
    m_indices.push_back(startIndex + 1);
    m_indices.push_back(startIndex + 2);
    m_indices.push_back(startIndex + 2);
    m_indices.push_back(startIndex + 3);
    m_indices.push_back(startIndex + 0);
}

void Chunk::generateGreedyMesh() {
    // Greedy mesh generation is disabled for now
    // Using regular voxel mesh generation instead
    generateVoxelMesh();
    return;

    // TODO: Implement proper greedy mesh with block types
    /*
    glm::vec3 chunkWorldOffset = getWorldOffset();

    // Top face (+Y) - check if top layer has solid blocks
    bool hasTopSolid = false;
    for (int x = 0; x < SIZE; x++) {
        for (int z = 0; z < SIZE; z++) {
            if (m_voxels[x][SIZE-1][z] != BlockType::AIR) {
                hasTopSolid = true;
                break;
            }
        }
        if (hasTopSolid) break;
    }

    // Commented out old greedy mesh implementation
    */
}

void Chunk::addGreedySideFace(int faceDir) {
    // Disabled for now - using regular voxel mesh generation
    (void)faceDir; // Suppress unused parameter warning
    return;

    // TODO: Implement proper greedy side face generation
    /*
    // Check if this side face should be rendered by sampling a center voxel
    int centerX = SIZE / 2;
    int centerY = SIZE / 2;
    int centerZ = SIZE / 2;

    if (!shouldRenderFace(centerX, centerY, centerZ, faceDir)) {
        return; // Don't render this side face - it's adjacent to a solid chunk
    }

    // Same side face generation as before, but using chunk's world offset
    glm::vec3 chunkWorldOffset = getWorldOffset();
    float minXZ = 0.0f;
    float maxXZ = SIZE;
    float bottomY = 0.0f;
    float midY = SIZE/2.0f;
    float topY = SIZE;

    // Blue bottom half
    uint32_t startIndex = static_cast<uint32_t>(m_vertices.size());

    switch (faceDir) {
        case 0: { // +X face
            float x = maxXZ;
            m_vertices.push_back({{x, bottomY, minXZ}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}, chunkWorldOffset, glm::vec3(0.0f, 0.0f, 1.0f)});
            m_vertices.push_back({{x, bottomY, maxXZ}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, chunkWorldOffset, glm::vec3(0.0f, 0.0f, 1.0f)});
            m_vertices.push_back({{x, midY,    maxXZ}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}, chunkWorldOffset, glm::vec3(0.0f, 0.0f, 1.0f)});
            m_vertices.push_back({{x, midY,    minXZ}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}, chunkWorldOffset, glm::vec3(0.0f, 0.0f, 1.0f)});
            break;
        }
        case 1: { // -X face
            float x = minXZ;
            m_vertices.push_back({{x, bottomY, minXZ}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, chunkWorldOffset, glm::vec3(0.0f, 0.0f, 1.0f)});
            m_vertices.push_back({{x, bottomY, maxXZ}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}, chunkWorldOffset, glm::vec3(0.0f, 0.0f, 1.0f)});
            m_vertices.push_back({{x, midY,    maxXZ}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}, chunkWorldOffset, glm::vec3(0.0f, 0.0f, 1.0f)});
            m_vertices.push_back({{x, midY,    minXZ}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}, chunkWorldOffset, glm::vec3(0.0f, 0.0f, 1.0f)});
            break;
        }
        case 4: { // +Z face
            float z = maxXZ;
            m_vertices.push_back({{minXZ, bottomY, z}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, chunkWorldOffset, glm::vec3(0.0f, 0.0f, 1.0f)});
            m_vertices.push_back({{maxXZ, bottomY, z}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, chunkWorldOffset, glm::vec3(0.0f, 0.0f, 1.0f)});
            m_vertices.push_back({{maxXZ, midY,    z}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, chunkWorldOffset, glm::vec3(0.0f, 0.0f, 1.0f)});
            m_vertices.push_back({{minXZ, midY,    z}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, chunkWorldOffset, glm::vec3(0.0f, 0.0f, 1.0f)});
            break;
        }
        case 5: { // -Z face
            float z = minXZ;
            m_vertices.push_back({{minXZ, bottomY, z}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}, chunkWorldOffset, glm::vec3(0.0f, 0.0f, 1.0f)});
            m_vertices.push_back({{maxXZ, bottomY, z}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}, chunkWorldOffset, glm::vec3(0.0f, 0.0f, 1.0f)});
            m_vertices.push_back({{maxXZ, midY,    z}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}, chunkWorldOffset, glm::vec3(0.0f, 0.0f, 1.0f)});
            m_vertices.push_back({{minXZ, midY,    z}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}, chunkWorldOffset, glm::vec3(0.0f, 0.0f, 1.0f)});
            break;
        }
    }

    // Add indices for blue bottom quad
    m_indices.push_back(startIndex + 0);
    m_indices.push_back(startIndex + 1);
    m_indices.push_back(startIndex + 2);
    m_indices.push_back(startIndex + 0);
    m_indices.push_back(startIndex + 2);
    m_indices.push_back(startIndex + 3);

    // Red top half
    startIndex = static_cast<uint32_t>(m_vertices.size());

    switch (faceDir) {
        case 0: { // +X face
            float x = maxXZ;
            m_vertices.push_back({{x, midY, minXZ}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}, chunkWorldOffset, glm::vec3(1.0f, 0.0f, 0.0f)});
            m_vertices.push_back({{x, midY, maxXZ}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, chunkWorldOffset, glm::vec3(1.0f, 0.0f, 0.0f)});
            m_vertices.push_back({{x, topY, maxXZ}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}, chunkWorldOffset, glm::vec3(1.0f, 0.0f, 0.0f)});
            m_vertices.push_back({{x, topY, minXZ}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}, chunkWorldOffset, glm::vec3(1.0f, 0.0f, 0.0f)});
            break;
        }
        case 1: { // -X face
            float x = minXZ;
            m_vertices.push_back({{x, midY, minXZ}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, chunkWorldOffset, glm::vec3(1.0f, 0.0f, 0.0f)});
            m_vertices.push_back({{x, midY, maxXZ}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}, chunkWorldOffset, glm::vec3(1.0f, 0.0f, 0.0f)});
            m_vertices.push_back({{x, topY, maxXZ}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}, chunkWorldOffset, glm::vec3(1.0f, 0.0f, 0.0f)});
            m_vertices.push_back({{x, topY, minXZ}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}, chunkWorldOffset, glm::vec3(1.0f, 0.0f, 0.0f)});
            break;
        }
        case 4: { // +Z face
            float z = maxXZ;
            m_vertices.push_back({{minXZ, midY, z}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, chunkWorldOffset, glm::vec3(1.0f, 0.0f, 0.0f)});
            m_vertices.push_back({{maxXZ, midY, z}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, chunkWorldOffset, glm::vec3(1.0f, 0.0f, 0.0f)});
            m_vertices.push_back({{maxXZ, topY, z}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, chunkWorldOffset, glm::vec3(1.0f, 0.0f, 0.0f)});
            m_vertices.push_back({{minXZ, topY, z}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, chunkWorldOffset, glm::vec3(1.0f, 0.0f, 0.0f)});
            break;
        }
        case 5: { // -Z face
            float z = minXZ;
            m_vertices.push_back({{minXZ, midY, z}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}, chunkWorldOffset, glm::vec3(1.0f, 0.0f, 0.0f)});
            m_vertices.push_back({{maxXZ, midY, z}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}, chunkWorldOffset, glm::vec3(1.0f, 0.0f, 0.0f)});
            m_vertices.push_back({{maxXZ, topY, z}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}, chunkWorldOffset, glm::vec3(1.0f, 0.0f, 0.0f)});
            m_vertices.push_back({{minXZ, topY, z}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}, chunkWorldOffset, glm::vec3(1.0f, 0.0f, 0.0f)});
            break;
        }
    }

    // Add indices for red top quad
    m_indices.push_back(startIndex + 0);
    m_indices.push_back(startIndex + 1);
    m_indices.push_back(startIndex + 2);
    m_indices.push_back(startIndex + 0);
    m_indices.push_back(startIndex + 2);
    m_indices.push_back(startIndex + 3);
    */
}

void Chunk::createBuffers() {
    if (m_vertices.empty()) return;

    // Create vertex buffer
    VkDeviceSize vertexBufferSize = sizeof(m_vertices[0]) * m_vertices.size();

    VkBuffer vertexStagingBuffer;
    VkDeviceMemory vertexStagingBufferMemory;
    m_device.createBuffer(vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                         vertexStagingBuffer, vertexStagingBufferMemory);

    void* vertexData;
    vkMapMemory(m_device.getDevice(), vertexStagingBufferMemory, 0, vertexBufferSize, 0, &vertexData);
    memcpy(vertexData, m_vertices.data(), static_cast<size_t>(vertexBufferSize));
    vkUnmapMemory(m_device.getDevice(), vertexStagingBufferMemory);

    m_vertexBuffer = std::make_unique<VulkanBuffer>(
        m_device,
        sizeof(m_vertices[0]),
        static_cast<uint32_t>(m_vertices.size()),
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    m_device.copyBuffer(vertexStagingBuffer, m_vertexBuffer->getBuffer(), vertexBufferSize);

    vkDestroyBuffer(m_device.getDevice(), vertexStagingBuffer, nullptr);
    vkFreeMemory(m_device.getDevice(), vertexStagingBufferMemory, nullptr);

    // Create index buffer
    VkDeviceSize indexBufferSize = sizeof(m_indices[0]) * m_indices.size();

    VkBuffer indexStagingBuffer;
    VkDeviceMemory indexStagingBufferMemory;
    m_device.createBuffer(indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                         indexStagingBuffer, indexStagingBufferMemory);

    void* indexData;
    vkMapMemory(m_device.getDevice(), indexStagingBufferMemory, 0, indexBufferSize, 0, &indexData);
    memcpy(indexData, m_indices.data(), static_cast<size_t>(indexBufferSize));
    vkUnmapMemory(m_device.getDevice(), indexStagingBufferMemory);

    m_indexBuffer = std::make_unique<VulkanBuffer>(
        m_device,
        sizeof(m_indices[0]),
        static_cast<uint32_t>(m_indices.size()),
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    m_device.copyBuffer(indexStagingBuffer, m_indexBuffer->getBuffer(), indexBufferSize);

    vkDestroyBuffer(m_device.getDevice(), indexStagingBuffer, nullptr);
    vkFreeMemory(m_device.getDevice(), indexStagingBufferMemory, nullptr);
}

void Chunk::render(VkCommandBuffer commandBuffer) {
    if (isEmpty()) return;

    VkBuffer vertexBuffers[] = {m_vertexBuffer->getBuffer()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, m_indexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(m_indices.size()), 1, 0, 0, 0);
}

#ifdef _DEBUG
bool Chunk::verifyWindingOrder() const {
    // Verify that all triangles have correct CCW winding order
    for (size_t i = 0; i < m_indices.size(); i += 3) {
        if (i + 2 >= m_indices.size()) break;

        const ChunkVertex& v0 = m_vertices[m_indices[i]];
        const ChunkVertex& v1 = m_vertices[m_indices[i + 1]];
        const ChunkVertex& v2 = m_vertices[m_indices[i + 2]];

        // Use the normal from the first vertex as expected normal
        if (!isTriangleCCW(v0, v1, v2, v0.normal)) {
            return false;
        }
    }
    return true;
}

bool Chunk::isTriangleCCW(const ChunkVertex& v0, const ChunkVertex& v1, const ChunkVertex& v2, const glm::vec3& expectedNormal) const {
    // Calculate triangle normal using cross product
    glm::vec3 edge1 = v1.pos - v0.pos;
    glm::vec3 edge2 = v2.pos - v0.pos;
    glm::vec3 triangleNormal = glm::normalize(glm::cross(edge1, edge2));

    // Check if triangle normal matches expected face normal (within tolerance)
    float dotProduct = glm::dot(triangleNormal, expectedNormal);
    const float tolerance = 0.95f; // Allow for floating point precision

    return dotProduct > tolerance;
}
#endif