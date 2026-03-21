#ifndef BITMAP_H
#define BITMAP_H

#include <stdint.h>

/*
    +---------------------------+
    |   BMP File Header         |
    |   14 bytes                |
    +---------------------------+

    Signature   2 bytes     "BM"
    File size   4 bytes     total file size in bytes
    Reserved    4 bytes     unused (always 0)
    Offset      4 bytes     byte offset to pixel data
*/

/*
    +---------------------------+
    |   DIB Header              |
    |   BITMAPINFOHEADER        |
    |   40 bytes                |
    +---------------------------+

    Header size     4 bytes     must be 40 for BITMAPINFOHEADER
    Width           4 bytes     signed; image width in pixels
    Height          4 bytes     signed; positive = bottom-up, negative = top-down
    Color planes    2 bytes     must be 1
    BPP             2 bytes     bits per pixel (24 or 32 supported)
    Compression     4 bytes     0 = BI_RGB (none), only this is supported
    Image size      4 bytes     raw pixel data size (may be 0 for BI_RGB)
    X pixels/m     4 bytes     horizontal resolution (ignored)
    Y pixels/m     4 bytes     vertical resolution (ignored)
    Colors used     4 bytes     colors in color table (ignored)
    Colors imp.     4 bytes     important colors (ignored)
*/

/*
    +---------------------------+
    |   Pixel Layout            |
    +---------------------------+

    BMP stores pixels in BGR (24-bit) or BGRA (32-bit) order.
    Rows are padded to a 4-byte boundary.
    By default rows are stored bottom-to-top.

    read_bitmap() outputs pixels as RGBA, top-left first:

        pixels[0] = R of pixel (0, 0)  top-left
        pixels[1] = G of pixel (0, 0)
        pixels[2] = B of pixel (0, 0)
        pixels[3] = A of pixel (0, 0)
        pixels[4] = R of pixel (1, 0)  one step right
        ...

    For 24-bit BMPs, alpha is set to 0xFF (fully opaque).
    Always 4 bytes per pixel in the output buffer.
*/

typedef struct bitmap{
    uint32_t  width;
    uint32_t  height;
    uint16_t  bpp;      /* bits per pixel of the source file: 24 or 32 */
    uint8_t*  pixels;   /* RGBA, row-major, top-left first, width*height*4 bytes */
}bitmap;

bitmap* read_bitmap(const char* filename);
void    free_bitmap(bitmap* bmp);

#endif
