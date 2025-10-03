#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec4 lightPos;
    vec4 viewPos;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec2 inTexCoord;
layout(location = 4) in vec2 inAtlasOffset;
layout(location = 5) in vec2 inAtlasSize;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec3 fragPos;
layout(location = 3) out vec3 fragLightPos;
layout(location = 4) out vec3 fragViewPos;
layout(location = 5) out vec2 fragTexCoord;
layout(location = 6) out vec2 fragAtlasOffset;
layout(location = 7) out vec2 fragAtlasSize;

void main() {
    vec4 worldPos = ubo.model * vec4(inPosition, 1.0);
    fragPos = worldPos.xyz;

    // Transform normal to world space
    fragNormal = mat3(transpose(inverse(ubo.model))) * inNormal;

    fragColor = inColor;
    fragLightPos = ubo.lightPos.xyz;
    fragViewPos = ubo.viewPos.xyz;
    fragTexCoord = inTexCoord;
    fragAtlasOffset = inAtlasOffset;
    fragAtlasSize = inAtlasSize;

    gl_Position = ubo.proj * ubo.view * worldPos;
}
