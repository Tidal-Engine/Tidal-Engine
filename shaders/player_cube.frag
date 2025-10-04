#version 450

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec4 lightPos;
    vec4 viewPos;
} ubo;

layout(set = 1, binding = 1) uniform sampler2D faceTexture;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragPos;
layout(location = 3) in vec3 localPos;
layout(location = 4) in vec2 texCoord;

layout(location = 0) out vec4 outColor;

void main() {
    // Simple lighting to show 3D shape
    vec3 lightDir = normalize(ubo.lightPos.xyz - fragPos);
    vec3 norm = normalize(fragNormal);
    float diff = max(dot(norm, lightDir), 0.0);

    // Ambient + diffuse lighting
    vec3 ambient = 0.4 * fragColor;
    vec3 diffuse = diff * fragColor * 0.6;
    vec3 finalColor = ambient + diffuse;

    // Check if we're on the front face (Z+ in local space)
    // localPos contains the unrotated vertex position
    if (abs(localPos.z - 0.25) < 0.01) {
        // Sample face texture (flip Y coordinate because texture is upside down)
        vec4 faceColor = texture(faceTexture, vec2(texCoord.x, 1.0 - texCoord.y));

        // Apply face texture with transparency
        // Use the texture's RGB but respect its alpha for transparency
        finalColor = mix(finalColor, faceColor.rgb * (0.4 + 0.6 * diff), faceColor.a);
    }

    outColor = vec4(finalColor, 1.0);
}
