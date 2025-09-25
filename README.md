# 🌊 Tidal Engine - Voxel Game Engine

A modern C++23 Vulkan-based voxel game engine with client-server architecture. Features a complete game system with world generation, multiplayer support, and an integrated development environment for voxel-based games.

![Tidal Engine](https://img.shields.io/badge/C%2B%2B-23-blue.svg)
![Vulkan](https://img.shields.io/badge/Vulkan-1.4-red.svg)
![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20Windows%20%7C%20macOS-lightgrey.svg)

## Features

### Core Engine
- **Modern Vulkan Rendering**: Vulkan 1.4 with optimized voxel rendering
- **Client-Server Architecture**: Supports both single-player and multiplayer
- **Voxel World System**: Infinite chunk-based world generation
- **Real-time Lighting**: Dynamic lighting system with texture atlasing
- **Advanced UI System**: ImGui-based interface with main menu and in-game UI

### Game Features
- **Main Menu System**: World selection, settings, and multiplayer options
- **World Management**: Create, load, and save multiple worlds
- **Block Placement/Breaking**: Interactive voxel manipulation
- **Loading Screen**: Smooth world loading with animated feedback
- **In-Game Pause Menu**: ESC menu with save/quit functionality
- **Debug Tools**: Performance monitoring and rendering debug options

### Technical Features
- **Chunk System**: Efficient LOD and frustum culling
- **Network Protocol**: Custom client-server communication
- **Save System**: World persistence with compression
- **Texture Management**: Atlas-based texture system
- **Memory Management**: Complete cleanup and state reset

## Requirements

### System Requirements
- **GPU**: Vulkan 1.4 compatible graphics card
- **CPU**: Multi-core processor (4+ cores recommended)
- **RAM**: 8GB+ recommended
- **Storage**: 1GB+ free space

### Build Dependencies
- **C++23 Compiler**: GCC 13+, Clang 16+, or MSVC 19.35+
- **CMake**: 3.25 or higher
- **Vulkan SDK**: Latest version from LunarG
- **Package Manager**: System package manager or vcpkg

### Runtime Dependencies
- GLFW3 (windowing)
- GLM (mathematics)
- Dear ImGui (UI - fetched automatically)
- STB (image loading)

## Documentation

📚 **Interactive Documentation** - Complete technical documentation with interactive navigation

The documentation uses a multi-page HTML structure that requires a local web server to view properly.

### Viewing Documentation

**Option 1: Python Built-in Server (Easiest)**
```bash
cd docs
python3 -m http.server 8000
# Then open http://localhost:8000 in your browser
```

**Option 2: Node.js http-server**
```bash
npm install -g http-server
cd docs
http-server -p 8000
# Open http://localhost:8000
```

**Option 3: VS Code Live Server**
- Install the "Live Server" extension in VS Code
- Right-click on `docs/index.html`
- Select "Open with Live Server"

The documentation covers architecture, rendering systems, networking, build system, and development guides.

### API Documentation

📖 **API Reference Documentation** - Complete C++ API reference from source code

The project includes comprehensive Doxygen documentation for all classes and functions across the entire codebase.

**Prerequisites:**
```bash
# Ubuntu/Debian
sudo apt install doxygen graphviz

# Fedora
sudo dnf install doxygen graphviz

# macOS
brew install doxygen graphviz
```

**Windows:**
1. Install [Doxygen](https://www.doxygen.nl/download.html)
2. Install [Graphviz](https://graphviz.org/download/) (required for dependency diagrams)

**Generate Documentation:**

**Linux/macOS:**
```bash
cd docs
doxygen Doxyfile.in
# Open docs/api-reference.html in your browser
```

**Windows (via CMake - recommended):**
```cmd
cd build
cmake --build . --target doc
# Open build\docs\html\index.html in your browser
```

The API documentation provides detailed reference for:
- **Core Systems**: Camera, ThreadPool, and engine utilities
- **Game Logic**: Client/Server architecture, chunk management, save system
- **Networking**: Client-server communication and protocol definitions
- **Graphics**: Vulkan rendering pipeline and texture management
- **System**: Cross-platform user data and settings management

All public APIs include parameter documentation, return values, usage examples, and cross-references.

## Quick Start

### Windows

1. **Install Prerequisites**:
   - **Visual Studio 2022** with C++ workload
   - **Vulkan SDK** from [LunarG](https://vulkan.lunarg.com/sdk/home)
   - **Git** for Windows
   - **Doxygen** from [doxygen.nl](https://www.doxygen.nl/download.html) (for documentation)
   - **Graphviz** from [graphviz.org](https://graphviz.org/download/) (for documentation graphs)

2. **Setup vcpkg**:
```cmd
git clone https://github.com/Microsoft/vcpkg.git <anywhere>
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg install
```

3. **Build and Run**:
```cmd
git clone <repository-url>
cd Tidal-Engine
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=<INSERT VCPKG PATH HERE>\vcpkg\scripts\buildsystems\vcpkg.cmake
cmake --build . --config Release
.\Release\TidalEngine.exe
```

4. **Generate Documentation** (optional):
```cmd
cmake --build . --target doc
# Open build\docs\html\index.html in your browser
```

### macOS

1. **Install Prerequisites**:
```bash
# Install Xcode command line tools
xcode-select --install

# Install Homebrew (if not already installed)
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install dependencies
brew install cmake ninja git molten-vk glfw glm
```

2. **Install Vulkan SDK**:
   - Download from [LunarG](https://vulkan.lunarg.com/sdk/home)
   - Follow macOS installation instructions

3. **Build and Run**:
```bash
git clone <repository-url>
cd Tidal-Engine
mkdir build && cd build
cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release
ninja
./TidalEngine
```

### Linux

1. **Install Dependencies**:
```bash
# Ubuntu/Debian
sudo apt update
sudo apt install build-essential cmake ninja-build git
sudo apt install vulkan-sdk libglfw3-dev libglm-dev libstb-dev

# Fedora
sudo dnf install gcc-c++ cmake ninja-build git
sudo dnf install vulkan-devel glfw-devel glm-devel stb_image-devel

# Arch Linux
sudo pacman -S gcc cmake ninja git vulkan-devel glfw glm
```

2. **Build and Run**:
```bash
git clone <repository-url>
cd Tidal-Engine
mkdir build && cd build
cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release
ninja
./TidalEngine
```

### Verify Installation

After building, verify everything works:
```bash
# Check Vulkan support
vulkaninfo

# Test the engine (should show main menu)
./TidalEngine

# Test dedicated server
./TidalEngine --server
```

## Usage

### Main Menu Navigation
- **Single Player**: Create or select a world to play
- **Multiplayer**: Connect to servers (command-line options available)
- **Settings**: Configure game options (placeholder)
- **Quit**: Exit the application

### In-Game Controls
- **Movement**:
  - `WASD` - Move horizontally (ignores camera pitch)
  - `Space` - Move up
  - `Shift` - Move down
  - `Ctrl` - Speed boost (5x movement)
- **Camera**:
  - `Mouse` - Look around (FPS-style)
  - `Mouse Wheel` - Adjust movement speed
- **Interaction**:
  - `Left Click` - Break blocks
  - `Right Click` - Place blocks
  - `1-9` - Select hotbar blocks
- **Interface**:
  - `ESC` - Pause menu (save/quit options)
  - `F1` - Toggle debug window
  - `F2` - Toggle performance HUD
  - `F3` - Toggle rendering info
  - `F4` - Toggle chunk boundaries

### Command Line Options
```bash
./TidalEngine                    # Start with main menu
./TidalEngine --server           # Run dedicated server
./TidalEngine --client <host>    # Connect to remote server
```

## Architecture

### Client-Server Design
- **GameClient**: Handles rendering, input, UI, and client-side logic
- **GameServer**: Manages world state, physics, and multiplayer coordination
- **NetworkManager**: Handles communication between client and server
- **Integrated Server**: Single-player uses embedded server for consistency

### Rendering Pipeline
1. **Chunk Rendering**: Frustum culling → Vertex/index generation → GPU upload
2. **Texture Atlas**: Block textures packed into single atlas for efficiency
3. **Lighting**: Per-vertex lighting with ambient, diffuse, and specular components
4. **UI Overlay**: ImGui rendering for menus and debug interfaces

### World System
- **Chunk-based**: 16x16x16 voxel chunks for efficient LOD and culling
- **Infinite Generation**: On-demand chunk loading around player
- **Save Format**: Compressed world data with metadata
- **Block System**: Extensible block type system with texture mapping

## Development

### Debug Features
- **Validation Layers**: Extensive Vulkan debugging in debug builds
- **Performance Monitoring**: Real-time FPS, frame time, and GPU metrics
- **Chunk Visualization**: Wireframe chunk boundaries and debug info
- **Memory Tracking**: Allocation monitoring and leak detection

### Future Roadmap
- **Mod System**: Dynamic loading of gameplay modifications
- **Advanced Lighting**: Shadow mapping and global illumination
- **Physics System**: Block physics and entity simulation
- **Audio System**: 3D positional audio
- **Networking**: Optimized multiplayer protocols

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes following the existing code style
4. Test thoroughly (especially Vulkan validation)
5. Submit a pull request

## License

This project is part of the Tidal Engine development. Please refer to the LICENSE file for terms and conditions.