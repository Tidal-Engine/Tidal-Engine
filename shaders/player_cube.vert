#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec4 lightPos;
    vec4 viewPos;
} ubo;

layout(push_constant) uniform PushConstants {
    vec3 position;
    float yaw;
    vec3 color;
    float pitch;
} push;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec3 fragPos;
layout(location = 3) out vec3 localPos;
layout(location = 4) out vec2 texCoord;

void main() {
    // Create rotation matrices for yaw and pitch
    float yawRad = radians(push.yaw - 90.0);
    float pitchRad = radians(push.pitch);

    // Yaw rotation (around Y axis)
    mat3 yawRotation = mat3(
        cos(yawRad), 0.0, sin(yawRad),
        0.0, 1.0, 0.0,
        -sin(yawRad), 0.0, cos(yawRad)
    );

    // Pitch rotation (around X axis)
    mat3 pitchRotation = mat3(
        1.0, 0.0, 0.0,
        0.0, cos(pitchRad), -sin(pitchRad),
        0.0, sin(pitchRad), cos(pitchRad)
    );

    // Apply rotations and translation
    mat3 rotation = yawRotation * pitchRotation;
    vec3 rotatedPos = rotation * inPosition;
    vec3 worldPos = rotatedPos + push.position;

    gl_Position = ubo.proj * ubo.view * vec4(worldPos, 1.0);

    fragColor = push.color;
    fragNormal = rotation * inNormal;  // Rotate normals too for proper lighting
    fragPos = worldPos;
    localPos = inPosition;  // Pass local position to determine which face we're on
    texCoord = inTexCoord;  // Pass texture coordinates
}
