# Tidal Engine - Project Overview

## Table of Contents
- [Introduction](#introduction)
- [Project Goals](#project-goals)
- [Architecture Overview](#architecture-overview)
- [Core Components](#core-components)
- [Dependencies](#dependencies)
- [Build System](#build-system)
- [Project Structure](#project-structure)
- [Getting Started](#getting-started)
- [Key Features](#key-features)
- [Development Workflow](#development-workflow)

## Introduction

Tidal Engine is a modern C++23 game engine built from the ground up using the Vulkan API. The project demonstrates a clean, educational implementation of real-time 3D graphics with a focus on understanding Vulkan's explicit nature and modern graphics programming concepts.

The engine currently implements a minimal but complete 3D rendering pipeline featuring:
- A rotating textured cube with Phong lighting
- First-person camera controls (WASD + mouse)
- Proper Vulkan resource management and synchronization
- Cross-platform support (Windows, Linux, macOS)

## Project Goals

### Primary Objectives
- **Educational**: Provide a clear, well-documented example of Vulkan usage
- **Modern**: Utilize C++23 features and modern graphics programming practices
- **Clean Architecture**: Maintain separation of concerns with well-defined components
- **Extensibility**: Design for easy addition of new features and rendering techniques

### Secondary Objectives
- **Performance**: Demonstrate efficient Vulkan resource management
- **Cross-platform**: Support major desktop platforms
- **Documentation**: Comprehensive wiki explaining every aspect of the implementation

## Architecture Overview

Tidal Engine follows a layered architecture that abstracts Vulkan complexity while maintaining explicit control:

```
┌─────────────────────────────────────────┐
│           Application Layer             │
│  (Game logic, input, main loop)        │
└─────────────────────────────────────────┘
┌─────────────────────────────────────────┐
│         Rendering Layer                 │
│  (VulkanRenderer, Camera)               │
└─────────────────────────────────────────┘
┌─────────────────────────────────────────┐
│         Resource Layer                  │
│  (VulkanBuffer, Shaders, Textures)     │
└─────────────────────────────────────────┘
┌─────────────────────────────────────────┐
│          Device Layer                   │
│  (VulkanDevice, Queue management)       │
└─────────────────────────────────────────┘
┌─────────────────────────────────────────┐
│        Platform Layer                   │
│  (GLFW, Vulkan SDK)                     │
└─────────────────────────────────────────┘
```

## Core Components

### Application (`src/Application.cpp`)
- **Purpose**: Main application orchestrator
- **Responsibilities**:
  - Initialize all subsystems
  - Handle GLFW window and input events
  - Coordinate rendering and game loop
  - Manage application lifetime
- **Key Features**: WASD camera movement, mouse look, ESC to quit

### VulkanDevice (`src/VulkanDevice.cpp`)
- **Purpose**: Vulkan device and instance management
- **Responsibilities**:
  - Create Vulkan instance with validation layers
  - Select suitable physical device
  - Create logical device and queues
  - Provide helper functions for buffer/image operations
- **Key Features**: Debug validation, queue family management, memory allocation

### VulkanRenderer (`src/VulkanRenderer.cpp`)
- **Purpose**: High-level rendering coordination
- **Responsibilities**:
  - Manage swap chain and presentation
  - Handle frame synchronization
  - Coordinate render passes
  - Manage command buffer recording
- **Key Features**: Double buffering, automatic swap chain recreation

### VulkanBuffer (`src/VulkanBuffer.cpp`)
- **Purpose**: Vulkan buffer abstraction
- **Responsibilities**:
  - Simplify buffer creation and management
  - Handle memory mapping and synchronization
  - Provide type-safe buffer operations
- **Key Features**: RAII management, automatic alignment, descriptor helpers

### Camera (`src/Camera.cpp`)
- **Purpose**: 3D camera system
- **Responsibilities**:
  - Generate view matrices
  - Handle input for camera movement
  - Manage camera parameters (position, rotation, zoom)
- **Key Features**: FPS-style controls, smooth movement, mouse look

## Dependencies

### Core Dependencies
- **Vulkan SDK**: Graphics API and validation layers
- **GLFW3**: Cross-platform window management and input
- **GLM**: Mathematics library for graphics operations
- **STB**: Image loading utilities

### Build Dependencies
- **CMake 3.25+**: Build system generator
- **vcpkg**: C++ package manager
- **glslangValidator**: GLSL to SPIR-V shader compiler

### Platform Requirements
- **Windows**: Visual Studio 2022+ or MinGW
- **Linux**: GCC 13+ or Clang 16+
- **macOS**: Xcode 15+ with MoltenVK

## Build System

The project uses CMake with vcpkg for dependency management:

### Key CMake Features
```cmake
# C++23 standard enforcement
set(CMAKE_CXX_STANDARD 23)

# Automatic shader compilation
compile_shader(TidalEngine "shaders/vertex.vert")
compile_shader(TidalEngine "shaders/fragment.frag")

# Platform-specific Vulkan setup
if(WIN32)
    target_compile_definitions(TidalEngine PRIVATE VK_USE_PLATFORM_WIN32_KHR)
elseif(UNIX AND NOT APPLE)
    target_compile_definitions(TidalEngine PRIVATE VK_USE_PLATFORM_XLIB_KHR)
```

### Build Process
1. **Dependency Resolution**: vcpkg fetches and builds dependencies
2. **Shader Compilation**: GLSL shaders compiled to SPIR-V bytecode
3. **Asset Copying**: Runtime assets copied to build directory
4. **Platform Configuration**: Platform-specific Vulkan definitions

## Project Structure

```
Tidal-Engine/
├── src/                    # Source files
│   ├── main.cpp           # Entry point
│   ├── Application.cpp    # Main application class
│   ├── VulkanDevice.cpp   # Device management
│   ├── VulkanRenderer.cpp # Rendering coordination
│   ├── VulkanBuffer.cpp   # Buffer abstraction
│   └── Camera.cpp         # Camera system
├── include/               # Header files
│   ├── Application.h
│   ├── VulkanDevice.h
│   ├── VulkanRenderer.h
│   ├── VulkanBuffer.h
│   └── Camera.h
├── shaders/              # GLSL shader sources
│   ├── vertex.vert       # Vertex shader
│   └── fragment.frag     # Fragment shader
├── assets/               # Runtime assets
│   └── texture.cpp       # Procedural texture generation
├── wiki/                 # Documentation
│   └── vulkan/           # Vulkan-specific docs
├── CMakeLists.txt        # Build configuration
└── vcpkg.json           # Package dependencies
```

## Getting Started

### Prerequisites
1. **Install Vulkan SDK**: Download from LunarG
2. **Install vcpkg**: Follow Microsoft's installation guide
3. **Clone Repository**: `git clone <repository-url>`

### Building
```bash
# Configure with vcpkg
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=[vcpkg-root]/scripts/buildsystems/vcpkg.cmake

# Build project
cmake --build build

# Run executable
./build/TidalEngine
```

### Controls
- **WASD**: Move camera forward/back/left/right
- **QE**: Move camera up/down
- **Mouse**: Look around (FPS-style)
- **Scroll**: Zoom in/out
- **ESC**: Exit application

## Key Features

### Vulkan Implementation
- **Explicit Resource Management**: Manual control over all Vulkan objects
- **Validation Layers**: Debug builds include comprehensive validation
- **Proper Synchronization**: Semaphores and fences for frame coordination
- **Memory Management**: Efficient buffer and image memory allocation

### Rendering Features
- **3D Transformations**: Model-View-Projection matrix pipeline
- **Phong Lighting**: Ambient, diffuse, and specular lighting components
- **Texture Mapping**: Procedural texture generation and sampling
- **Depth Testing**: Proper Z-buffer for 3D rendering

### Modern C++ Practices
- **RAII**: Automatic resource management
- **Smart Pointers**: Memory safety with unique_ptr
- **C++23 Features**: Latest language standards
- **Exception Safety**: Proper error handling throughout

## Development Workflow

### Adding New Features
1. **Design**: Plan integration with existing architecture
2. **Implement**: Follow established patterns and naming conventions
3. **Test**: Verify with validation layers enabled
4. **Document**: Update relevant wiki documentation

### Debugging
- **Validation Layers**: Enable in debug builds for Vulkan debugging
- **Error Handling**: All Vulkan calls checked for errors
- **Logging**: Console output for device selection and initialization

### Performance Considerations
- **Buffer Management**: Minimize allocations and copies
- **Command Buffer Recording**: Efficient render pass usage
- **Synchronization**: Proper pipeline barriers and memory barriers

---

This project serves as both a learning tool and a foundation for more complex graphics applications. The clean separation of concerns and comprehensive documentation make it an ideal starting point for understanding modern graphics programming with Vulkan.