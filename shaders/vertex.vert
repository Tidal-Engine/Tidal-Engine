#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoord;
layout(location = 3) in vec3 worldOffset;
layout(location = 4) in vec3 color;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(push_constant) uniform PushConstants {
    vec3 lightPos;
    vec3 lightColor;
    vec3 viewPos;
    vec3 voxelPosition;
    vec3 voxelColor;
} pushConstants;

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragTexCoord;
layout(location = 3) out vec3 fragVoxelColor;

void main() {
    // Use worldOffset from vertex attributes for voxel position
    vec3 translatedPosition = position + worldOffset;
    vec4 worldPosition = ubo.model * vec4(translatedPosition, 1.0);
    fragPos = worldPosition.xyz;
    fragNormal = mat3(transpose(inverse(ubo.model))) * normal;
    fragTexCoord = texCoord;
    fragVoxelColor = color;  // Use color from vertex attributes

    gl_Position = ubo.proj * ubo.view * worldPosition;
}