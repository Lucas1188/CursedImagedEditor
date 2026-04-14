# CursedImageEditor 🧙‍♂️💀

> *"One does not simply deliver a payload... One compresses it and lives off the land through cursed HTML."*

Welcome to the **CursedImageEditor** - where image processing meets the subtle art of compression, and every pixel serves the greater goal of payload delivery through living off the land techniques.

## 🔥 The Cursed Payload Delivery System

This isn't just an image editor. This is a **LOTL-inspired compression framework** that transforms your binaries into artifacts that blend seamlessly with legitimate web technologies, requiring no external dependencies or suspicious downloads.

The "cursed" aspect emerges from the deliberate choice to implement everything from scratch - no external libraries, no framework dependencies, just pure C89 code wrestling with complex algorithms. This approach ensures the system remains lightweight, portable, and dependent only on universal standards that are already present on target systems.

### The Components of Delivery
- **cursed-linux**: The ELF executable for Linux environments (x86_64)
- **cursed.exe**: The PE binary for Windows systems
- **cursed-mac-x86**: The Mach-O executable for macOS Intel (x86_64)
- **cursed-mac-arm**: The Mach-O executable for macOS ARM64 (Apple Silicon)
- **packer**: The compression engine that binds everything together
- **cursed-delivery.html**: The final self-contained HTML artifact containing all components

### The Compression Foundation
Drawing inspiration from DEFLATE's intricate Huffman coding tables and LZSS's efficient string matching, our compression algorithms demonstrate how complex data structures can be elegantly reduced:

- **DEFLATE**: Combines LZ77 string matching with Huffman coding for optimal compression ratios
- **LZSS**: Sliding window compression with greedy matching (32KB window, 3-258 byte matches)
- **Zlib/Gzip**: RFC-compliant wrappers providing checksums and cross-platform compatibility
- **Adler-32/CRC-32**: Checksum algorithms for data integrity verification

All compression is implemented from scratch in pure C89, ensuring no external dependencies and complete control over the compression pipeline.

## 🎮 Quick Start

```bash
# Build the complete system
make all

# Experience the delivery
open cursed-delivery.html
```

The HTML interface features terminal aesthetics with scanlines and subtle animations - a nod to the system's roots in computational efficiency and its inspiration from classic computing interfaces.

## 🖼️ Image Processing Capabilities

Beyond compression, the system provides sophisticated image manipulation with a focus on precision and efficiency:

### Pixel Format & Memory Layout
- **64-bit RGBA pixels** (16 bits per channel for enhanced precision vs standard 8-bit)
- **Row-major memory layout** for cache-efficient access patterns
- **Channel abstraction** allowing zero-copy operations on individual RGBA components

### Convolution Kernels (11 predefined)
- **Identity**: No-op kernel for baseline operations
- **Edge Detection**: Sobel (horizontal/vertical) and Laplacian operators
- **Sharpening**: Unsharp masking and basic sharpen kernels
- **Smoothing**: Box blur, Gaussian blur (3×3 and 5×5)
- **Emboss**: Depth-enhancing filter for artistic effects

### Geometric Primitives
- **Lines**: Bresenham algorithm for pixel-perfect rasterization
- **Circles**: Midpoint circle algorithm with 8-way symmetry
- **Rectangles/Triangles**: Scanline fill algorithms with edge-based triangle filling
- **Separable Operations**: Gaussian blur implemented as two 1D passes for O(2r) complexity

### Mathematical Expression Engine
- **AST-based parser** for per-pixel mathematical operations
- **Channel masking** (e.g., `l0[r,g,b] + l1[a]`) for selective operations
- **Multi-layer compositing** with up to 4 layers for complex effects
- **Real-time evaluation** across entire images

## 📦 The Delivery Mechanisms

### HTML Artifact (`dist/cursed-delivery.html`) - *Default Delivery Method*
- **Self-contained HTML** with all platform binaries embedded via Base64 data URLs
- **Terminal interface** featuring command execution and real-time visualization
- **Client-side validation** using Pako.js to verify decompression integrity
- **Minimal external dependencies** - only requires a modern web browser

### PDF Manifest (`dist/Submission_Manifest_GroupX.pdf`) - *Recommended for Distribution*
- **Payload embedded within PDF streams** using standard PDF object structure
- **Multi-layer compression pipeline**: Base64 encoding → gzip compression → zlib wrapping
- **Cross-platform compatibility** - PDFs render identically across all systems
- **Stealth delivery** - appears as legitimate documentation while containing all executable payloads
- ***More cursed*** - PDFs are ubiquitous but rarely scrutinized for embedded executables

### The Packer's Operation
The packer serves as the orchestration engine for multi-platform payload delivery:

```bash
# Generate compressed data URL containing all 4 platform binaries
cd dist && ../packer dist.html cursed-linux cursed.exe cursed-mac-x86 cursed-mac-arm > url.txt && cd ..

# Create HTML delivery artifact (default method - tests payload)
./packer -delivery delivery.template.html dist/url.txt > dist/cursed-delivery.html

# Generate PDF manifest (recommended for distribution - more cursed!)
./packer -pdf dist/Submission_Manifest_GroupX.pdf dist/url.txt
```

## 🎭 The Cursed Aesthetic

The interface design reflects the underlying compression principles and computational constraints:

### Terminal User Interface (TUI)
- **Memory-conscious design** with constrained 100-line command history
- **17 command types** supporting load/save, filtering, drawing, and mathematical operations
- **Real-time feedback** with immediate command execution and error reporting
- **Layer management** with 4-layer compositing and selective operations

### Web Interface Features
- **Live layer visualization** through HTML5 Canvas rendering
- **Real-time monitoring** of image processing operations
- **Decompression validation** ensuring payload integrity
- **Cross-platform accessibility** through universal web standards

### From-Scratch Philosophy
- **Zero external dependencies** - all algorithms implemented in pure C89
- **RFC-compliant implementations** of DEFLATE, zlib, and gzip
- **Efficient algorithms** prioritizing computational constraints over convenience
- **Minimal memory footprint** designed for resource-constrained environments

## 🧙‍♂️ LOTL Principles

Living off the land means leveraging existing system capabilities rather than introducing suspicious artifacts:

### Universal Web Standards
- **HTML/JavaScript**: Present on virtually all modern computing platforms
- **PDF format**: Standard document format with built-in compression support
- **Browser engines**: Native decompression, rendering, and execution capabilities
- **Terminal interfaces**: Command-line access available on most operating systems

### Legitimate Compression Standards
- **RFC 1950/1951/1952**: Official specifications for zlib, DEFLATE, and gzip
- **RFC 2083**: PNG specification for image format support
- **ISO/IEC 15948:2003**: International standard for PNG format
- **ITU-R BT.601**: Standard luma coefficients for image processing

### Cross-Platform Compatibility
- **No platform-specific code** - identical behavior across Linux, Windows, and macOS
- **Standard C89 compliance** ensuring broad compiler support
- **Endianness handling** for portable binary formats
- **Filesystem abstraction** through standard POSIX/Windows APIs

## 💡 Usage Examples

For detailed usage examples and advanced compositing workflows, see [`Recipes.md`](Recipes.md) - this file contains practical command sequences and techniques that may be updated as new features are added.

## 🏗️ Build System & Architecture

### Monolithic Design
- **Single compilation unit** - all source files combined into one executable
- **Preprocessor dispatch** - `#ifdef BUILD_ENGINE` vs `#ifdef BUILD_PACKER`
- **Unified cross-platform builds** - Zig `zig cc` handles all platform targets

### Directory Structure
```
├── Dockerfile               # Multi-stage build with Zig compiler
├── Makefile                 # Cross-platform build orchestration
├── index.template.html      # Web interface template
├── delivery.template.html   # Delivery artifact template
├── test.txt                 # Test data
├── dist/                    # Build artifacts (generated)
│   ├── cursed-linux         # Linux x86_64 executable
│   ├── cursed.exe           # Windows x86_64 executable
│   ├── cursed-mac-x86       # macOS Intel x86_64 executable
│   ├── cursed-mac-arm       # macOS ARM64 executable
│   ├── packer               # Compression engine
│   ├── pako.min.js          # Decompression library (fetched)
│   ├── dist.html            # Distribution template (stitched)
│   ├── url.txt              # Generated payload URLs
│   ├── cursed-delivery.html # HTML delivery artifact
│   └── Submission_Manifest_GroupX.pdf # PDF manifest
├── include/                 # Header files
│   ├── cursed_viewer.h
│   ├── cursedtui.h
│   ├── parser.h
│   ├── tui_exec.h
│   ├── tui_help.h
│   ├── tui_math.h
│   └── tui_state.h
├── lib/                     # Core algorithm implementations
│   ├── cursedhelpers.c/.h   # Utility functions
│   ├── packer.c             # Packer implementation
│   ├── search.h             # Search utilities
│   ├── sort.h               # Sorting utilities
│   ├── adler32/             # Adler-32 checksum
│   ├── base64encoder/       # Base64 encoding
│   ├── bithelper/           # Bit-level I/O
│   ├── bitmap/              # BMP format support
│   ├── crc32/               # CRC-32 checksum
│   ├── cursedlib/           # Image processing core
│   │   ├── image/           # Core image types
│   │   │   ├── bitdepth/    # Bit depth conversion
│   │   │   ├── channel/     # Channel manipulation
│   │   │   ├── draw/        # Drawing primitives
│   │   │   ├── filters/     # Image filters
│   │   │   └── gamma/       # Gamma correction
│   │   └── math/            # Mathematical kernels
│   ├── deflate/             # DEFLATE compression
│   ├── gzip/                # Gzip wrapper
│   ├── huffman/             # Huffman coding
│   ├── lzss/                # LZ77 string matching
│   ├── png/                 # PNG format support
│   └── zlib/                # Zlib wrapper
├── src/                     # Application logic
│   ├── cursed.c             # Main entry point
│   ├── cursed_viewer.c      # Viewer implementation
│   ├── cursedtui.c          # Terminal UI
│   ├── parser.c             # Command parsing
│   ├── tui_exec.c           # Command execution
│   ├── tui_help.c           # Help system
│   ├── tui_math.c           # Mathematical expressions
│   └── tui_state.c          # UI state management
└── test/                    # Test files
    ├── b64test.c            # Base64 tests
    └── sorttest.c           # Sort tests
```

### Dependencies
- **None for core functionality** - everything implemented from scratch
- **Pako.js** - Only for client-side validation in HTML interface
- **Standard C library** - Only libc functions for portability

## 📚 Documentation

For detailed technical information:
- [`DOCS.md`](DOCS.md) - Complete technical documentation with algorithms and APIs
- [`Recipes.md`](Recipes.md) - Advanced compositing workflows and examples

## ⚔️ Building from Source

### Requirements
- **Zig 0.16.0+** (provides unified cross-compilation via `zig cc`)
- **Make** (GNU Make for orchestration)
- **curl** (for fetching Pako.js)

### Using Docker (Recommended for Consistent Builds)
##### Linux
```bash
# Build the Docker image with Zig toolchain
docker build -t cursed-builder .

# Run the build with proper permissions and caching
docker run --rm -u $(id -u):$(id -g) -e ZIG_GLOBAL_CACHE_DIR=/tmp/zig-cache \
  -v $(pwd):/project cursed-builder make all
```
##### Windows

```bash
# Build the Docker image with Zig toolchain
docker build -t cursed-builder .

# Use internal PWD to resolve windows path
# User id automatically mapped by WSL
docker run --rm `
  -e ZIG_GLOBAL_CACHE_DIR=/tmp/zig-cache `
  -v ${PWD}:/project `
  cursed-builder make all
```
### Local Build Commands
```bash
# Individual platform binaries (outputs to dist/)
make dist/cursed-linux    # Linux x86_64
make dist/cursed.exe      # Windows x86_64
make dist/cursed-mac-x86  # macOS Intel (x86_64)
make dist/cursed-mac-arm  # macOS ARM64 (Apple Silicon)
make dist/packer          # Compression and delivery engine

# Bundling and delivery
make bundle               # Create compressed bundle with Pako.js and data URLs
make deliver              # Generate HTML delivery artifact (dist/cursed-delivery.html)
make pdf                  # Generate PDF manifest (dist/Submission_Manifest_GroupX.pdf)

# Everything (default target - builds HTML delivery)
make all                  # Equivalent to: make deliver pdf

# Clean all build artifacts
make clean               # Remove entire dist/ directory
```

### Build System Details
The build system uses **Zig's unified C compiler** (`zig cc`) which simplifies cross-compilation by handling:
- **Target triple specification** (Linux glibc, Windows GNU, macOS Intel, macOS ARM)
- **Compiler toolchain selection** automatically
- **Platform-specific linking** and runtime dependencies

This replaces the previous approach of using separate compilers (GCC, MinGW, etc.) with a single, portable compiler infrastructure.

## 🎯 Design Philosophy

The CursedImageEditor embodies several key principles:

### Computational Minimalism
- **Memory efficiency** - Designed for systems with limited resources
- **Algorithmic purity** - From-scratch implementations for full control
- **Constraint-driven design** - Every limitation becomes a design opportunity

### LOTL Methodology
- **Universal standards** - HTML, PDF, and compression formats present everywhere
- **No suspicious artifacts** - Everything blends with legitimate system traffic
- **Cross-platform portability** - Identical behavior across all major operating systems

### Educational Value
- **Algorithm study** - Complete implementations of complex compression algorithms
- **Systems programming** - Demonstrates low-level C programming techniques
- **Software architecture** - Shows how to build complex systems from simple components

---

*"Living off the land means your tools are already there... waiting."*

*May your compressions be efficient and your payloads blend seamlessly with the environment.* 🧙‍♂️💀</content>
