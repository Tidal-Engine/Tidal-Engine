# 🌊 Tidal Engine

A modern C++ voxel engine built with Vulkan, featuring a modular architecture and comprehensive logging system.

## Features

- **Vulkan 1.3 Rendering**: Modern graphics API with explicit control
- **SDL3 Integration**: Cross-platform window management and input handling
- **Modular Architecture**: Separated subsystems for maintainability
  - VulkanBuffer: Buffer management (vertex, index, uniform)
  - VulkanSwapchain: Swapchain lifecycle and recreation
  - VulkanPipeline: Graphics pipeline and shader management
  - VulkanRenderer: Command buffers and rendering
- **Blinn-Phong Lighting**: Physically-based shading with ambient, diffuse, and specular components
- **Swapchain Recreation**: Automatic handling of window resize and minimization
- **Comprehensive Logging**: spdlog integration with 95 log statements across subsystems
- **Crash Handling**: cpptrace for detailed stack traces
- **ECS Ready**: EnTT integration for future entity management
- **ImGui Integration**: Ready for UI implementation

## Installation & Setup

### Prerequisites

#### All Platforms
- C++20 compatible compiler (Clang recommended, MSVC for Windows)
- CMake 3.20+
- Vulkan SDK

#### Fedora Linux (with Distrobox)

```bash
# Install required packages
sudo dnf install clang cmake ninja-build vulkan-tools vulkan-loader-devel \
                 vulkan-validation-layers-devel spirv-tools SDL3-devel \
                 glm-devel spdlog-devel

# If using distrobox, create a Fedora container
distrobox create --name fedora-dev --image fedora:latest
distrobox enter fedora-dev

# Then install the packages inside the container
sudo dnf install clang cmake ninja-build vulkan-tools vulkan-loader-devel \
                 vulkan-validation-layers-devel spirv-tools SDL3-devel \
                 glm-devel spdlog-devel
```

#### Windows

1. **Install Visual Studio 2022** (Community Edition or higher)
   - Select "Desktop development with C++"
   - Ensure C++ CMake tools are included

2. **Install Vulkan SDK**
   - Download from [LunarG Vulkan SDK](https://vulkan.lunarg.com/)
   - Run installer and ensure environment variables are set

3. **Install CMake** (if not included with Visual Studio)
   - Download from [cmake.org](https://cmake.org/download/)
   - Add CMake to system PATH during installation

### IDE Setup

#### Visual Studio Code

1. **Install Extensions**:
   - clangd (LLVM) - for C++ language support
   - CMake Tools (Microsoft)

2. **Configure CMake**:
   - Open the project folder in VSCode
   - Press `Ctrl+Shift+P` and select "CMake: Configure"
   - Select your preferred kit (Clang or GCC on Linux, MSVC on Windows)

3. **Build**:
   - Press `F7` or use "CMake: Build" from the command palette
   - Or use the build button in the status bar

4. **Run**:
   - Press `Ctrl+F5` or use "CMake: Run Without Debugging"
   - Or select the play button in the status bar

#### Visual Studio (Windows)

1. **Open the Project**:
   - File → Open → CMake
   - Select the `CMakeLists.txt` file

2. **Configure**:
   - Visual Studio will automatically detect CMake and configure the project
   - Wait for CMake cache generation to complete

3. **Build**:
   - Build → Build All (`Ctrl+Shift+B`)
   - Or right-click CMakeLists.txt and select "Build"

4. **Run**:
   - Select the startup item (TidalEngine.exe) from the dropdown
   - Press `F5` to run with debugging or `Ctrl+F5` to run without debugging

### Manual Build Instructions

#### Linux (Fedora)

```bash
# Configure
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build

# Run
./build/TidalEngine
```

#### Windows (Command Line)

```bash
# Configure (Visual Studio generator)
cmake -B build -G "Visual Studio 17 2022" -A x64

# Build
cmake --build build --config Release

# Run
.\build\Release\TidalEngine.exe
```

## Documentation

This project uses [Doxide](https://github.com/doxide/doxide) for API documentation generation.

### Generating Documentation

```bash
# Generate documentation
doxide build

# Documentation will be output to docs/
```

View the generated documentation by opening `docs/index.html` in your browser.

## Project Structure

```
Tidal-Engine/
├── include/
│   ├── core/           # Core utilities (logging, crash handling)
│   └── vulkan/         # Vulkan subsystems
├── src/
│   ├── core/           # Core implementations
│   └── vulkan/         # Vulkan implementations
├── shaders/            # GLSL shaders and compiled SPIR-V
├── infrastructure/     # Example programs (not compiled)
└── docs/              # Generated documentation
```

## Architecture

The engine follows a modular design where the `VulkanEngine` class orchestrates specialized subsystems:

- **VulkanBuffer**: Manages all buffer types with proper memory allocation
- **VulkanSwapchain**: Handles presentation and framebuffer management
- **VulkanPipeline**: Manages graphics pipeline state and shaders
- **VulkanRenderer**: Coordinates rendering commands and synchronization

This separation allows for easier maintenance and future expansion.

## License

[Add your license here]
