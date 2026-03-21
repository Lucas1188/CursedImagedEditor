#ifndef PNGSAMPLE_H
#define PNGSAMPLE_H

#include <stdint.h>

typedef union {
    uint8_t indexed;
    uint8_t grayscale8;
    uint8_t grayscale16 [2];
    uint8_t grayscale_alpha16[2];
    uint8_t grayscale_alpha32[4];
    uint8_t truecolor24[3];
    uint8_t truecolor48[6];
    uint8_t truecolor_alpha32[4];
    uint8_t truecolor_alpha64[8];
}png_pix;


#endif