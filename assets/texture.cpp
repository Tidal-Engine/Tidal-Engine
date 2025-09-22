#include <vector>
#include <cstdint>

// Simple procedural texture generation
// Creates a checkerboard pattern texture
std::vector<uint8_t> generateCheckerboardTexture(int width = 256, int height = 256) {
    std::vector<uint8_t> texture(width * height * 4); // RGBA

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int index = (y * width + x) * 4;

            // Create checkerboard pattern
            bool isWhite = ((x / 32) + (y / 32)) % 2 == 0;

            if (isWhite) {
                texture[index + 0] = 255; // R
                texture[index + 1] = 255; // G
                texture[index + 2] = 255; // B
                texture[index + 3] = 255; // A
            } else {
                texture[index + 0] = 100; // R
                texture[index + 1] = 100; // G
                texture[index + 2] = 100; // B
                texture[index + 3] = 255; // A
            }
        }
    }

    return texture;
}

// Alternative: gradient texture
std::vector<uint8_t> generateGradientTexture(int width = 256, int height = 256) {
    std::vector<uint8_t> texture(width * height * 4); // RGBA

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int index = (y * width + x) * 4;

            // Create gradient from blue to red
            float u = static_cast<float>(x) / static_cast<float>(width - 1);
            float v = static_cast<float>(y) / static_cast<float>(height - 1);

            texture[index + 0] = static_cast<uint8_t>(u * 255); // R
            texture[index + 1] = static_cast<uint8_t>((1.0f - u) * v * 255); // G
            texture[index + 2] = static_cast<uint8_t>((1.0f - v) * 255); // B
            texture[index + 3] = 255; // A
        }
    }

    return texture;
}