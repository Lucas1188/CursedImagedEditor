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
    char        channels[3];

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
/* Custom exponential function (Taylor Series) to avoid math.h */
static double cursed_exp(double x) {
    double sum = 1.0;
    double term = 1.0;
    int is_negative = 0;
    int i;

    /* Handle negative exponents safely by calculating the positive and dividing */
    if (x < 0.0) {
        is_negative = 1;
        x = -x;
    }

    /* 25 iterations is more than enough precision for a Gaussian visual curve */
    for (i = 1; i < 25; i++) {
        term *= (x / (double)i);
        sum += term;
        
        /* Early exit if the numbers get too microscopic to matter */
        if (term < 0.0000001) break; 
    }

    if (is_negative) {
        return 1.0 / sum;
    }
    return sum;
}
/* * Applies a true two-pass separable Gaussian blur.
 * radius: The distance the blur spreads (e.g., radius 50 = 101x101 effective kernel)
 * sigma: The "softness" of the blur. If 0, it auto-calculates a good default.
 */
void cursed_apply_separable_blur(cursed_img* img, int radius, double sigma) {
    int size = radius * 2 + 1;
    double* kernel;
    double sum = 0.0;
    int i, x, y, kx, ky, sx, sy, ci;
    size_t w, h;
    double *buf_in, *buf_out;
    const char channels[3] = {'R', 'G', 'B'};
    cursedimg_channel ch;

    if (!img || !img->pxs || radius < 1) return;

    /* Auto-calculate standard deviation if none provided */
    if (sigma <= 0.0) sigma = (double)radius / 3.0;

    w = img->width;
    h = img->height;

    kernel = (double*)malloc(size * sizeof(double));
    buf_in = (double*)malloc(w * h * sizeof(double));
    buf_out = (double*)malloc(w * h * sizeof(double));

    /* Safe allocation check */
    if (!kernel || !buf_in || !buf_out) {
        if (kernel) free(kernel);
        if (buf_in) free(buf_in);
        if (buf_out) free(buf_out);
        return;
    }

    /* --- 1. GENERATE THE 1D GAUSSIAN KERNEL --- */
    for (i = -radius; i <= radius; i++) {
        /* Call our custom math function! */
        double val = cursed_exp(-((double)(i * i)) / (2.0 * sigma * sigma));
        kernel[i + radius] = val;
        sum += val;
    }
    
    /* Normalize the kernel so the image doesn't get brighter or darker */
    for (i = 0; i < size; i++) {
        kernel[i] /= sum;
    }

    /* --- 2. APPLY THE TWO-PASS BLUR --- */
    for (ci = 0; ci < 3; ci++) {
        ch = channel_of(img, channels[ci]);

        /* Snapshot the channel into buf_in */
        for (y = 0; y < (int)h; y++) {
            for (x = 0; x < (int)w; x++) {
                buf_in[y * w + x] = (double)channel_get(&ch, x, y);
            }
        }

        /* PASS A: Horizontal Blur (Read buf_in, write to buf_out) */
        for (y = 0; y < (int)h; y++) {
            for (x = 0; x < (int)w; x++) {
                double acc = 0.0;
                for (kx = -radius; kx <= radius; kx++) {
                    sx = x + kx;
                    /* Clamp edges */
                    if (sx < 0) sx = 0;
                    if (sx >= (int)w) sx = (int)w - 1;
                    
                    acc += buf_in[y * w + sx] * kernel[kx + radius];
                }
                buf_out[y * w + x] = acc;
            }
        }

        /* PASS B: Vertical Blur (Read buf_out, write to image) */
        for (y = 0; y < (int)h; y++) {
            for (x = 0; x < (int)w; x++) {
                double acc = 0.0;
                for (ky = -radius; ky <= radius; ky++) {
                    sy = y + ky;
                    /* Clamp edges */
                    if (sy < 0) sy = 0;
                    if (sy >= (int)h) sy = (int)h - 1;
                    
                    acc += buf_out[sy * w + x] * kernel[ky + radius];
                }

                /* Clamp to 16-bit space and write back */
                if (acc < 0.0) acc = 0.0;
                if (acc > 65535.0) acc = 65535.0;
                channel_set(&ch, x, y, (uint64_t)acc);
            }
        }
    }

    free(kernel);
    free(buf_in);
    free(buf_out);
}