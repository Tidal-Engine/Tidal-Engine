#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <array>

namespace engine {

/**
 * @brief Vertex data structure for 3D geometry
 *
 * Contains position, color, and normal data for each vertex.
 * Provides Vulkan binding and attribute descriptions for the graphics pipeline.
 */
struct Vertex {
    glm::vec3 position;     ///< Vertex position in 3D space
    glm::vec3 color;        ///< Vertex color (RGB)
    glm::vec3 normal;       ///< Vertex normal for lighting calculations
    glm::vec2 texCoord;     ///< Texture coordinates (UV, tiled for greedy meshing)
    glm::vec2 atlasOffset;  ///< Texture atlas offset (uvMin)
    glm::vec2 atlasSize;    ///< Texture atlas size for this block type

    /**
     * @brief Get the Vulkan vertex input binding description
     * @return Binding description for this vertex format
     */
    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescription;
    }

    /**
     * @brief Get the Vulkan vertex attribute descriptions
     * @return Array of attribute descriptions for position, color, normal, texCoord, atlasOffset, and atlasSize
     */
    static std::array<VkVertexInputAttributeDescription, 6> getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 6> attributeDescriptions{};

        // Position
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, position);

        // Color
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, color);

        // Normal
        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Vertex, normal);

        // Texture coordinates
        attributeDescriptions[3].binding = 0;
        attributeDescriptions[3].location = 3;
        attributeDescriptions[3].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[3].offset = offsetof(Vertex, texCoord);

        // Atlas offset
        attributeDescriptions[4].binding = 0;
        attributeDescriptions[4].location = 4;
        attributeDescriptions[4].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[4].offset = offsetof(Vertex, atlasOffset);

        // Atlas size
        attributeDescriptions[5].binding = 0;
        attributeDescriptions[5].location = 5;
        attributeDescriptions[5].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[5].offset = offsetof(Vertex, atlasSize);

        return attributeDescriptions;
    }
};

} // namespace engine
