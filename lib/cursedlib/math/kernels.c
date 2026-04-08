#include "kernels.h"
#include "../image/channel/channel.h"
#include <stdlib.h>

/* -------------------------------------------------------------------------
   Predefined kernel definitions
   ------------------------------------------------------------------------- */

const cursed_kernel CURSED_KERNEL_IDENTITY = {
    3,
    { 0, 0, 0,
      0, 1, 0,
      0, 0, 0 },
    1, 0
};

const cursed_kernel CURSED_KERNEL_EDGE1 = {
    3,
    { 0,-1, 0,
     -1, 4,-1,
      0,-1, 0 },
    1, 1
};

const cursed_kernel CURSED_KERNEL_EDGE2 = {
    3,
    {-1,-1,-1,
     -1, 8,-1,
     -1,-1,-1 },
    1, 1
};

const cursed_kernel CURSED_KERNEL_SHARPEN = {
    3,
    { 0,-1, 0,
     -1, 5,-1,
      0,-1, 0 },
    1, 0
};

const cursed_kernel CURSED_KERNEL_BOX_BLUR = {
    3,
    { 1, 1, 1,
      1, 1, 1,
      1, 1, 1 },
    9, 0
};

const cursed_kernel CURSED_KERNEL_GAUSSIAN3 = {
    3,
    { 1, 2, 1,
      2, 4, 2,
      1, 2, 1 },
    16, 0
};

const cursed_kernel CURSED_KERNEL_GAUSSIAN5 = {
    5,
    {  1,  4,  6,  4,  1,
       4, 16, 24, 16,  4,
       6, 24, 36, 24,  6,
       4, 16, 24, 16,  4,
       1,  4,  6,  4,  1 },
    256, 0
};

const cursed_kernel CURSED_KERNEL_UNSHARP5 = {
    5,
    {  1,  4,   6,  4,  1,
       4, 16,  24, 16,  4,
       6, 24,-476, 24,  6,
       4, 16,  24, 16,  4,
       1,  4,   6,  4,  1 },
    256, 0
};

const cursed_kernel CURSED_KERNEL_EMBOSS = {
    3,
    {-2,-1, 0,
     -1, 1, 1,
      0, 1, 2 },
    1, 1
};

const cursed_kernel CURSED_KERNEL_SOBEL_GX = {
    3,
    {-1, 0, 1,
     -2, 0, 2,
     -1, 0, 1 },
    1, 1
};

const cursed_kernel CURSED_KERNEL_SOBEL_GY = {
    3,
    {-1,-2,-1,
      0, 0, 0,
      1, 2, 1 },
    1, 1
};

/* -------------------------------------------------------------------------
   cursed_apply_kernel
   ------------------------------------------------------------------------- */

void cursed_apply_kernel(cursed_img* img, const cursed_kernel* k)
{
    uint16_t*         buf;
    cursedimg_channel ch;
    size_t            w, h, x, y;
    int               half, kx, ky, kidx;
    int               sx, sy;
    int               ci;
    int64_t           acc, divided;
    uint64_t          result;
    const char        channels[3];

    if (!img || !img->pxs || !k) return;

    w    = img->width;
    h    = img->height;
    half = k->size / 2;

    ((char*)channels)[0] = 'R';
    ((char*)channels)[1] = 'G';
    ((char*)channels)[2] = 'B';

    buf = (uint16_t*)malloc(w * h * sizeof(uint16_t));
    if (!buf) return;

    for (ci = 0; ci < 3; ci++) {
        ch = channel_of(img, channels[ci]);

        /* Phase 1: snapshot channel values into temp buffer */
        for (y = 0; y < h; y++) {
            for (x = 0; x < w; x++) {
                buf[y * w + x] = (uint16_t)channel_get(&ch, x, y);
            }
        }

        /* Phase 2: convolve and write back */
        for (y = 0; y < h; y++) {
            for (x = 0; x < w; x++) {
                acc = 0;

                for (ky = -half; ky <= half; ky++) {
                    for (kx = -half; kx <= half; kx++) {
                        sx = (int)x + kx;
                        sy = (int)y + ky;

                        if (sx < 0)       sx = 0;
                        if (sx >= (int)w) sx = (int)w - 1;
                        if (sy < 0)       sy = 0;
                        if (sy >= (int)h) sy = (int)h - 1;

                        kidx = (ky + half) * k->size + (kx + half);
                        acc += (int64_t)k->values[kidx] * buf[sy * w + sx];
                    }
                }

                divided = (k->divisor != 0) ? acc / (int64_t)k->divisor : acc;

                if (k->use_abs && divided < 0) divided = -divided;

                if (divided < 0)     divided = 0;
                if (divided > 65535) divided = 65535;

                result = (uint64_t)divided;
                channel_set(&ch, x, y, result);
            }
        }
    }

    free(buf);
}
