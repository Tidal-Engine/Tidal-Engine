# Tidal Engine Documentation

This documentation uses a multi-page HTML structure that requires a web server to view properly.

## Viewing the Documentation

### Option 1: Python Built-in Server (Easiest)

```bash
# Navigate to the docs directory
cd /path/to/Tidal-Engine/docs

# Python 3
python3 -m http.server 8000

# Python 2 (if python3 not available)
python -m SimpleHTTPServer 8000

# Then open in browser:
# http://localhost:8000
```

### Option 2: Node.js http-server

```bash
# Install http-server globally
npm install -g http-server

# Navigate to docs directory
cd /path/to/Tidal-Engine/docs

# Start server
http-server -p 8000

# Open http://localhost:8000
```

### Option 3: PHP Built-in Server

```bash
# Navigate to docs directory
cd /path/to/Tidal-Engine/docs

# Start PHP server
php -S localhost:8000

# Open http://localhost:8000
```

### Option 4: VS Code Live Server Extension

If you use VS Code:
1. Install the "Live Server" extension
2. Right-click on `index.html`
3. Select "Open with Live Server"

## File Structure

```
docs/
├── index.html              # Main overview page
├── architecture.html       # Architecture documentation
├── rendering.html          # Rendering system details
├── quick-start.html        # Getting started guide
├── styles.css             # Shared styles
├── nav.js                 # Navigation functionality
└── README.md              # This file
```

## Adding New Pages

To add a new documentation page:

1. Create a new HTML file (e.g., `networking.html`)
2. Copy the structure from an existing page
3. Update the navigation to mark the correct page as active
4. Add your content in the main section
5. The TOC will be generated automatically

## Development

The documentation uses plain HTML/CSS/JS with no build process required. Just edit the files and refresh your browser to see changes.