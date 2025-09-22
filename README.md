# Tidal Engine - Vulkan Rotating Cube Demo

A modern C++23 Vulkan-based demo showcasing a rotating textured cube with real-time camera controls. This project serves as the foundation for the Tidal Engine graphics engine.

## Features

- **Modern Vulkan Rendering**: Uses Vulkan 1.3 for optimal performance
- **Real-time Rendering**: Rotating textured cube at 60+ FPS
- **Camera Controls**: Free-look camera with keyboard and mouse input
- **Advanced Graphics**: Phong lighting with diffuse, ambient, and specular components
- **Multiple Frames in Flight**: Optimized rendering with 2 frames in flight
- **Cross-Platform**: Builds on Windows, Linux, and macOS (with MoltenVK)

## Requirements

### System Requirements
- Vulkan 1.3 compatible GPU and drivers
- C++23 compatible compiler (GCC 13+, Clang 16+, MSVC 19.35+)
- CMake 3.25+

### Dependencies (managed via vcpkg)
- Vulkan SDK
- GLFW3
- GLM
- STB (for image loading)

## Building

### Using vcpkg (Recommended)

1. Clone the repository:
```bash
git clone <repository-url>
cd Tidal-Engine
```

2. Install vcpkg if not already installed:
```bash
git clone https://github.com/Microsoft/vcpkg.git
./vcpkg/bootstrap-vcpkg.sh  # Linux/macOS
./vcpkg/bootstrap-vcpkg.bat # Windows
```

3. Configure and build:
```bash
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=<path-to-vcpkg>/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release
```

### Manual Dependencies

If you prefer to install dependencies manually:

1. Install Vulkan SDK from LunarG
2. Install GLFW3, GLM, and STB through your package manager
3. Build normally with CMake

## Controls

- **W/A/S/D**: Move camera forward/left/backward/right
- **Q/E**: Move camera down/up
- **Mouse**: Look around (cursor is captured)
- **Mouse Wheel**: Zoom in/out
- **ESC**: Exit application

## Project Structure

```
Tidal-Engine/
├── CMakeLists.txt          # Build configuration
├── vcpkg.json             # Dependency manifest
├── src/                   # Source files
│   ├── main.cpp           # Entry point
│   ├── Application.cpp    # Main application logic
│   ├── VulkanDevice.cpp   # Vulkan device management
│   ├── VulkanRenderer.cpp # Core rendering system
│   ├── VulkanBuffer.cpp   # Buffer management
│   └── Camera.cpp         # Camera system
├── include/               # Header files
├── shaders/               # GLSL shaders
│   ├── vertex.glsl        # Vertex shader
│   └── fragment.glsl      # Fragment shader
└── assets/                # Asset files
    └── texture.cpp        # Procedural texture generation
```

## Architecture

The engine follows modern Vulkan best practices:

- **RAII Resource Management**: Automatic cleanup of Vulkan objects
- **Command Buffer Management**: Efficient recording and submission
- **Memory Management**: Optimal buffer and image allocation
- **Synchronization**: Proper use of semaphores and fences
- **Error Handling**: Comprehensive validation layer support

## Rendering Pipeline

1. **Vertex Processing**: 3D cube vertices with normals and texture coordinates
2. **Uniform Buffers**: Model-View-Projection matrices updated per frame
3. **Texture Sampling**: Procedural checkerboard texture
4. **Lighting**: Phong shading with push constants for light parameters
5. **Depth Testing**: Z-buffer for proper depth sorting

## Performance

- Target: 60+ FPS at 800x600 resolution
- Memory Usage: <200MB including Vulkan overhead
- GPU Memory: <100MB VRAM usage
- Multiple frames in flight for optimal GPU utilization

## Validation

Debug builds include comprehensive Vulkan validation layers:
- Memory allocation tracking
- API usage validation
- Performance warnings
- Synchronization validation

## Future Enhancements

- Model loading (glTF/OBJ support)
- Advanced lighting models (PBR)
- Shadow mapping
- Post-processing effects
- Multi-threaded command buffer recording

## License

This project is part of the Tidal Engine development and follows the project's licensing terms.

## Development

This demo was built following the technical specification in `.agent-os/specs/rotating-cube-demo.md` and serves as a proof-of-concept for the Tidal Engine's Vulkan rendering capabilities.