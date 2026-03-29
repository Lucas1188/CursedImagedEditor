#ifndef GREYSCALE_H
#define GREYSCALE_H

#include "../image.h"

/*
    cursed_greyscale:
        Converts a cursed_img to greyscale in-place.
        Uses the ITU-R BT.601 luminance coefficients:
            Y = 0.299*R + 0.587*G + 0.114*B
        Alpha is preserved unchanged.
*/
void cursed_greyscale(cursed_img* img);

#endif
