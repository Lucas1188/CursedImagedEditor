#include <stdio.h>
#include <stdlib.h>

#include "bitmap.h"
#include "../cursedhelpers.h"

static int read_u16_le(FILE* f, uint16_t* out){
    uint8_t b[2];
    if(fread(b, 1, 2, f) != 2) return 0;
    *out = (uint16_t)(b[0] | ((uint16_t)b[1] << 8));
    return 1;
}

static int read_u32_le(FILE* f, uint32_t* out){
    uint8_t b[4];
    if(fread(b, 1, 4, f) != 4) return 0;
    *out = b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
    return 1;
}

bitmap* read_bitmap(const char* filename){
    FILE*    f;
    uint8_t  sig[2];
    uint32_t pixel_offset, dib_size, raw_w, raw_h, compression, dummy;
    uint32_t row_stride, row;
    int32_t  width, height;
    uint16_t u_bpp, dummy16;
    uint8_t* row_buf;
    uint8_t* out;
    uint8_t* in;
    bitmap*  bmp;
    uint32_t x;
    int      flip;

    f = fopen(filename, "rb");
    if(!f){
        LOG_E("Failed to open file: %s\n", filename);
        return NULL;
    }

    /* File Header (14 bytes) */
    if(fread(sig, 1, 2, f) != 2 || sig[0] != 'B' || sig[1] != 'M'){
        LOG_E("Invalid BMP signature: %s\n", filename);
        fclose(f);
        return NULL;
    }
    if(!read_u32_le(f, &dummy)        ||  /* file_size - unused */
       !read_u32_le(f, &dummy)        ||  /* reserved  - unused */
       !read_u32_le(f, &pixel_offset)){
        LOG_E("Failed to read BMP file header\n");
        fclose(f);
        return NULL;
    }
    LOG_I("Pixel offset: %u\n", pixel_offset);

    /* DIB Header (BITMAPINFOHEADER = 40 bytes) */
    if(!read_u32_le(f, &dib_size)){
        LOG_E("Failed to read DIB header size\n");
        fclose(f);
        return NULL;
    }
    if(dib_size < 40){
        LOG_E("Unsupported DIB header size %u, need BITMAPINFOHEADER (40)\n", dib_size);
        fclose(f);
        return NULL;
    }
    if(!read_u32_le(f, &raw_w)           ||
       !read_u32_le(f, &raw_h)           ||
       !read_u16_le(f, &dummy16)         ||  /* planes - unused */
       !read_u16_le(f, &u_bpp)           ||
       !read_u32_le(f, &compression)     ||
       fseek(f, 20, SEEK_CUR) != 0){         /* skip: image_size, xppm, yppm, clrs, iclrs */
        LOG_E("Failed to read DIB header fields\n");
        fclose(f);
        return NULL;
    }
    width  = (int32_t)raw_w;
    height = (int32_t)raw_h;

    LOG_I("Width: %d  Height: %d  BPP: %u  Compression: %u\n",
           width, height, u_bpp, compression);

    if(u_bpp != 24 && u_bpp != 32){
        LOG_E("Unsupported bit depth: %u (only 24 and 32 supported)\n", u_bpp);
        fclose(f);
        return NULL;
    }
    if(compression != 0){
        LOG_E("Unsupported compression: %u (only BI_RGB=0 supported)\n", compression);
        fclose(f);
        return NULL;
    }

    /* Negative height = top-down, no flip needed */
    flip = (height > 0);
    if(height < 0) height = -height;

    /* Seek to pixel data (skips color table and any extra DIB header bytes) */
    if(fseek(f, (long)pixel_offset, SEEK_SET) != 0){
        LOG_E("Failed to seek to pixel data at offset %u\n", pixel_offset);
        fclose(f);
        return NULL;
    }

    /* BMP rows are padded to a 4-byte boundary */
    row_stride = (u_bpp == 32) ? (uint32_t)width * 4 : (((uint32_t)width * 3 + 3) & ~3u);

    row_buf = (uint8_t*)malloc(row_stride);
    if(!row_buf){
        LOG_E("Failed to allocate row buffer\n");
        fclose(f);
        return NULL;
    }

    bmp = (bitmap*)malloc(sizeof(bitmap));
    if(!bmp){
        LOG_E("Failed to allocate bitmap struct\n");
        free(row_buf);
        fclose(f);
        return NULL;
    }

    bmp->width  = (uint32_t)width;
    bmp->height = (uint32_t)height;
    bmp->bpp    = u_bpp;
    bmp->pixels = (uint8_t*)malloc(bmp->width * bmp->height * 4);
    if(!bmp->pixels){
        LOG_E("Failed to allocate pixel buffer\n");
        free(row_buf);
        free(bmp);
        fclose(f);
        return NULL;
    }

    /*
        BMP stores BGR (24-bit) or BGRA (32-bit) — we output RGBA.
        flip=1: row 0 in file is the bottom row, write rows in reverse.
    */
    for(row = 0; row < bmp->height; row++){
        if(fread(row_buf, 1, row_stride, f) != row_stride){
            LOG_E("Failed to read row %u\n", row);
            free(row_buf);
            free(bmp->pixels);
            free(bmp);
            fclose(f);
            return NULL;
        }

        out = bmp->pixels + (flip ? bmp->height - 1 - row : row) * bmp->width * 4;
        in  = row_buf;
        for(x = 0; x < bmp->width; x++, out += 4, in += u_bpp / 8){
            out[0] = in[2]; out[1] = in[1]; out[2] = in[0];
            out[3] = (u_bpp == 32) ? in[3] : 0xFF;
        }
    }

    free(row_buf);
    fclose(f);
    LOG_I("Bitmap loaded: %ux%u %ubpp\n", bmp->width, bmp->height, bmp->bpp);
    return bmp;
}

static int write_u16_le(FILE* f, uint16_t v){
    uint8_t b[2];
    b[0] = (uint8_t)(v);
    b[1] = (uint8_t)(v >> 8);
    return fwrite(b, 1, 2, f) == 2;
}

static int write_u32_le(FILE* f, uint32_t v){
    uint8_t b[4];
    b[0] = (uint8_t)(v);
    b[1] = (uint8_t)(v >>  8);
    b[2] = (uint8_t)(v >> 16);
    b[3] = (uint8_t)(v >> 24);
    return fwrite(b, 1, 4, f) == 4;
}

int write_bitmap(const bitmap* bmp, const char* filename){
    FILE*    f;
    uint32_t row_stride, pad, file_size, row, x;
    uint8_t* in;
    uint8_t  bgr[3];
    uint8_t  zeros[3] = {0, 0, 0};

    if(!bmp || !filename) return 0;

    /* 24-bit output: 3 bytes per pixel, rows padded to 4-byte boundary */
    row_stride = ((bmp->width * 3 + 3) & ~3u);
    pad        = row_stride - bmp->width * 3;
    file_size  = 14 + 40 + row_stride * bmp->height;

    f = fopen(filename, "wb");
    if(!f){ LOG_E("write_bitmap: failed to open %s\n", filename); return 0; }

    /* File header (14 bytes) */
    fwrite("BM", 1, 2, f);
    write_u32_le(f, file_size);
    write_u32_le(f, 0);   /* reserved */
    write_u32_le(f, 54);  /* pixel data starts at byte 54: 14 + 40 */

    /* DIB header / BITMAPINFOHEADER (40 bytes) */
    write_u32_le(f, 40);              /* header size */
    write_u32_le(f, bmp->width);
    write_u32_le(f, bmp->height);     /* positive = bottom-up storage */
    write_u16_le(f, 1);               /* color planes */
    write_u16_le(f, 24);              /* bits per pixel */
    write_u32_le(f, 0);               /* compression = BI_RGB */
    write_u32_le(f, row_stride * bmp->height); /* raw pixel data size */
    write_u32_le(f, 0);               /* x pixels per meter */
    write_u32_le(f, 0);               /* y pixels per meter */
    write_u32_le(f, 0);               /* colors in table */
    write_u32_le(f, 0);               /* important colors */

    /*
        Pixel data: BMP stores rows bottom-to-top.
        bmp->pixels is top-left first, so row 0 of our buffer
        is the top row of the image, which must be written last.
    */
    for(row = 0; row < bmp->height; row++){
        in = bmp->pixels + (bmp->height - 1 - row) * bmp->width * 4;
        for(x = 0; x < bmp->width; x++, in += 4){
            bgr[0] = in[2]; /* B */
            bgr[1] = in[1]; /* G */
            bgr[2] = in[0]; /* R */
            fwrite(bgr, 1, 3, f);
        }
        if(pad) fwrite(zeros, 1, pad, f);
    }

    fclose(f);
    LOG_I("write_bitmap: %ux%u written to %s\n", bmp->width, bmp->height, filename);
    return 1;
}

void free_bitmap(bitmap* bmp){
    if(!bmp) return;
    free(bmp->pixels);
    free(bmp);
}

#ifdef STANDALONE_BITMAP

int main(int argv, char* argc[]){
    bitmap*  bmp;
    uint32_t x, y, i;

    if(argv < 2){
        printf("Usage: %s <file.bmp>\n", argc[0]);
        return 1;
    }

    bmp = read_bitmap(argc[1]);
    if(!bmp){
        fprintf(stderr, "Failed to read bitmap: %s\n", argc[1]);
        return 1;
    }

    printf("Width: %u  Height: %u  BPP: %u\n", bmp->width, bmp->height, bmp->bpp);
    printf("Pixels (RGBA, top-left first):\n");

    for(y = 0; y < bmp->height; y++){
        for(x = 0; x < bmp->width; x++){
            i = (y * bmp->width + x) * 4;
            printf("[%u,%u] R:%3u G:%3u B:%3u A:%3u\n",
                   x, y,
                   bmp->pixels[i + 0],
                   bmp->pixels[i + 1],
                   bmp->pixels[i + 2],
                   bmp->pixels[i + 3]);
        }
    }

    free_bitmap(bmp);
    return 0;
}

#endif
