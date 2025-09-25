# Tidal Engine Documentation

This directory contains the complete documentation for Tidal Engine, including both high-level guides and detailed API reference.

## Documentation Structure

### 📚 Manual Documentation (`/docs`)
- **High-level architecture guides** - System overviews and design decisions
- **Getting started tutorials** - Build instructions and quick start guides
- **Development guides** - Performance, debugging, and development workflows
- **Technical deep-dives** - Detailed explanations of core systems

### 🔍 API Reference (Generated)
- **Doxygen-generated API docs** - Complete class and function reference
- **Code examples** - Inline examples from source code comments
- **Class diagrams** - Visual inheritance and collaboration diagrams
- **Cross-references** - Automatic linking between related APIs

## Building the Documentation

### Prerequisites
Install Doxygen and Graphviz for generating API documentation:

```bash
# Ubuntu/Debian
sudo apt install doxygen graphviz

# macOS (with Homebrew)
brew install doxygen graphviz

# Windows (with Chocolatey)
choco install doxygen.install graphviz
```

### Generate API Documentation
```bash
# From project root directory
cmake -B build
cmake --build build --target doc
```

The generated API documentation will be available at `build/docs/html/index.html`.

## Viewing Documentation

### Local Development
1. **Manual docs**: Open `docs/index.html` in your browser
2. **API reference**: After building, open `build/docs/html/index.html`
3. **Integrated view**: Use `docs/api-reference.html` for unified access

### Web Hosting Options
The entire `/docs` directory can be hosted as a static website:

#### Option 1: Python Built-in Server
```bash
cd docs
python3 -m http.server 8000
# Open http://localhost:8000
```

#### Option 2: Node.js http-server
```bash
npm install -g http-server
cd docs
http-server -p 8000
```

#### Option 3: VS Code Live Server Extension
1. Install the "Live Server" extension
2. Right-click on `index.html`
3. Select "Open with Live Server"

## Documentation Style Guide

### Doxygen Comments
We use Doxygen-style comments in header files:

```cpp
/**
 * @brief Brief description of the class/function
 *
 * Detailed description with usage examples and important notes.
 *
 * @param paramName Description of parameter
 * @return Description of return value
 * @see RelatedClass for related functionality
 * @warning Important warnings about usage
 * @note Additional notes
 */
```

### Manual Documentation
- Use clear, concise language
- Include code examples where helpful
- Link to related API documentation
- Keep technical depth appropriate for the target audience

## Contributing to Documentation

1. **API docs**: Add/update Doxygen comments in header files
2. **Manual docs**: Edit HTML files in `/docs` directory
3. **Build system**: Update `CMakeLists.txt` if adding new documentation features
4. **Integration**: Update navigation in `docs/index.html` and related files

## File Organization

```
docs/
├── index.html                 # Main documentation entry point
├── api-reference.html         # API documentation integration page
├── assets/                    # CSS, JavaScript, images
├── getting-started/           # Tutorials and setup guides
├── technical/                 # Deep technical documentation
├── development/               # Development workflows and tools
├── Doxyfile.in               # Doxygen configuration template
└── README.md                  # This file
```

The documentation system is designed to be:
- **Portable** - Works offline and online
- **Maintainable** - Minimal manual sync required
- **Comprehensive** - Covers both high-level concepts and detailed APIs
- **Developer-friendly** - Easy to build and contribute to