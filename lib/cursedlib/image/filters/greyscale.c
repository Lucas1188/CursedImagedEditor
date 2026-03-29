#include "greyscale.h"

/*
    ITU-R BT.601 integer coefficients scaled to sum to 2^16 (65536):
        R: 19595  (0.299 * 65536 = 19595.264)
        G: 38470  (0.587 * 65536 = 38469.632)
        B:  7471  (0.114 * 65536 =  7471.104)
        sum = 65536

    For a 16-bit channel value in [0, 65535]:
        Y = (19595*R + 38470*G + 7471*B) >> 16

    This maps [0,65535] back to [0,65535] exactly:
        max: (65536 * 65535) >> 16 = 65535
*/

#define LUM_R 19595u
#define LUM_G 38470u
#define LUM_B  7471u

void cursed_greyscale(cursed_img* img){
    size_t      i, npix;
    tcursed_pix px;
    uint64_t    r, g, b, a, y;

    if(!img || !img->pxs) return;

    npix = img->width * img->height;

    for(i = 0; i < npix; i++){
        px = img->pxs[i];

        /* Extract each 16-bit channel from its position in the 64-bit pixel */
        r = (px >> 48) & 0xFFFFu;
        g = (px >> 32) & 0xFFFFu;
        b = (px >> 16) & 0xFFFFu;
        a =  px        & 0xFFFFu;

        /* Compute luminance using integer fixed-point arithmetic */
        y = (LUM_R * r + LUM_G * g + LUM_B * b) >> 16;

        /* Write Y into R, G, B — keep A untouched */
        img->pxs[i] = (y << 48) | (y << 32) | (y << 16) | a;
    }
}
