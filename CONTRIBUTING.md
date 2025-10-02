# Contributing to Tidal Engine

## Code Style

This project uses clang-format for automatic code formatting. Run before committing:

```bash
clang-format -i src/**/*.cpp include/**/*.hpp
```

## Building

```bash
cmake -B build
cmake --build build
```

## Testing

Run the engine:

```bash
./build/TidalEngine
```

Check logs:

```bash
cat logs/engine.log
```

## Documentation

Generate API documentation:

```bash
doxygen
```

View at `docs/html/index.html`

## Commit Guidelines

- Write clear, descriptive commit messages
- Keep commits focused on a single change
- Reference issue numbers when applicable
