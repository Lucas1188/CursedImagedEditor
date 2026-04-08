# CursedImageEditor — Codebase Documentation

## Table of Contents

1. [Project Overview](#1-project-overview)
2. [Build System](#2-build-system)
3. [Directory Layout](#3-directory-layout)
4. [Core Image Types (`lib/cursedlib/image/image.h`)](#4-core-image-types)
5. [Channel Access (`lib/cursedlib/image/channel/`)](#5-channel-access)
6. [Filters (`lib/cursedlib/image/filters/`)](#6-filters)
7. [Convolution Kernels (`lib/cursedlib/math/`)](#7-convolution-kernels)
8. [Drawing Primitives (`lib/cursedlib/image/draw/`)](#8-drawing-primitives)
9. [Bit Depth (`lib/cursedlib/image/bitdepth/`)](#9-bit-depth)
10. [Gamma (`lib/cursedlib/image/gamma/`)](#10-gamma)
11. [Bitmap I/O (`lib/bitmap/`)](#11-bitmap-io)
12. [PNG (`lib/png/`)](#12-png)
13. [Compression Stack (`lib/deflate/`, `lib/zlib/`, `lib/gzip/`)](#13-compression-stack)
14. [Helpers (`lib/cursedhelpers.h`)](#14-helpers)
15. [Utilities (`lib/search.h`, `lib/sort.h`)](#15-utilities)
16. [CLI Entry Point (`src/cursed.c`)](#16-cli-entry-point)
17. [TUI (`src/cursedtui.c`, `include/cursedtui.h`)](#17-tui)
18. [TUI State (`src/tui_state.c`, `include/tui_state.h`)](#18-tui-state)
19. [Command Parser (`src/parser.c`, `include/parser.h`)](#19-command-parser)
20. [Command Executor (`src/tui_exec.c`, `include/tui_exec.h`)](#20-command-executor)
21. [Math / Expression Evaluator (`src/tui_math.c`, `include/tui_math.h`)](#21-math--expression-evaluator)

---

## 1. Project Overview

CursedImageEditor is a C image processing tool with two interfaces:

- **CLI mode** — batch process a BMP file from the command line (greyscale, PNG encode, gzip compress).
- **TUI mode** — interactive terminal UI for layer-based editing, drawing, math-driven pixel operations, and saving.

The internal image format uses 64-bit packed pixels (16 bits per RGBA channel), giving higher precision than typical 8-bit pipelines. Files are read/written as BMP or PNG, with optional gzip compression.

---

## 2. Build System

**`Makefile`**

```
CC     = gcc
CFLAGS = -ansi -Werror
```

- All `.c` files under `src/` and `lib/` are discovered automatically with `find`.
- Object files mirror the source tree under `build/`.
- Output binary: `cursed`
- `make force` = clean + rebuild.

No external libraries are required. All compression, PNG, and encoding logic is implemented from scratch in `lib/`.

---

## 3. Directory Layout

```
CursedImageEditor/
├── src/                    Application layer (CLI + TUI)
│   ├── cursed.c            CLI entry point
│   ├── cursedtui.c         TUI render + input loop
│   ├── parser.c            Command string parser
│   ├── tui_exec.c          Command dispatcher
│   ├── tui_math.c          Math expression evaluator
│   └── tui_state.c         Global layer/canvas state
├── include/                Headers for the application layer
├── lib/
│   ├── cursedlib/
│   │   ├── image/          Core image type, channels, filters, drawing
│   │   └── math/           Convolution kernels
│   ├── bitmap/             BMP file I/O + 8↔16 bit conversion
│   ├── png/                PNG encoder/decoder
│   ├── deflate/            DEFLATE compressor
│   ├── zlib/               zlib wrapper
│   ├── gzip/               gzip wrapper
│   ├── huffman/            Huffman coding
│   ├── lzss/               LZSS compression
│   ├── adler32/            Adler-32 checksum
│   ├── crc32/              CRC-32 checksum
│   ├── base64encoder/      Base64 encoder
│   ├── bithelper/          Bit-level I/O helpers
│   ├── cursedhelpers.h     Logging macros
│   ├── search.h            Generic search
│   └── sort.h              Generic sort
└── test/                   Unit tests
```

---

## 4. Core Image Types

**`lib/cursedlib/image/image.h`** — included everywhere.

### Pixel format

```c
typedef uint64_t tcursed_pix;
```

Each pixel is a 64-bit integer with four 16-bit channels packed as:

| Bits  | Channel |
|-------|---------|
| 63–48 | Red     |
| 47–32 | Green   |
| 31–16 | Blue    |
| 15–0  | Alpha   |

16 bits per channel gives a value range of 0–65535, twice the precision of standard 8-bit RGBA.

### Image struct

```c
typedef struct {
    size_t      height;
    size_t      width;
    spixel_fmt  px_fmt;   /* format descriptor (masks, offsets, sample size) */
    tcursed_pix* pxs;     /* flat row-major pixel buffer */
} cursed_img;
```

Pixels are stored row-major, top-left first: `pxs[y * width + x]`.

### Pixel format descriptor

`spixel_fmt` holds bitmasks and bit-offsets for each channel so that generic code can extract channels without hardcoding the layout. The standard format is `CURSED_RGBA64_PXFMT`.

### Macros

| Macro | Purpose |
|-------|---------|
| `GEN_CURSED_IMG(w, h)` | Allocate a zero-filled image on the heap |
| `RELEASE_CURSED_IMG(img)` | Free the pixel buffer and NULL the pointer |
| `CURSED_RGBA64_PXFMT` | Compile-time initializer for the standard RGBA64 format |

---

## 5. Channel Access

**`lib/cursedlib/image/channel/channel.h/.c`**

Provides a lightweight view into a single channel (R, G, B, or A) of a `cursed_img` without copying data.

```c
typedef struct {
    uint8_t* start;    /* pointer into img->pxs */
    size_t   h, w;
    uint64_t mask;     /* bitmask for this channel */
    uint8_t  offset;   /* right-shift amount to isolate value */
    uint8_t  smpl_sz;  /* bit depth (16 for RGBA64) */
} cursedimg_channel;
```

### Functions

```c
cursedimg_channel channel_of(const cursed_img* img, char ch);
```
Build a channel view. `ch` is `'R'`, `'G'`, `'B'`, or `'A'`.

```c
uint64_t channel_get(const cursedimg_channel* ch, size_t x, size_t y);
```
Read the channel value at pixel `(x, y)`. Returns a value in `[0, 65535]`.

```c
void channel_set(cursedimg_channel* ch, size_t x, size_t y, uint64_t value);
```
Write a channel value at pixel `(x, y)`. Only the bits belonging to this channel are modified; other channels in the same pixel are preserved.

### Implementation detail

`channel_get` and `channel_set` read/write through the raw `uint8_t* start` pointer cast to `tcursed_pix*`. The mask and offset are applied to isolate the target bits:

```c
/* get */
pix = ((const tcursed_pix*)start)[y * w + x];
return (pix & mask) >> offset;

/* set */
*pix = (*pix & ~mask) | (((tcursed_pix)value << offset) & mask);
```

---

## 6. Filters

**`lib/cursedlib/image/filters/greyscale.h/.c`**

```c
void cursed_greyscale(cursed_img* img);
```

Converts an image to greyscale in-place using ITU-R BT.601 luminance coefficients:

```
Y = 0.299*R + 0.587*G + 0.114*B
```

Implemented with fixed-point integer arithmetic scaled to 2^16 (no floating point):

```
Y = (19595*R + 38470*G + 7471*B) >> 16
```

The computed luminance Y is written into the R, G, and B channels. Alpha is untouched. The result maps `[0, 65535]` → `[0, 65535]` exactly.

---

## 7. Convolution Kernels

**`lib/cursedlib/math/kernels.h/.c`**

Implements the 11 standard image processing kernels from [Wikipedia: Kernel (image processing)](https://en.wikipedia.org/wiki/Kernel_(image_processing)) and a generic convolution function.

### Struct

```c
typedef struct {
    int size;                           /* side length: 3 or 5 */
    int values[CURSED_KERNEL_MAX_ELEMS];/* row-major integer coefficients */
    int divisor;                        /* normalization divisor (0 = skip) */
    int use_abs;                        /* 1 = abs(result) before clamping */
} cursed_kernel;
```

`use_abs = 1` is set for kernels that produce signed gradient values (edge detection, Sobel, emboss) — the absolute value gives the edge magnitude.

### Predefined kernels

| Constant | Size | Effect |
|----------|------|--------|
| `CURSED_KERNEL_IDENTITY` | 3×3 | No change |
| `CURSED_KERNEL_EDGE1` | 3×3 | Laplacian edge detection (4-connected) |
| `CURSED_KERNEL_EDGE2` | 3×3 | Laplacian edge detection (8-connected) |
| `CURSED_KERNEL_SHARPEN` | 3×3 | Edge sharpening |
| `CURSED_KERNEL_BOX_BLUR` | 3×3 | Uniform average blur (÷9) |
| `CURSED_KERNEL_GAUSSIAN3` | 3×3 | Gaussian blur (÷16) |
| `CURSED_KERNEL_GAUSSIAN5` | 5×5 | Gaussian blur (÷256) |
| `CURSED_KERNEL_UNSHARP5` | 5×5 | Unsharp masking (÷256) |
| `CURSED_KERNEL_EMBOSS` | 3×3 | Directional emboss effect |
| `CURSED_KERNEL_SOBEL_GX` | 3×3 | Vertical edge magnitude |
| `CURSED_KERNEL_SOBEL_GY` | 3×3 | Horizontal edge magnitude |

### Function

```c
void cursed_apply_kernel(cursed_img* img, const cursed_kernel* k);
```

Applies the kernel in-place. R, G, B channels are convolved independently; alpha is untouched.

**Algorithm:**
1. Allocate a `uint16_t` snapshot buffer of the channel's current values.
2. For each pixel, compute the weighted sum of neighbors using the snapshot (so mid-pass writes don't affect the result).
3. Divide by `k->divisor`, apply `abs()` if `k->use_abs`, then clamp to `[0, 65535]`.
4. Write the result back via `channel_set`.
5. Repeat for G and B. Free the buffer.

**Boundary handling:** clamp-to-edge — out-of-bounds neighbor coordinates are clamped to the nearest valid pixel.

---

## 8. Drawing Primitives

**`lib/cursedlib/image/draw/draw.h/.c`**

All functions write directly into a `cursed_img` using the current color pixel.

### Outline shapes

```c
void draw_line(int x0, int y0, int x1, int y1, tcursed_pix color, cursed_img* img);
void draw_circle(int cx, int cy, int r, tcursed_pix color, cursed_img* img);
void draw_rectangle(int x0, int y0, int x1, int y1, tcursed_pix color, cursed_img* img);
void draw_triangle(int x0, int y0, int x1, int y1, int x2, int y2, tcursed_pix color, cursed_img* img);
```

### Filled shapes

```c
void fill_rectangle(int x0, int y0, int x1, int y1, tcursed_pix color, cursed_img* img);
void fill_circle(int cx, int cy, int r, tcursed_pix color, cursed_img* img);
void fill_triangle(int x0, int y0, int x1, int y1, int x2, int y2, tcursed_pix color, cursed_img* img);
```

### Algorithms used

| Primitive | Algorithm |
|-----------|-----------|
| Line | Bresenham's line algorithm |
| Circle (outline) | Midpoint circle algorithm with 8-way symmetry |
| Circle (fill) | Scanline fill using midpoint circle bounds |
| Triangle (fill) | Edge-based rasterization with scanline |

All functions perform bounds checking — pixels outside the image dimensions are silently skipped.

---

## 9. Bit Depth

**`lib/cursedlib/image/bitdepth/bitdepth.h/.c`**

Utilities for resampling channel values between different bit depths (e.g., 8-bit to 16-bit).

Used internally by the BMP conversion functions when loading/saving 8-bit images into the 16-bit internal format.

---

## 10. Gamma

**`lib/cursedlib/image/gamma/gamma.h/.c`**

Placeholder for gamma correction. Currently not implemented.

---

## 11. Bitmap I/O

### `lib/bitmap/bitmap.h/.c`

Reads and writes Windows BMP files (24-bit or 32-bit).

```c
typedef struct {
    int      width, height;
    int      bpp;       /* bits per pixel: 24 or 32 */
    uint8_t* pixels;    /* RGBA, 4 bytes per pixel */
} bitmap;

bitmap* read_bitmap(const char* filename);
int     write_bitmap(const bitmap* bmp, const char* filename);
void    free_bitmap(bitmap* bmp);
```

`pixels` is always stored as 4-byte RGBA regardless of the source BMP's `bpp`.

### `lib/bitmap/bitmap_cursed.h/.c`

Converts between the 8-bit `bitmap` format and the internal 16-bit `cursed_img` format.

```c
cursed_img* bitmap_to_cursed(const bitmap* bmp);
bitmap*     cursed_to_bitmap(const cursed_img* img);
```

**8-bit → 16-bit:** Each channel value `v` is expanded as `(v << 8) | v`. This replicates the high byte into the low byte, ensuring that 255 maps to 65535 and 0 maps to 0, with a linear distribution across the full range.

**16-bit → 8-bit:** Each channel is downsampled by `value >> 8` (take the high byte).

---

## 12. PNG

**`lib/png/`**

A from-scratch PNG encoder/decoder supporting truecolor + alpha (RGBA8).

Key types:
- `ihdr_chunk` — image header (width, height, bit depth, color type)
- `png_s` — assembled PNG structure

```c
png_s* create_png(const ihdr_chunk* ihdr, const uint8_t* pixels, int channels);
int    write_png(const char* filename, const png_s* png);
void   free_png(png_s* png);
```

`IHDR_TRUECOLOR8_A8(w, h)` is a convenience macro for creating a standard 8-bit RGBA PNG header.

PNG data is compressed using the DEFLATE algorithm (see §13).

---

## 13. Compression Stack

The project implements the full zlib/DEFLATE/gzip stack from scratch, with no external library dependency.

| Module | File | Purpose |
|--------|------|---------|
| Huffman coding | `lib/huffman/` | Prefix-free codes used by DEFLATE |
| LZSS | `lib/lzss/` | LZ77-style string matching |
| DEFLATE | `lib/deflate/` | DEFLATE compression (RFC 1951) |
| zlib | `lib/zlib/` | zlib wrapper with Adler-32 checksum (RFC 1950) |
| gzip | `lib/gzip/` | gzip wrapper with CRC-32 and size trailer (RFC 1952) |
| Adler-32 | `lib/adler32/` | Checksum used by zlib |
| CRC-32 | `lib/crc32/` | Checksum used by gzip |

### Bit-level I/O

**`lib/bithelper/`** — `bitarray` type and functions for reading/writing individual bits, used throughout the compression modules.

### gzip CLI helper (`src/cursed.c`)

```c
int write_gzip_from_file(const char* src, bitarray* out);
```

Reads a file and produces a gzip-compressed `bitarray`.

---

## 14. Helpers

**`lib/cursedhelpers.h`**

Logging macros controlled by compile-time flags:

| Macro | Flag | Purpose |
|-------|------|---------|
| `LOG_I(fmt, ...)` | `-DDEBUG` | Info-level log to stdout |
| `LOG_V(fmt, ...)` | `-DVERBOSE` | Verbose log to stdout |
| `LOG_E(fmt, ...)` | always | Error log (supports optional callback) |
| `PRINT_BINARY(val, bits)` | debug | Print a value in binary |

Build with `make DFLAGS=-DDEBUG` to enable info logs, `DFLAGS=-DVERBOSE` for verbose output.

---

## 15. Utilities

### `lib/search.h`

Generic linear/binary search macros that operate on any array type via a user-supplied comparator.

### `lib/sort.h`

Generic sort macro(s) — likely insertion sort or similar — operating on any array type.

---

## 16. CLI Entry Point

**`src/cursed.c`**

```
Usage: cursed <input.bmp> <output> [-grey] [-png] [-gzip]
```

| Flag | Effect |
|------|--------|
| `-grey` | Apply greyscale filter before saving |
| `-png` | Encode output as PNG (default is BMP) |
| `-gzip` | Compress the output file with gzip |

Flags can be combined: `-png -gzip` produces a gzip-compressed PNG.

**Flow:**
1. Read input BMP → `bitmap`
2. Convert to internal `cursed_img`
3. Optionally apply `cursed_greyscale()`
4. Convert back to `bitmap`
5. Write as BMP or encode as PNG; optionally gzip the result

If no image file is given, the program launches the TUI instead.

---

## 17. TUI

**`src/cursedtui.c`**, **`include/cursedtui.h`**

The interactive terminal interface. Renders a split-pane layout:

```
┌─────────────────────────────────────┐
│  Log output (scrolling messages)    │
├─────────────────────────────────────┤
│  Command input line                 │
└─────────────────────────────────────┘
```

### Key functions

```c
void run_tui(void);
```
Enters the TUI event loop. Reads a line of input, passes it to the parser, then dispatches to `execute_command()`. Exits when the command returns 0 (i.e., `exit` command).

```c
void add_log(const char* message);
```
Appends a message to the scrolling log panel. Used by all command handlers to report results or errors.

---

## 18. TUI State

**`src/tui_state.c`**, **`include/tui_state.h`**

Global state shared across the TUI modules.

### Layers

```c
#define MAX_LAYERS   8
#define MAX_HISTORY 16

typedef struct {
    char        name[64];
    int         is_active;
    int         op_count;
    cursed_img* img_data;
} tui_layer;

extern tui_layer layers[MAX_LAYERS];
extern int       selected_layer_idx;
```

Up to 8 named layers can exist simultaneously. `selected_layer_idx` is the index of the currently active layer (-1 if none).

### Canvas

```c
extern int canvas_width;
extern int canvas_height;
```

The canvas size is locked to the dimensions of the first image loaded (or the first `new` command). All subsequent layers must match this size; mismatched images are centered and padded/cropped automatically.

### Color

```c
extern tcursed_pix current_color;
```

The active drawing color used by all draw commands. Set with the `color` command.

### Helpers

```c
tcursed_pix make_pixel(int r, int g, int b, int a);
int         get_layer_idx_by_name(const char* name);
```

---

## 19. Command Parser

**`src/parser.c`**, **`include/parser.h`**

Parses a raw input string into a `CommandAST` struct.

### Command types

```c
typedef enum {
    CMD_EMPTY,
    CMD_EXIT,
    CMD_LIST,
    CMD_LOAD,
    CMD_SELECT,
    CMD_NEW,
    CMD_CLEAR,
    CMD_COLOR,
    CMD_RECT,
    CMD_SAVE,
    CMD_EVAL,
    CMD_BLUR,
    CMD_INVERT,
    CMD_UNKNOWN
} CommandType;
```

### CommandAST

```c
typedef struct {
    CommandType type;
    int         num_args;
    char        args[MAX_ARGS][MAX_ARG_LEN];
} CommandAST;
```

### Argument accessors

```c
int         get_arg_int(const CommandAST* ast, int idx, int default_val);
const char* get_arg_str(const CommandAST* ast, int idx, const char* default_val);
```

Safe accessors that return the default if the argument index is out of range.

---

## 20. Command Executor

**`src/tui_exec.c`**, **`include/tui_exec.h`**

```c
int execute_command(CommandAST ast);
```

Returns 1 to continue the TUI loop, 0 to exit.

Dispatches on `ast.type`. Each case operates on the global `layers[]` array and `selected_layer_idx`.

### Commands

| Command | Syntax | Description |
|---------|--------|-------------|
| `list` | `list` | Show files in the current directory (with numeric index) |
| `load` | `load <file\|index>` | Load a BMP into a new layer; centers/crops to canvas if needed |
| `select` | `select <name>` | Switch the active layer |
| `new` | `new [w h] [name]` | Create a blank layer; canvas size locked after first use |
| `clear` | `clear` | Destroy all layers and reset the canvas |
| `color` | `color <r> <g> <b> [a]` | Set the active drawing color (0–255 per channel) |
| `rect` | `rect <x0> <y0> <x1> <y1>` | Draw a rectangle outline on the active layer |
| `save` | `save [format] [filename]` | Save active layer as PNG (default) or BMP |
| `eval` | `eval <dest> = <expr>` | Evaluate a pixel math expression (see §21) |
| `blur` | `blur` | Stub (wired to `CMD_BLUR`; hook `cursed_apply_kernel` here) |
| `invert` | `invert` | Stub (wired to `CMD_INVERT`) |
| `exit` | `exit` | Quit the TUI |

### Canvas fitting (on `load`)

When a loaded image doesn't match the locked canvas size, `fit_to_canvas()` creates a new image of the canvas dimensions, zero-fills it (transparent), and copies the source image centered within it. Pixels outside the canvas are cropped; empty borders are left transparent.

---

## 21. Math / Expression Evaluator

**`src/tui_math.c`**, **`include/tui_math.h`**

Powers the `eval` command. Parses and evaluates arithmetic expressions over layer pixels.

### Usage

```
eval <dest> = <expression>
```

Example:
```
eval result = L0 + L1 * 0.5
```

`L0`, `L1`, … refer to layers by index. The expression is evaluated independently for every pixel position; each `Ln` reference returns the RGB values of that layer's pixel at the current position.

### AST

```c
typedef enum {
    AST_OP_ADD, AST_OP_SUB, AST_OP_MUL, AST_OP_DIV,
    AST_LAYER,
    AST_CONST
} ASTNodeType;

typedef struct ASTNode {
    ASTNodeType   type;
    float         value;     /* used when type == AST_CONST */
    struct ASTNode* left;
    struct ASTNode* right;
} ASTNode;
```

### Functions

```c
ASTNode* init_and_parse_ast(const char* rhs_string);
```
Parse the right-hand side of an `eval` expression into an AST. Returns NULL on syntax error.

```c
int check_ast_layers(ASTNode* node, int* w, int* h);
```
Validates that all layer references exist and that their dimensions match. Writes the canvas dimensions into `*w` and `*h`.

```c
RGBFloat eval_ast(ASTNode* node, size_t p_idx);
```
Evaluate the AST for pixel index `p_idx`. Returns an `RGBFloat` with `r`, `g`, `b` components. The caller clamps these to `[0, 65535]` before writing.

### RGBFloat

```c
typedef struct { float r, g, b; } RGBFloat;
```

Intermediate type used during expression evaluation to allow fractional math between integer pixel values.
