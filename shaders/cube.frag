#version 450

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragPos;
layout(location = 3) in vec3 fragLightPos;
layout(location = 4) in vec3 fragViewPos;
layout(location = 5) in vec2 fragTexCoord;
layout(location = 6) in vec2 fragAtlasOffset;
layout(location = 7) in vec2 fragAtlasSize;

layout(location = 0) out vec4 outColor;

void main() {
    // Blinn-Phong lighting components
    vec3 lightColor = vec3(1.0, 1.0, 1.0);

    // Ambient
    float ambientStrength = 0.3;
    vec3 ambient = ambientStrength * lightColor;

    // Diffuse
    vec3 norm = normalize(fragNormal);
    vec3 lightDir = normalize(fragLightPos - fragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    // Specular (Blinn-Phong)
    float specularStrength = 0.5;
    vec3 viewDir = normalize(fragViewPos - fragPos);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(norm, halfwayDir), 0.0), 32.0);
    vec3 specular = specularStrength * spec * lightColor;

    // Remap tiled UVs to atlas region for greedy meshing
    // fragTexCoord ranges from (0,0) to (width, height) in blocks
    // We use fract() to tile, then remap to the block's atlas region
    vec2 tiledUV = fract(fragTexCoord);
    vec2 atlasUV = fragAtlasOffset + tiledUV * fragAtlasSize;

    // Sample texture and combine with lighting
    vec4 texColor = texture(texSampler, atlasUV);
    // Apply vertex color tinting (for biome-based grass coloring)
    vec3 tintedColor = texColor.rgb * fragColor;
    vec3 result = (ambient + diffuse + specular) * tintedColor;
    outColor = vec4(result, 1.0);
}
