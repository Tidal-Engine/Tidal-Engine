#version 450

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec3 fragVoxelColor;

layout(set = 0, binding = 1) uniform sampler2D texSampler;

layout(push_constant) uniform PushConstants {
    vec3 lightPos;
    vec3 lightColor;
    vec3 viewPos;
    vec3 voxelPosition;
    vec3 voxelColor;
} pushConstants;

layout(location = 0) out vec4 outColor;

void main() {
    // Ambient lighting
    float ambientStrength = 0.1;
    vec3 ambient = ambientStrength * pushConstants.lightColor;

    // Diffuse lighting
    vec3 norm = normalize(fragNormal);
    vec3 lightDir = normalize(pushConstants.lightPos - fragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * pushConstants.lightColor;

    // Specular lighting (Blinn-Phong)
    float specularStrength = 0.5;
    vec3 viewDir = normalize(pushConstants.viewPos - fragPos);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(norm, halfwayDir), 0.0), 64);
    vec3 specular = specularStrength * spec * pushConstants.lightColor;

    // Check if this is a wireframe by checking if texture coordinates are very close to (0,0)
    vec3 baseColor;
    if (fragTexCoord.x < 0.001 && fragTexCoord.y < 0.001) {
        // Wireframes have zero texture coordinates - use vertex color directly
        baseColor = fragVoxelColor;
    } else {
        // Regular blocks - sample texture and multiply by vertex color for lighting
        vec3 textureColor = texture(texSampler, fragTexCoord).rgb;
        baseColor = textureColor * fragVoxelColor;
    }

    vec3 result = (ambient + diffuse + specular) * baseColor;
    outColor = vec4(result, 1.0);
}