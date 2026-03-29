# CursedImageEditor

A command-line image processing tool that supports lossless image format conversion and filter application. Images are converted through an internal 64-bit RGBA intermediate format (`cursed_img`), keeping full precision across operations.

Supported formats: **PNG**, **BMP**
Supported filters: **greyscale** (more coming)

---

## Building

From the repo root:

```
make
```

This cleans any previous build and recompiles everything. The output binary is `./cursed` in the repo root.

---

## Usage

```
./cursed <input> <output> [filters...]
```

The input and output formats are detected automatically from the file extension. You can mix formats freely — read a BMP, write a PNG, etc.

### Convert between formats

```bash
# BMP to PNG
./cursed photo.bmp output.png

# PNG to BMP
./cursed photo.png output.bmp

# PNG to PNG (no-op conversion, useful for reencoding)
./cursed photo.png output.png
```

### Apply filters

Filters are passed as flags after the output path. Multiple filters can be chained and are applied in order.

```bash
# Greyscale
./cursed photo.bmp output.png -grey

# Greyscale BMP to BMP
./cursed photo.bmp output.bmp -grey

# Multiple filters (once more are added)
./cursed photo.png output.png -grey -edge
```

### Available filters

| Flag    | Description                                      |
|---------|--------------------------------------------------|
| `-grey` | Convert to greyscale using ITU-R BT.601 weights  |

### GZIP compression

```bash
./cursed -f <input_file> <output_file>
```

---

## Adding a new filter

1. Create `lib/cursedlib/image/filters/<filtername>.h` and `.c`
2. Implement `void cursed_<filtername>(cursed_img* img)` — operates in-place on the `cursed_img`
3. Include the header in `src/cursed.c`
4. Add a flag check inside `apply_filters()` in `src/cursed.c`:
   ```c
   if(strcmp(argc[i], "-filtername") == 0){
       cursed_filtername(img);
   }
   ```
5. Run `make force` from the repo root

---

## Project structure

```
src/
  cursed.c                    # Main entry point
lib/
  png/                        # PNG read/write + cursed_img conversion
  bitmap/                     # BMP read/write + cursed_img conversion
  cursedlib/image/
    image.h                   # cursed_img type (64-bit RGBA intermediate)
    filters/
      greyscale.h/c           # Greyscale filter
  deflate/                    # Deflate compression
  gzip/                       # GZIP wrapper
  zlib/                       # zlib support
  inflate/                    # Inflate decompression
  crc32/                      # CRC32 checksum
```
