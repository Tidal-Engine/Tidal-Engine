# Infrastructure Examples

This folder contains examples and documentation for the engine's infrastructure systems.

## Logging System (spdlog)

### Features
- **Async logging** - Runs in separate thread, minimal performance impact
- **Multiple sinks** - Console and rotating file output
- **Multiple loggers** - Create separate loggers for different subsystems
- **Thread-safe** - Can log from any thread
- **Pattern formatting** - Customizable output format

### Usage Examples

See `logging_example.cpp` for full examples.

## Crash Handling (cpptrace)

### Features
- **Automatic crash detection** - Catches segfaults, aborts, etc.
- **Detailed stack traces** - Shows function names, files, and line numbers with colors
- **Manual traces** - Print stack trace anywhere for debugging
- **Better cross-platform** - Works great on Windows/Linux/Mac
- **Modern C++** - Active development, more accurate than backward-cpp

### Usage Examples

See `crash_example.cpp` for full examples.

## Integration Notes

Both systems are wrapped in `Logger.hpp` and `CrashHandler.hpp` to:
- Decouple from specific library implementations
- Provide consistent API across the codebase
- Make it easy to swap libraries later if needed
