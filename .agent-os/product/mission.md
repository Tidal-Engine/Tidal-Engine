# Product Mission

> Last Updated: 2025-09-08
> Version: 1.0.0

## Pitch

Tidal Engine is a high-performance voxel engine that empowers mod developers to create immersive game experiences through a comprehensive LuaJIT modding API, combining C++ core performance with the flexibility to build everything from simple block games to complex multiplayer worlds.

## Users

**Primary Users:** Mod developers seeking a powerful, performance-focused voxel engine platform

**User Personas:**
- **Independent Game Developers:** Small teams or solo developers who want to create voxel-based games without building engine technology from scratch
- **Modding Communities:** Experienced modders who need advanced scripting capabilities and performance for complex gameplay mechanics
- **Educational Institutions:** Computer science programs teaching game development, engine architecture, and mod development
- **Internal Development Teams:** Company developers creating first-party games and proof-of-concept experiences

## The Problem

**Problem 1: Performance Bottlenecks in Existing Voxel Engines**
Most voxel engines sacrifice performance for ease of use, leading to limited world sizes, poor frame rates, and memory issues. This forces developers to spend months optimizing instead of creating content.
*Solution:* C++ core engine with multithreaded architecture and efficient memory management delivers consistent performance at scale.

**Problem 2: Limited Modding Flexibility**
Current voxel platforms offer restrictive modding APIs that prevent developers from implementing complex game mechanics or accessing core engine functionality.
*Solution:* Comprehensive LuaJIT modding API provides deep engine access while maintaining performance through JIT compilation.

**Problem 3: Platform Fragmentation**
Developers must maintain separate codebases or compromise on features to support Windows, Mac, and Linux, increasing development time and maintenance burden.
*Solution:* Cross-platform architecture ensures consistent performance and feature parity across all major desktop operating systems.

**Problem 4: Steep Learning Curve for Advanced Features**
Existing engines either oversimplify (limiting possibilities) or require extensive C++ knowledge for basic functionality, creating barriers for creative developers.
*Solution:* LuaJIT scripting provides approachable syntax with performance comparable to native code, enabling rapid prototyping and iteration.

## Differentiators

**LuaJIT-Powered Performance Modding**
While competitors use interpreted scripting languages, Tidal Engine leverages LuaJIT's just-in-time compilation to deliver scripting performance within 2-3x of native C++ code, enabling complex gameplay logic without engine modifications.

**Scalable Cubic Chunk Architecture**
Our 32x32x32 cubic chunk system with intelligent loading/unloading supports virtually unlimited world sizes while maintaining consistent memory usage, outperforming traditional column-based approaches in both memory efficiency and rendering performance.

**Modpack-to-Game Pipeline**
Unique architecture allows developers to combine multiple mods into cohesive "modpacks" that function as complete games, creating a sustainable ecosystem where mod developers can collaborate and build upon each other's work.

## Key Features

### Core Engine Performance
- **Large-Scale World Generation:** Cubic chunk system (32x32x32) supports massive worlds with efficient memory management and seamless loading
- **Multithreaded Architecture:** Parallel processing for world generation, physics, and rendering ensures smooth performance on modern multicore systems
- **Cross-Platform Compatibility:** Native performance on Windows, Mac, and Linux with consistent feature parity

### Gameplay Systems
- **Dynamic Block System:** Real-time block placement and destruction with automatic mesh updates and collision detection
- **Advanced Physics Simulation:** Realistic physics for blocks, entities, and fluids with configurable parameters for different game styles
- **Comprehensive Inventory & Crafting:** Flexible item management system with customizable recipes and crafting mechanics

### Multiplayer & Networking
- **Built-in Multiplayer Support:** Server-client architecture with efficient synchronization and lag compensation
- **Scalable Network Protocol:** Optimized for voxel data transmission with compression and delta updates

### Modding & Extensibility
- **LuaJIT Modding API:** High-performance scripting interface providing access to all engine systems and game logic
- **Modpack System:** Combine multiple mods into cohesive game experiences with dependency management and version control
- **Real-Time Lighting Engine:** Dynamic lighting system supporting colored lights, shadows, and day/night cycles with mod-configurable parameters