#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec3 lightPos;
    vec3 viewPos;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec3 fragPos;
layout(location = 3) out vec3 fragLightPos;
layout(location = 4) out vec3 fragViewPos;

void main() {
    vec4 worldPos = ubo.model * vec4(inPosition, 1.0);
    fragPos = worldPos.xyz;

    // Transform normal to world space
    fragNormal = mat3(transpose(inverse(ubo.model))) * inNormal;

    fragColor = inColor;
    fragLightPos = ubo.lightPos;
    fragViewPos = ubo.viewPos;

    gl_Position = ubo.proj * ubo.view * worldPos;
}
