// Navigation and TOC functionality for Tidal Engine Documentation

document.addEventListener('DOMContentLoaded', function() {
    // Set active navigation item based on current page
    setActiveNavItem();

    // Generate table of contents for current page
    generateTOC();

    // Handle TOC clicks for smooth scrolling
    handleTOCClicks();

    // Highlight active section on scroll
    window.addEventListener('scroll', highlightActiveSection);

    // Initial highlighting
    highlightActiveSection();
});

function setActiveNavItem() {
    const currentPage = window.location.pathname.split('/').pop() || 'index.html';
    const navLinks = document.querySelectorAll('.sidebar-left a');

    navLinks.forEach(link => {
        link.classList.remove('active');
        const linkPage = link.getAttribute('href');
        if (linkPage === currentPage ||
            (currentPage === '' && linkPage === 'index.html')) {
            link.classList.add('active');
        }
    });
}

function generateTOC() {
    const tocList = document.querySelector('.toc ul');
    if (!tocList) return;

    // Clear existing TOC
    tocList.innerHTML = '';

    const headings = document.querySelectorAll('.content h1, .content h2, .content h3, .content h4');

    headings.forEach(heading => {
        // Create anchor ID if it doesn't exist
        if (!heading.id) {
            heading.id = heading.textContent.toLowerCase()
                .replace(/[^\w\s-]/g, '') // Remove special chars
                .replace(/\s+/g, '-') // Replace spaces with dashes
                .trim();
        }

        const li = document.createElement('li');
        const a = document.createElement('a');
        a.href = '#' + heading.id;
        a.textContent = heading.textContent;
        a.className = `level-${heading.tagName.charAt(1)}`;
        li.appendChild(a);
        tocList.appendChild(li);
    });
}

function handleTOCClicks() {
    const tocLinks = document.querySelectorAll('.toc a');
    tocLinks.forEach(link => {
        link.addEventListener('click', function(e) {
            e.preventDefault();
            const targetId = this.getAttribute('href').substring(1);
            const targetElement = document.getElementById(targetId);

            if (targetElement) {
                targetElement.scrollIntoView({
                    behavior: 'smooth',
                    block: 'start'
                });

                // Update TOC active state
                tocLinks.forEach(tl => tl.classList.remove('active'));
                this.classList.add('active');
            }
        });
    });
}

function highlightActiveSection() {
    const tocLinks = document.querySelectorAll('.toc a');
    if (tocLinks.length === 0) return;

    const headings = document.querySelectorAll('.content h1, .content h2, .content h3, .content h4');
    let activeHeading = null;

    const scrollPos = window.scrollY + 100;

    // Find the currently visible heading
    headings.forEach(heading => {
        const headingTop = heading.offsetTop;
        if (scrollPos >= headingTop - 50) {
            activeHeading = heading;
        }
    });

    // Update TOC active states
    tocLinks.forEach(link => {
        link.classList.remove('active');
        if (activeHeading && link.getAttribute('href') === '#' + activeHeading.id) {
            link.classList.add('active');
        }
    });
}

// Utility function to create page template
function createPageTemplate(title, activeNavItem, content, tocItems = []) {
    return `<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>${title} - Tidal Engine Documentation</title>
    <link rel="stylesheet" href="styles.css">
</head>
<body>
    <div class="container">
        <!-- Left Sidebar Navigation -->
        <nav class="sidebar-left">
            <h1>Tidal Engine</h1>

            <div class="nav-section">
                <h3>Getting Started</h3>
                <ul>
                    <li><a href="index.html"${activeNavItem === 'index' ? ' class="active"' : ''}>Overview</a></li>
                    <li><a href="architecture.html"${activeNavItem === 'architecture' ? ' class="active"' : ''}>Architecture</a></li>
                    <li><a href="quick-start.html"${activeNavItem === 'quick-start' ? ' class="active"' : ''}>Quick Start</a></li>
                </ul>
            </div>

            <div class="nav-section">
                <h3>Core Systems</h3>
                <ul>
                    <li><a href="rendering.html"${activeNavItem === 'rendering' ? ' class="active"' : ''}>Rendering System</a></li>
                    <li><a href="networking.html"${activeNavItem === 'networking' ? ' class="active"' : ''}>Networking</a></li>
                    <li><a href="world-system.html"${activeNavItem === 'world-system' ? ' class="active"' : ''}>World System</a></li>
                    <li><a href="input-system.html"${activeNavItem === 'input-system' ? ' class="active"' : ''}>Input System</a></li>
                </ul>
            </div>

            <div class="nav-section">
                <h3>Infrastructure</h3>
                <ul>
                    <li><a href="vulkan-infrastructure.html"${activeNavItem === 'vulkan-infrastructure' ? ' class="active"' : ''}>Vulkan Infrastructure</a></li>
                    <li><a href="threading.html"${activeNavItem === 'threading' ? ' class="active"' : ''}>Threading & Concurrency</a></li>
                    <li><a href="shaders.html"${activeNavItem === 'shaders' ? ' class="active"' : ''}>Shader System</a></li>
                </ul>
            </div>

            <div class="nav-section">
                <h3>Development</h3>
                <ul>
                    <li><a href="build-system.html"${activeNavItem === 'build-system' ? ' class="active"' : ''}>Build System</a></li>
                    <li><a href="performance.html"${activeNavItem === 'performance' ? ' class="active"' : ''}>Performance</a></li>
                    <li><a href="debugging.html"${activeNavItem === 'debugging' ? ' class="active"' : ''}>Debugging</a></li>
                </ul>
            </div>
        </nav>

        <!-- Main Content -->
        <main class="content">
            ${content}
        </main>

        <!-- Right Sidebar - Table of Contents -->
        <aside class="sidebar-right">
            <div class="toc">
                <h4>On This Page</h4>
                <ul>
                    ${tocItems.map(item => `<li><a href="#${item.id}" class="${item.level}">${item.text}</a></li>`).join('')}
                </ul>
            </div>
        </aside>
    </div>

    <script src="nav.js"></script>
</body>
</html>`;
}