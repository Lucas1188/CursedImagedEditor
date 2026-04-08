#ifndef CURSED_KERNELS_H
#define CURSED_KERNELS_H

#include "../image/image.h"
#include <stdint.h>
#include <stddef.h>

#define CURSED_KERNEL_MAX_ELEMS 25 /* max 5x5 */

/*
    cursed_kernel:
        size     - side length of the square kernel (3 or 5)
        values   - row-major integer coefficients
        divisor  - normalization divisor applied after accumulation (0 = no divide)
        use_abs  - if 1, take absolute value of result before clamping
                   (used for edge detection / gradient kernels)
*/
typedef struct {
    int size;
    int values[CURSED_KERNEL_MAX_ELEMS];
    int divisor;
    int use_abs;
} cursed_kernel;

/*
    Predefined kernels from https://en.wikipedia.org/wiki/Kernel_(image_processing)
*/
extern const cursed_kernel CURSED_KERNEL_IDENTITY;
extern const cursed_kernel CURSED_KERNEL_EDGE1;      /* Laplacian 4-connected  */
extern const cursed_kernel CURSED_KERNEL_EDGE2;      /* Laplacian 8-connected  */
extern const cursed_kernel CURSED_KERNEL_SHARPEN;
extern const cursed_kernel CURSED_KERNEL_BOX_BLUR;
extern const cursed_kernel CURSED_KERNEL_GAUSSIAN3;  /* 3x3 Gaussian blur      */
extern const cursed_kernel CURSED_KERNEL_GAUSSIAN5;  /* 5x5 Gaussian blur      */
extern const cursed_kernel CURSED_KERNEL_UNSHARP5;   /* 5x5 unsharp masking    */
extern const cursed_kernel CURSED_KERNEL_EMBOSS;
extern const cursed_kernel CURSED_KERNEL_SOBEL_GX;   /* vertical edges         */
extern const cursed_kernel CURSED_KERNEL_SOBEL_GY;   /* horizontal edges       */

/*
    cursed_apply_kernel:
        Applies a convolution kernel to img in-place.
        R, G, B channels are processed independently; alpha is preserved.
        Boundary handling: clamp-to-edge (border pixels are replicated).
*/
void cursed_apply_kernel(cursed_img* img, const cursed_kernel* k);

#endif
