# RISC-V Partitioning Research Project - Documentation

This directory contains the documentation website for the RISC-V Dynamic Register File Partitioning (DRFP) research project.

## Project Overview

This website showcases research on reducing context-switch overhead in Real-Time Embedded Systems (RTES) using hardware-software co-design on RISC-V CVA6.

## Website Structure

```
docs/
├── index.html              # Main entry point (modular SPA)
├── components/             # Reusable HTML components
│   ├── head.html          # Base head with Tailwind config
│   ├── navbar.html        # Navigation bar
│   ├── hero.html          # Hero section
│   ├── section-summary.html   # Abstract/Problem
│   ├── section-architecture.html  # Architecture diagrams
│   ├── section-comparison.html    # Competitive landscape
│   ├── section-testing.html       # Benchmark results
│   ├── section-team.html          # Research team
│   ├── section-publications.html  # CTA/Resources
│   └── footer.html         # Footer
└── assets/                 # Static assets
    ├── logo.png           # Project logo
    ├── hero-section-cpu.png  # Hero image
    ├── latency-decomposition.svg
    ├── modified-riscv-datapath-with-register-partitioning-logic.svg
    └── team/              # Team member photos
```

## Running the Website

### Option 1: Direct Browser
Open `docs/index.html` directly in a web browser.

### Option 2: Local Server (Recommended)
```bash
cd docs
python -m http.server 8000
```
Then visit `http://localhost:8000`

### Option 3: VS Code Live Server
Use the Live Server extension to serve the `docs/` directory.

## Technology Stack

- **Tailwind CSS** - Utility-first CSS framework
- **Google Fonts** - Inter, Space Grotesk, Fira Code
- **Material Symbols** - Icon library
- **Vanilla JavaScript** - Component loading

## Key Features

- Responsive design with mobile support
- Modular component architecture
- Custom Tailwind theme with project colors
- Interactive hover effects and animations
- SVG-based charts and diagrams

## Browser Compatibility

- Chrome 90+
- Firefox 88+
- Safari 14+
- Edge 90+

## License

© 2024 RISC-V Research Project. All rights reserved.

## Links

- **GitHub Repository**: https://github.com/cepdnaclk/e20-4yp-Reducing-Context-Switching-Latency-in-RTES-Using-RFP
- **University of Peradeniya**: https://www.pdn.ac.lk
