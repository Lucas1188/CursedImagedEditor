#ifndef CURSEDIMAGE_H
#define CURSEDIMAGE_H

#include <stdint.h>

typedef struct{

    const uint8_t sz;
    const uint8_t bit_offset;
    
}scolor_fmt;

typedef struct{
    const scolor_fmt R;
    const scolor_fmt G;
    const scolor_fmt B;
    const scolor_fmt A;
    const scolor_fmt X;
}
spixel_fmt_info;

typedef struct{
    spixel_fmt_info info;
    uint64_t maskR;
    uint64_t maskG;
    uint64_t maskB;
    uint64_t maskA;
    uint8_t pixel_sz;
}spixel_fmt;

typedef uint64_t tcursed_pix;

void make_spixel_fmt(const spixel_fmt_info* pixel_info, spixel_fmt* px_fmtout);

#define CURSED_RGBA64_PXFMT()do {}while(0);

#endif