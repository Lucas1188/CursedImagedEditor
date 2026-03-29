#include "bitmap_cursed.h"
#include "../cursedhelpers.h"
#include <stdlib.h>
#include <string.h>

cursed_img* bitmap_to_cursed(const bitmap* bmp){
    cursed_img* img;
    size_t      i, npix;
    uint64_t    r, g, b, a;

    if(!bmp) return NULL;

    img = (cursed_img*)malloc(sizeof(cursed_img));
    if(!img){ LOG_E("bitmap_to_cursed: alloc failed\n"); return NULL; }

    img->width  = bmp->width;
    img->height = bmp->height;
    {
        spixel_fmt tmp = CURSED_RGBA64_PXFMT;
        memcpy(&img->px_fmt, &tmp, sizeof(spixel_fmt));
    }

    npix    = (size_t)bmp->width * bmp->height;
    img->pxs = (tcursed_pix*)malloc(npix * sizeof(tcursed_pix));
    if(!img->pxs){
        LOG_E("bitmap_to_cursed: pixel alloc failed\n");
        free(img);
        return NULL;
    }

    for(i = 0; i < npix; i++){
        /* bmp->pixels is 8-bit RGBA, tightly packed */
        r = bmp->pixels[i * 4 + 0];
        g = bmp->pixels[i * 4 + 1];
        b = bmp->pixels[i * 4 + 2];
        a = bmp->pixels[i * 4 + 3];

        /*
            Scale 8-bit -> 16-bit via left bit replication: (v << 8) | v
            This maps the full 8-bit range onto the full 16-bit range:
                0   -> 0x0000
                128 -> 0x8080
                255 -> 0xFFFF
        */
        r = (r << 8) | r;
        g = (g << 8) | g;
        b = (b << 8) | b;
        a = (a << 8) | a;

        /* Pack into 64-bit pixel: R[63:48] G[47:32] B[31:16] A[15:0] */
        img->pxs[i] = (r << 48) | (g << 32) | (b << 16) | a;
    }

    LOG_I("bitmap_to_cursed: %zux%zu converted\n", img->width, img->height);
    return img;
}

bitmap* cursed_to_bitmap(const cursed_img* img){
    bitmap*     bmp;
    size_t      i, npix;
    tcursed_pix px;

    if(!img) return NULL;

    bmp = (bitmap*)malloc(sizeof(bitmap));
    if(!bmp){ LOG_E("cursed_to_bitmap: alloc failed\n"); return NULL; }

    bmp->width  = (uint32_t)img->width;
    bmp->height = (uint32_t)img->height;
    bmp->bpp    = 24;

    npix = (size_t)img->width * img->height;
    bmp->pixels = (uint8_t*)malloc(npix * 4);
    if(!bmp->pixels){
        LOG_E("cursed_to_bitmap: pixel alloc failed\n");
        free(bmp);
        return NULL;
    }

    for(i = 0; i < npix; i++){
        px = img->pxs[i];

        /*
            Each channel is 16 bits. Extract its high byte to get the 8-bit value.
            R is at bits [63:48]: shift right 56 to get bits [63:56] into [7:0]
            G is at bits [47:32]: shift right 40 to get bits [47:40] into [7:0]
            B is at bits [31:16]: shift right 24 to get bits [31:24] into [7:0]
            A is at bits [15: 0]: shift right  8 to get bits [15: 8] into [7:0]
        */
        bmp->pixels[i * 4 + 0] = (uint8_t)(px >> 56); /* R */
        bmp->pixels[i * 4 + 1] = (uint8_t)(px >> 40); /* G */
        bmp->pixels[i * 4 + 2] = (uint8_t)(px >> 24); /* B */
        bmp->pixels[i * 4 + 3] = (uint8_t)(px >>  8); /* A */
    }

    LOG_I("cursed_to_bitmap: %ux%u converted\n", bmp->width, bmp->height);
    return bmp;
}
