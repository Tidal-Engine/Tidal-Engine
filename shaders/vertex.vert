#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoord;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragTexCoord;

void main() {
    vec4 worldPosition = ubo.model * vec4(position, 1.0);
    fragPos = worldPosition.xyz;
    fragNormal = mat3(transpose(inverse(ubo.model))) * normal;
    fragTexCoord = texCoord;

    gl_Position = ubo.proj * ubo.view * worldPosition;
}