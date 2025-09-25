#include "game/ClientChunk.h"
#include "game/ClientChunkManager.h"
#include <iostream>
#include <cstring>

ClientChunk::ClientChunk(const ChunkPos& position, VulkanDevice& device,
                        TextureManager* textureManager, ClientChunkManager* chunkManager)
    : m_position(position), m_device(device), m_textureManager(textureManager), m_chunkManager(chunkManager) {
    std::memset(m_voxels, 0, sizeof(m_voxels));
}

bool ClientChunk::isVoxelSolid(int x, int y, int z) const {
    // Check bounds
    if (x < 0 || x >= Chunk::SIZE || y < 0 || y >= Chunk::SIZE || z < 0 || z >= Chunk::SIZE) {
        return false;  // Out of bounds is considered air
    }
    return m_voxels[x][y][z] != BlockType::AIR;
}

BlockType ClientChunk::getVoxelType(int x, int y, int z) const {
    // Check bounds
    if (x < 0 || x >= Chunk::SIZE || y < 0 || y >= Chunk::SIZE || z < 0 || z >= Chunk::SIZE) {
        return BlockType::AIR;  // Out of bounds is considered air
    }
    return m_voxels[x][y][z];
}

void ClientChunk::render(VkCommandBuffer commandBuffer) {
    // REMOVED: Automatic mesh regeneration during render to prevent queue conflicts
    // Mesh regeneration now handled by the deferred update system

    if (!m_vertexBuffer || m_vertices.empty()) {
        return;
    }

    VkBuffer vertexBuffers[] = {m_vertexBuffer->getBuffer()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

    if (m_indexBuffer) {
        vkCmdBindIndexBuffer(commandBuffer, m_indexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(m_indices.size()), 1, 0, 0, 0);
    }
}

void ClientChunk::setChunkData(const ChunkData& data) {
    std::memcpy(m_voxels, data.voxels, sizeof(m_voxels));
    m_meshDirty = true;
}

void ClientChunk::updateBlock(int x, int y, int z, BlockType blockType) {
    if (x >= 0 && x < Chunk::SIZE && y >= 0 && y < Chunk::SIZE && z >= 0 && z < Chunk::SIZE) {
        m_voxels[x][y][z] = blockType;
        m_meshDirty = true;
        m_modified = true;  // Mark chunk as modified when any block changes
    }
}

void ClientChunk::regenerateMesh() {
    // Ensure device is idle before modifying buffers to prevent Vulkan queue conflicts
    vkDeviceWaitIdle(m_device.getDevice());

    size_t oldVertexCount = m_vertices.size();
    generateMesh();
    createBuffers();
    m_meshDirty = false;

    // Only log when vertex count changes significantly (for block placement)
    if (m_vertices.size() != oldVertexCount) {
        std::cout << "Regenerating mesh for chunk (" << m_position.x << ", " << m_position.y << ", " << m_position.z
                  << ") - vertices: " << oldVertexCount << " -> " << m_vertices.size() << std::endl;
    }
}

void ClientChunk::generateMesh() {
    m_vertices.clear();
    m_indices.clear();

    // Simple mesh generation (similar to Chunk class)
    for (int x = 0; x < Chunk::SIZE; x++) {
        for (int y = 0; y < Chunk::SIZE; y++) {
            for (int z = 0; z < Chunk::SIZE; z++) {
                BlockType blockType = m_voxels[x][y][z];
                if (blockType != BlockType::AIR) {
                    // Generate faces for visible sides
                    for (int face = 0; face < 6; face++) {
                        if (shouldRenderFace(x, y, z, face)) {
                            addVoxelFace(x, y, z, face, blockType);
                        }
                    }
                }
            }
        }
    }
}

void ClientChunk::addVoxelFace(int x, int y, int z, int faceDir, BlockType blockType) {
    // Calculate voxel position in world coordinates
    glm::vec3 voxelWorldPos = glm::vec3(m_position.x * Chunk::SIZE + x, m_position.y * Chunk::SIZE + y, m_position.z * Chunk::SIZE + z);

    // Get texture coordinates for this block type from texture atlas
    glm::vec2 texOffset, texSize;
    if (m_textureManager) {
        TextureInfo texInfo = m_textureManager->getBlockTexture(blockType, faceDir);
        texOffset = texInfo.uvMin;
        texSize = texInfo.uvMax - texInfo.uvMin;
    } else {
        // Fallback to full atlas if no texture manager
        texOffset = glm::vec2(0.0f, 0.0f);
        texSize = glm::vec2(1.0f, 1.0f);
    }

    // Get color based on block type
    glm::vec3 color;
    switch (blockType) {
        case BlockType::GRASS: color = glm::vec3(0.2f, 0.8f, 0.2f); break;
        case BlockType::DIRT: color = glm::vec3(0.6f, 0.4f, 0.2f); break;
        case BlockType::STONE: color = glm::vec3(0.5f, 0.5f, 0.5f); break;
        case BlockType::WOOD: color = glm::vec3(0.4f, 0.3f, 0.1f); break;
        case BlockType::SAND: color = glm::vec3(0.9f, 0.8f, 0.6f); break;
        default: color = glm::vec3(1.0f, 1.0f, 1.0f); break;
    }

    uint32_t startIndex = static_cast<uint32_t>(m_vertices.size());

    // Define face vertices based on direction
    // Face directions: 0=+X, 1=-X, 2=+Y, 3=-Y, 4=+Z, 5=-Z
    switch (faceDir) {
        case 0: // +X face (right) - Counter-clockwise when viewed from outside
            m_vertices.push_back({{ 1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {texOffset.x, texOffset.y}, voxelWorldPos, color});
            m_vertices.push_back({{ 1.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {texOffset.x, texOffset.y + texSize.y}, voxelWorldPos, color});
            m_vertices.push_back({{ 1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {texOffset.x + texSize.x, texOffset.y + texSize.y}, voxelWorldPos, color});
            m_vertices.push_back({{ 1.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {texOffset.x + texSize.x, texOffset.y}, voxelWorldPos, color});
            break;
        case 1: // -X face (left) - Counter-clockwise when viewed from outside
            m_vertices.push_back({{ 0.0f, 0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {texOffset.x, texOffset.y}, voxelWorldPos, color});
            m_vertices.push_back({{ 0.0f, 1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {texOffset.x, texOffset.y + texSize.y}, voxelWorldPos, color});
            m_vertices.push_back({{ 0.0f, 1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, {texOffset.x + texSize.x, texOffset.y + texSize.y}, voxelWorldPos, color});
            m_vertices.push_back({{ 0.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, {texOffset.x + texSize.x, texOffset.y}, voxelWorldPos, color});
            break;
        case 2: // +Y face (top) - Counter-clockwise when viewed from outside
            m_vertices.push_back({{ 0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {texOffset.x, texOffset.y}, voxelWorldPos, color});
            m_vertices.push_back({{ 0.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {texOffset.x, texOffset.y + texSize.y}, voxelWorldPos, color});
            m_vertices.push_back({{ 1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {texOffset.x + texSize.x, texOffset.y + texSize.y}, voxelWorldPos, color});
            m_vertices.push_back({{ 1.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {texOffset.x + texSize.x, texOffset.y}, voxelWorldPos, color});
            break;
        case 3: // -Y face (bottom) - Counter-clockwise when viewed from outside
            m_vertices.push_back({{ 0.0f, 0.0f, 1.0f}, {0.0f, -1.0f, 0.0f}, {texOffset.x, texOffset.y}, voxelWorldPos, color});
            m_vertices.push_back({{ 0.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, {texOffset.x, texOffset.y + texSize.y}, voxelWorldPos, color});
            m_vertices.push_back({{ 1.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, {texOffset.x + texSize.x, texOffset.y + texSize.y}, voxelWorldPos, color});
            m_vertices.push_back({{ 1.0f, 0.0f, 1.0f}, {0.0f, -1.0f, 0.0f}, {texOffset.x + texSize.x, texOffset.y}, voxelWorldPos, color});
            break;
        case 4: // +Z face (front) - Counter-clockwise when viewed from outside
            m_vertices.push_back({{ 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {texOffset.x, texOffset.y}, voxelWorldPos, color});
            m_vertices.push_back({{ 0.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {texOffset.x, texOffset.y + texSize.y}, voxelWorldPos, color});
            m_vertices.push_back({{ 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {texOffset.x + texSize.x, texOffset.y + texSize.y}, voxelWorldPos, color});
            m_vertices.push_back({{ 1.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {texOffset.x + texSize.x, texOffset.y}, voxelWorldPos, color});
            break;
        case 5: // -Z face (back) - Counter-clockwise when viewed from outside
            m_vertices.push_back({{ 1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {texOffset.x, texOffset.y}, voxelWorldPos, color});
            m_vertices.push_back({{ 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {texOffset.x + texSize.x, texOffset.y}, voxelWorldPos, color});
            m_vertices.push_back({{ 0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {texOffset.x + texSize.x, texOffset.y + texSize.y}, voxelWorldPos, color});
            m_vertices.push_back({{ 1.0f, 1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {texOffset.x, texOffset.y + texSize.y}, voxelWorldPos, color});
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

bool ClientChunk::shouldRenderFace(int x, int y, int z, int faceDir) const {
    // Calculate adjacent block position
    int adjX = x, adjY = y, adjZ = z;

    switch (faceDir) {
        case 0: adjX++; break; // +X
        case 1: adjX--; break; // -X
        case 2: adjY++; break; // +Y
        case 3: adjY--; break; // -Y
        case 4: adjZ++; break; // +Z
        case 5: adjZ--; break; // -Z
    }

    // Check if adjacent block is within this chunk
    if (adjX >= 0 && adjX < Chunk::SIZE &&
        adjY >= 0 && adjY < Chunk::SIZE &&
        adjZ >= 0 && adjZ < Chunk::SIZE) {
        // Adjacent block is within this chunk - check if it's solid
        return m_voxels[adjX][adjY][adjZ] == BlockType::AIR;
    }

    // Adjacent block is in a neighboring chunk - use world coordinates for cross-chunk culling
    if (m_chunkManager) {
        // Convert local position to world coordinates
        int worldX = m_position.x * Chunk::SIZE + adjX;
        int worldY = m_position.y * Chunk::SIZE + adjY;
        int worldZ = m_position.z * Chunk::SIZE + adjZ;

        // Check if adjacent block in neighboring chunk is solid
        // Use unsafe version since setChunkData already holds the mutex
        return !m_chunkManager->isVoxelSolidAtWorldPositionUnsafe(worldX, worldY, worldZ);
    }

    // Fallback: render the face if no chunk manager available
    return true;
}

void ClientChunk::createBuffers() {
    if (m_vertices.empty()) {
        return;
    }

    // Create vertex buffer
    m_vertexBuffer = std::make_unique<VulkanBuffer>(
        m_device,
        sizeof(m_vertices[0]),              // instanceSize
        static_cast<uint32_t>(m_vertices.size()), // instanceCount
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    m_vertexBuffer->map();
    m_vertexBuffer->writeToBuffer(m_vertices.data());

    // Create index buffer if we have indices
    if (!m_indices.empty()) {
        m_indexBuffer = std::make_unique<VulkanBuffer>(
            m_device,
            sizeof(m_indices[0]),              // instanceSize
            static_cast<uint32_t>(m_indices.size()), // instanceCount
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );

        m_indexBuffer->map();
        m_indexBuffer->writeToBuffer(m_indices.data());
    }
}