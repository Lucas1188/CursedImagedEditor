#include "png_cursed.h"
#include "../cursedhelpers.h"
#include <stdlib.h>
#include <string.h>

cursed_img* png_to_cursed(const png_img* img)
{
    cursed_img* out;
    spixel_fmt  tmp;
    size_t      i, npix;
    uint64_t    r, g, b, a;
    const uint8_t* p;

    if(!img || !img->pixels) return NULL;

    out = (cursed_img*)malloc(sizeof(cursed_img));
    if(!out) { LOG_E("png_to_cursed: alloc failed\n"); return NULL; }

    out->width  = img->width;
    out->height = img->height;
    {
        spixel_fmt fmt = CURSED_RGBA64_PXFMT;
        memcpy(&out->px_fmt, &fmt, sizeof(spixel_fmt));
    }

    npix    = (size_t)img->width * img->height;
    out->pxs = (tcursed_pix*)malloc(npix * sizeof(tcursed_pix));
    if(!out->pxs) {
        LOG_E("png_to_cursed: pixel alloc failed\n");
        free(out);
        return NULL;
    }

    if(img->bit_depth == 16) {
        /*
            16-bit input: 8 bytes/pixel, big-endian pairs.
            Reassemble each pair directly into the 16-bit channel value.
            No scaling -- the full precision of the source is preserved.
        */
        for(i = 0; i < npix; i++) {
            p = img->pixels + i * 8;
            r = ((uint64_t)p[0] << 8) | p[1];
            g = ((uint64_t)p[2] << 8) | p[3];
            b = ((uint64_t)p[4] << 8) | p[5];
            a = ((uint64_t)p[6] << 8) | p[7];
            out->pxs[i] = (r << 48) | (g << 32) | (b << 16) | a;
        }
    } else {
        /*
            8-bit input: 4 bytes/pixel.
            Scale 8->16 via left-bit replication: (v << 8) | v
            Maps 0->0x0000 and 255->0xFFFF exactly.
        */
        for(i = 0; i < npix; i++) {
            p = img->pixels + i * 4;
            r = p[0]; g = p[1]; b = p[2]; a = p[3];
            r = (r << 8) | r;
            g = (g << 8) | g;
            b = (b << 8) | b;
            a = (a << 8) | a;
            out->pxs[i] = (r << 48) | (g << 32) | (b << 16) | a;
        }
    }

    return out;
    (void)tmp; /* suppress unused-variable warning if compiler doesn't see the block */
}

png_img* cursed_to_png(const cursed_img* img, uint8_t bit_depth)
{
    png_img*    out;
    size_t      i, npix;
    tcursed_pix px;
    uint16_t    r, g, b, a;
    uint8_t*    p;

    if(!img || !img->pxs) return NULL;
    if(bit_depth != 8 && bit_depth != 16) return NULL;

    out = (png_img*)malloc(sizeof(png_img));
    if(!out) { LOG_E("cursed_to_png: alloc failed\n"); return NULL; }

    out->width      = (uint32_t)img->width;
    out->height     = (uint32_t)img->height;
    out->bit_depth  = bit_depth;
    out->color_type = CT_RGB_ALPHA;

    npix = (size_t)img->width * img->height;

    if(bit_depth == 16) {
        /*
            16-bit output: 8 bytes/pixel, big-endian.
            Extract each 16-bit channel and split into two bytes.
            Fully lossless round-trip from cursed_img.
        */
        out->pixels = (uint8_t*)malloc(npix * 8);
        if(!out->pixels) {
            LOG_E("cursed_to_png: pixel alloc failed\n");
            free(out); return NULL;
        }
        for(i = 0; i < npix; i++) {
            px = img->pxs[i];
            r  = (uint16_t)(px >> 48);
            g  = (uint16_t)(px >> 32);
            b  = (uint16_t)(px >> 16);
            a  = (uint16_t)(px);
            p  = out->pixels + i * 8;
            p[0] = (uint8_t)(r >> 8); p[1] = (uint8_t)(r);
            p[2] = (uint8_t)(g >> 8); p[3] = (uint8_t)(g);
            p[4] = (uint8_t)(b >> 8); p[5] = (uint8_t)(b);
            p[6] = (uint8_t)(a >> 8); p[7] = (uint8_t)(a);
        }
    } else {
        /*
            8-bit output: 4 bytes/pixel.
            Take the high byte of each 16-bit channel (equivalent to >> 8).
        */
        out->pixels = (uint8_t*)malloc(npix * 4);
        if(!out->pixels) {
            LOG_E("cursed_to_png: pixel alloc failed\n");
            free(out); return NULL;
        }
        for(i = 0; i < npix; i++) {
            px = img->pxs[i];
            p  = out->pixels + i * 4;
            p[0] = (uint8_t)(px >> 56); /* R high byte */
            p[1] = (uint8_t)(px >> 40); /* G high byte */
            p[2] = (uint8_t)(px >> 24); /* B high byte */
            p[3] = (uint8_t)(px >>  8); /* A high byte */
        }
    }

    return out;
}
