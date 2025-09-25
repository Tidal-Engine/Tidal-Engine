# Tidal Engine Architecture

## Multi-Threaded Server Design

### Threading Model

Tidal Engine uses a modern multi-threaded server architecture designed to avoid the single-threaded bottlenecks that plague games like Minecraft.

```
┌─────────────────────────────────────────────────────────────┐
│                    GameServer                               │
├─────────────────┬─────────────────┬─────────────────────────┤
│   Main Thread   │  Network Thread │    Worker Pool          │
│   (Game Loop)   │                 │   (Async Tasks)         │
│                 │                 │                         │
│ • Game logic    │ • TCP/UDP       │ • Chunk generation      │
│ • Physics       │ • Message       │ • File I/O              │
│ • Entity ticks  │   parsing       │ • Compression           │
│ • World updates │ • Connection    │ • World saving          │
│ • 20 TPS tick   │   management    │ • Heavy computations    │
└─────────────────┴─────────────────┴─────────────────────────┘
                  │                 │
            ┌─────┴─────┐     ┌─────┴─────┐
            │Task Queue │     │Event Queue│
            │(Lock-free)│     │(Lock-free)│
            └───────────┘     └───────────┘
```

### Thread Responsibilities

**Main Thread (Game Loop - 20 TPS):**
- Game state updates
- Player movement validation
- Block placement/breaking logic
- World simulation
- Tick-based events

**Network Thread:**
- TCP/UDP socket management
- Packet parsing and validation
- Connection handling
- Message routing to game thread

**Worker Pool (Thread Pool):**
- Chunk generation (procedural terrain)
- File I/O operations (world saves/loads)
- Compression/decompression
- Heavy computational tasks
- Background maintenance

### Thread-Safe Communication

**Lock-Free Queues:**
- Inter-thread message passing
- High-performance, wait-free
- Separate queues for different message types

**Atomic Operations:**
- Simple state flags
- Counters and statistics
- Lock-free data when possible

**Read-Write Locks:**
- Player state (frequent reads, rare writes)
- World data (many readers, few writers)
- Configuration data

**Thread Pools:**
- Dynamic work distribution
- CPU-core aware scaling
- Task prioritization

### Performance Benefits

**Compared to Minecraft's Single-Threaded Approach:**
- ✅ Chunk generation doesn't block game loop
- ✅ File I/O doesn't cause lag spikes
- ✅ Network processing is parallel
- ✅ Better CPU utilization
- ✅ Scalable with hardware

**Scalability:**
- Worker threads scale with CPU cores
- Network thread handles multiple connections
- Game thread remains consistent 20 TPS
- I/O operations don't impact gameplay

## Client-Server Architecture

### Separation of Concerns

**GameServer (Headless):**
- World state authority
- Game logic validation
- Player management
- Save system
- Zero graphics dependencies

**GameClient:**
- Rendering (Vulkan)
- Input handling
- UI/ImGui
- Audio (future)
- Client-side prediction

**NetworkLayer:**
- Message protocol
- Connection management
- Packet serialization
- Error handling

### Deployment Models

**Singleplayer:**
```
┌─────────────────────────────────┐
│         TidalEngine             │
│  ┌─────────────┬─────────────┐  │
│  │ GameServer  │ GameClient  │  │
│  │ (embedded)  │             │  │
│  └─────────────┴─────────────┘  │
└─────────────────────────────────┘
```

**Multiplayer Client:**
```
┌─────────────────┐    Network    ┌─────────────────┐
│   GameClient    │◄──────────────┤ Remote Server   │
│                 │               │                 │
└─────────────────┘               └─────────────────┘
```

**Dedicated Server:**
```
┌─────────────────┐    Network    ┌─────────────────┐
│  GameServer     │◄──────────────┤   GameClient    │
│  (headless)     │               │                 │
│                 │◄──────────────┤   GameClient    │
│  No Graphics    │               │                 │
│  Console Only   │◄──────────────┤   GameClient    │
└─────────────────┘               └─────────────────┘
```

## Plugin/Mod System

### Extensibility Design

**Hook System:**
- Event-driven plugin architecture
- Before/after hooks for game events
- Permission-based access control
- Safe plugin isolation

**Plugin Types:**
- Block/item mods
- World generators
- Game modes
- Server administration tools
- Custom protocols

### Future Considerations

**Hot Reloading:**
- Plugin updates without server restart
- Safe state migration
- Version compatibility

**Scripting Support:**
- Lua/JavaScript integration
- Sandboxed execution
- Performance monitoring

**Marketplace:**
- Plugin distribution
- Dependency management
- Security validation

## Save System

### File Structure
```
saves/
├── world_name/
│   ├── level.dat          # World metadata
│   ├── player.dat         # Player state
│   └── chunks/            # Chunk files
│       ├── chunk_0_0_0.dat
│       └── ...
```

### Features
- Binary format for efficiency
- Version compatibility
- Automatic backups
- Incremental saves
- Compression support

## Technology Stack

**Core:**
- C++23
- CMake build system
- Cross-platform (Linux, Windows, macOS)

**Graphics (Client):**
- Vulkan API
- GLFW window management
- ImGui for debug UI

**Networking:**
- TCP for reliable data
- UDP for real-time updates (future)
- Custom binary protocol

**Storage:**
- Binary save format
- File-based world storage
- SQLite for complex data (future)

**Threading:**
- std::thread
- Lock-free queues
- Thread pools
- Atomic operations