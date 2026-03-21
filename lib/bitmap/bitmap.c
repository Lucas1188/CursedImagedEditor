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
    *out = (uint32_t)b[0]
         | ((uint32_t)b[1] << 8)
         | ((uint32_t)b[2] << 16)
         | ((uint32_t)b[3] << 24);
    return 1;
}

bitmap* read_bitmap(const char* filename){
    FILE*    f;
    uint8_t  sig[2];
    uint32_t file_size, reserved, pixel_offset;
    uint32_t dib_size, raw_w, raw_h;
    uint32_t compression, image_size, xppm, yppm, clrs, iclrs;
    uint32_t row_stride, out_row, out_idx, in_idx, row;
    int32_t  width, height;
    uint16_t u_planes, u_bpp;
    uint8_t* row_buf;
    bitmap*  bmp;
    int      flip, x;

    f = fopen(filename, "rb");
    if(!f){
        LOG_E("Failed to open file: %s\n", filename);
        return NULL;
    }

    /* --- File Header (14 bytes) --- */
    if(fread(sig, 1, 2, f) != 2 || sig[0] != 'B' || sig[1] != 'M'){
        LOG_E("Invalid BMP signature: %s\n", filename);
        fclose(f);
        return NULL;
    }
    if(!read_u32_le(f, &file_size) ||
       !read_u32_le(f, &reserved)  ||
       !read_u32_le(f, &pixel_offset)){
        LOG_E("Failed to read BMP file header\n");
        fclose(f);
        return NULL;
    }
    LOG_I("File size: %u  Pixel offset: %u\n", file_size, pixel_offset);

    /* --- DIB Header (BITMAPINFOHEADER = 40 bytes) --- */
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
    if(!read_u32_le(f, &raw_w)      ||
       !read_u32_le(f, &raw_h)      ||
       !read_u16_le(f, &u_planes)   ||
       !read_u16_le(f, &u_bpp)      ||
       !read_u32_le(f, &compression)||
       !read_u32_le(f, &image_size) ||
       !read_u32_le(f, &xppm)       ||
       !read_u32_le(f, &yppm)       ||
       !read_u32_le(f, &clrs)       ||
       !read_u32_le(f, &iclrs)){
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

    /* Negative height means the image is already top-down, no flip needed */
    flip = 1;
    if(height < 0){
        height = -height;
        flip   = 0;
    }

    /* Seek to pixel data (skips color table and any extra DIB header bytes) */
    if(fseek(f, (long)pixel_offset, SEEK_SET) != 0){
        LOG_E("Failed to seek to pixel data at offset %u\n", pixel_offset);
        fclose(f);
        return NULL;
    }

    /* BMP rows are padded to a 4-byte boundary */
    if(u_bpp == 32){
        row_stride = (uint32_t)width * 4;
    }else{
        row_stride = ((uint32_t)width * 3 + 3) & ~3u;
    }

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
        Read each row from the file.
        BMP stores BGR (24-bit) or BGRA (32-bit) — we output RGBA.
        When flip=1, row 0 in the file is the bottom row of the image,
        so we write it into the last row of the output buffer.
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

        out_row = flip ? (bmp->height - 1 - row) : row;

        for(x = 0; x < (int)bmp->width; x++){
            out_idx = (out_row * bmp->width + (uint32_t)x) * 4;
            if(u_bpp == 32){
                in_idx = (uint32_t)x * 4;
                bmp->pixels[out_idx + 0] = row_buf[in_idx + 2]; /* R */
                bmp->pixels[out_idx + 1] = row_buf[in_idx + 1]; /* G */
                bmp->pixels[out_idx + 2] = row_buf[in_idx + 0]; /* B */
                bmp->pixels[out_idx + 3] = row_buf[in_idx + 3]; /* A */
            }else{
                in_idx = (uint32_t)x * 3;
                bmp->pixels[out_idx + 0] = row_buf[in_idx + 2]; /* R */
                bmp->pixels[out_idx + 1] = row_buf[in_idx + 1]; /* G */
                bmp->pixels[out_idx + 2] = row_buf[in_idx + 0]; /* B */
                bmp->pixels[out_idx + 3] = 0xFF;                /* A = opaque */
            }
        }
    }

    free(row_buf);
    fclose(f);
    LOG_I("Bitmap loaded: %ux%u %ubpp\n", bmp->width, bmp->height, bmp->bpp);
    return bmp;
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
