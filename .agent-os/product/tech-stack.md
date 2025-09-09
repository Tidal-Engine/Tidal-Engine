# Technical Stack

> Last Updated: 2025-09-08
> Version: 1.0.0

## Application Framework

- **Framework:** Custom C++ Engine
- **Version:** 1.0.0-alpha

## Core Language

- **Language:** C++23
- **Compiler:** GCC 11+ / Clang 14+ / MSVC 2022+
- **Build System:** CMake 4.1.1 / Ninja
- **Package Manager:** vcpkg or Conan

## Scripting Engine

- **Framework:** LuaJIT
- **Version:** 2.1+
- **Purpose:** High-performance mod scripting
- **Bindings:** Sol2 - Modern C++/LuaJIT binding library

## Database System

- **Primary Database:** n/a (game state stored in custom binary format)

## Memory Management

- **Strategy:** Custom memory pools and allocators
- **GC:** Manual memory management for performance

## Threading Architecture

- **Framework:** std::thread with custom job system
- **Pattern:** Multithreaded render/update/physics pipeline
- **Thread Management:** Intel TBB (Threading Building Blocks)

## UI Component Library

- **Library:** Dear ImGui - Debug/development UI
- **Style:** Immediate-mode GUI for development tools

## Fonts Provider

- **Provider:** Self-hosted bitmap fonts

## Platform Support

- **Platforms:** Windows, macOS, Linux
- **Window System:** GLFW - Window creation and input
- **Graphics API:** OpenGL 4.6 / Vulkan
- **OpenGL Loader:** GLAD
- **Math Library:** GLM - Graphics math
- **Image Loading:** stb_image
- **Audio:** OpenAL Soft - 3D audio

## Application Hosting

- **Hosting:** Steam, itch.io, direct distribution

## Database Hosting

- **Hosting:** n/a (local save files)

## Asset Hosting

- **Hosting:** Game installation directory
- **Streaming:** Custom asset streaming system

## Deployment Solution

- **CI/CD:** GitHub Actions
- **Packaging:** Platform-specific installers (NSIS/DMG/AppImage)

## Code Repository

- **Repository:** Git-based version control
- **URL:** https://github.com/Tidal-Engine/Tidal-Engine

## Utility Libraries

- **Logging:** spdlog - Fast C++ logging library
- **JSON:** nlohmann/json - JSON parsing and serialization
- **Compression:** zstd - High-performance compression for world saves

## Development Tools

- **Build System:** CMake & Ninja
- **Package Management:** vcpkg or Conan
- **CI/CD:** GitHub Actions
