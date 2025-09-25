# Tidal Engine - Voxel Game Engine

A modern C++23 Vulkan-based voxel game engine with client-server architecture. Features a complete game system with world generation, multiplayer support, and an integrated development environment for voxel-based games.

![Tidal Engine](https://img.shields.io/badge/C%2B%2B-23-blue.svg)
![Vulkan](https://img.shields.io/badge/Vulkan-1.3-red.svg)
![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20Windows%20%7C%20macOS-lightgrey.svg)

## Features

### Core Engine
- **Modern Vulkan Rendering**: Vulkan 1.3 with optimized voxel rendering
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
- **GPU**: Vulkan 1.3 compatible graphics card
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

📚 **[View Interactive Documentation](docs/index.html)** - Complete technical documentation with interactive navigation

## Quick Start

### Linux (System Packages)

1. **Install Dependencies**:
```bash
# Ubuntu/Debian
sudo apt update
sudo apt install build-essential cmake ninja-build
sudo apt install vulkan-sdk libglfw3-dev libglm-dev libstb-dev

# Fedora
sudo dnf install gcc-c++ cmake ninja-build
sudo dnf install vulkan-devel glfw-devel glm-devel stb_image-devel

# Arch Linux
sudo pacman -S gcc cmake ninja vulkan-devel glfw glm
```

2. **Build and Run**:
```bash
git clone <repository-url>
cd Tidal-Engine
mkdir build && cd build
cmake .. -GNinja
ninja
./TidalEngine
```

### Alternative: Using vcpkg

1. **Setup vcpkg**:
```bash
git clone https://github.com/Microsoft/vcpkg.git
./vcpkg/bootstrap-vcpkg.sh
```

2. **Build with vcpkg**:
```bash
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=<path-to-vcpkg>/scripts/buildsystems/vcpkg.cmake -GNinja
ninja
./TidalEngine
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

## Project Structure

```
Tidal-Engine/
├── CMakeLists.txt              # Build configuration
├── README.md                   # This file
├── src/                        # Source files
│   ├── MainNew.cpp            # Entry point and menu system
│   ├── GameClient.cpp         # Client-side game logic
│   ├── GameServer.cpp         # Server-side game logic
│   ├── VulkanRenderer.cpp     # Core Vulkan rendering
│   ├── VulkanDevice.cpp       # Vulkan device management
│   ├── Camera.cpp             # FPS camera system
│   ├── ChunkManager.cpp       # World chunk management
│   ├── TextureManager.cpp     # Texture atlas system
│   ├── SaveSystem.cpp         # World persistence
│   └── NetworkManager.cpp     # Client-server networking
├── include/                   # Header files
├── shaders/                   # GLSL shaders
│   ├── vertex.vert           # Vertex shader
│   └── fragment.frag         # Fragment shader
├── assets/                    # Game assets
│   └── texturepacks/         # Block textures
└── docs/                      # Documentation
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

## Troubleshooting

### Common Issues

**"Vulkan validation layer error"**
- Ensure Vulkan SDK is properly installed
- Check that your GPU drivers support Vulkan 1.3

**"GLFW failed to create window"**
- Update graphics drivers
- Check display server compatibility (X11/Wayland)

**"Failed to load world"**
- Verify write permissions in `~/.tidal-engine/saves/`
- Check disk space availability

**"Chunks not loading"**
- Verify server is running (check console output)
- Check network connectivity for multiplayer

### Performance Issues
- Enable GPU profiling with F2 key
- Monitor chunk count and render distance
- Consider reducing texture resolution for older GPUs

## License

This project is part of the Tidal Engine development. Please refer to the LICENSE file for terms and conditions.

---

**Ready to build voxel worlds? Clone, build, and start creating!**