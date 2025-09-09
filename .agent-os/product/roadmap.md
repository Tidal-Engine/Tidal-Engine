# Product Roadmap

> Last Updated: 2025-09-08
> Version: 1.0.0
> Status: Planning

## Phase 1: Core MVP (8-10 weeks)

**Goal:** Establish fundamental engine architecture and basic voxel world functionality
**Success Criteria:** Players can generate, navigate, and modify a basic voxel world with stable performance

### Must-Have Features

- **Basic World Generation** (L: 2 weeks)
  - Simple terrain generation with basic biomes
  - Foundation for cubic chunks system (32x32x32)
  - Basic world persistence

- **Block System** (M: 1 week)
  - Block placement and destruction
  - Basic block types (dirt, stone, grass, air)
  - Block state management

- **Core Rendering** (XL: 3+ weeks)
  - Basic voxel mesh generation
  - Camera system with first-person controls
  - Basic culling and optimization

- **Engine Architecture** (L: 2 weeks)
  - C++ core engine foundation
  - Basic memory management system
  - Cross-platform build system setup

- **Player Controller** (S: 2-3 days)
  - Basic movement (WASD + mouse look)
  - Collision detection with blocks
  - Basic player state management

### Dependencies
- None (foundational phase)

## Phase 2: Core Systems (6-8 weeks)

**Goal:** Implement key differentiating features and establish robust engine systems
**Success Criteria:** Stable lighting system, basic physics, and functional inventory system

### Must-Have Features

- **Real-time Lighting System** (XL: 3+ weeks)
  - Dynamic light propagation
  - Sunlight and block light sources
  - Smooth lighting transitions
  - Day/night cycle

- **Cubic Chunks Implementation** (L: 2 weeks)
  - Complete 32x32x32 chunk system
  - Efficient chunk loading/unloading
  - Memory optimization for large worlds

- **Physics Simulation** (L: 2 weeks)
  - Basic entity physics (gravity, collision)
  - Falling block mechanics
  - Water/fluid simulation foundation

- **Inventory System** (M: 1 week)
  - Player inventory management
  - Item stacking and organization
  - Basic UI for inventory access

- **Multithreaded Architecture** (L: 2 weeks)
  - Separate threads for rendering, world gen, and logic
  - Thread-safe data structures
  - Performance profiling tools

### Dependencies
- Phase 1 completion
- Stable block system from Phase 1

## Phase 3: Scale and Polish (8-10 weeks)

**Goal:** Achieve production-ready stability, multiplayer functionality, and comprehensive modding support
**Success Criteria:** Stable multiplayer experience with functional modding API and crafting system

### Must-Have Features

- **Multiplayer Capability** (XL: 3+ weeks)
  - Client-server architecture
  - Network synchronization
  - Player authentication and management
  - Lag compensation and prediction

- **LuaJIT Modding API** (XL: 3+ weeks)
  - Comprehensive scripting interface
  - Block/item registration system
  - Event system for mod interactions
  - Mod loading and management

- **Crafting System** (L: 2 weeks)
  - Recipe system and crafting mechanics
  - Crafting table interface
  - Resource processing chains

- **Advanced World Generation** (L: 2 weeks)
  - Multiple biome types
  - Structure generation (caves, ores)
  - Improved terrain algorithms
  - World generation configuration

- **Performance Optimization** (M: 1 week)
  - Memory usage optimization
  - Rendering performance improvements
  - Profiling and debugging tools

### Dependencies
- Phase 2 lighting system
- Phase 2 multithreaded architecture
- Stable inventory system from Phase 2

## Phase 4: Advanced Features (6-8 weeks)

**Goal:** Implement advanced features that distinguish Tidal Engine from competitors
**Success Criteria:** Advanced physics simulation, comprehensive mod ecosystem support, and professional-grade tools

### Must-Have Features

- **Advanced Physics** (XL: 3+ weeks)
  - Complex fluid dynamics
  - Particle systems
  - Advanced collision detection
  - Destructible terrain physics

- **Comprehensive Mod Tools** (L: 2 weeks)
  - Visual mod development tools
  - Asset pipeline for custom content
  - Mod distribution system
  - Debug and testing utilities

- **Advanced Rendering Features** (L: 2 weeks)
  - Shader system for visual effects
  - Advanced lighting effects
  - Post-processing pipeline
  - LOD (Level of Detail) system

- **World Editing Tools** (M: 1 week)
  - In-game world editor
  - Schematic import/export
  - Terrain sculpting tools

- **Cross-Platform Polish** (M: 1 week)
  - Platform-specific optimizations
  - Input device support
  - UI/UX refinements per platform

### Dependencies
- Phase 3 modding API
- Phase 3 multiplayer stability
- Complete core systems from previous phases

## Success Metrics

- **Performance:** Stable 60+ FPS with 16+ chunk render distance
- **Memory:** < 4GB RAM usage for large worlds
- **Network:** < 100ms latency tolerance for multiplayer
- **Modding:** 90%+ engine functionality accessible via LuaJIT API
- **Stability:** < 0.1% crash rate in production builds