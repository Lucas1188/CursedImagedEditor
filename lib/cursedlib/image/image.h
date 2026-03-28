#ifndef CURSEDIMAGE_H
#define CURSEDIMAGE_H

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

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
    uint8_t smpl_sz;
}spixel_fmt;

typedef uint64_t tcursed_pix;

typedef struct{
    size_t height;
    size_t width;
    spixel_fmt px_fmt;
    tcursed_pix* pxs;
}cursed_img;

#define SCOLOR_FMT(sz_, offset_)(scolor_fmt) {.sz=(sz_),.bit_offset= (offset_) }

#define CURSED_RGBA64_PXFMT {                                                                               \
    .info=(spixel_fmt_info){SCOLOR_FMT(16,48), SCOLOR_FMT(16,32),SCOLOR_FMT(16,16),SCOLOR_FMT(16,0),SCOLOR_FMT(0,0)},   \
    .maskR = 0xFFFF000000000000,                                                                                        \
    .maskG = 0x0000FFFF00000000,                                                                                        \
    .maskB = 0x00000000FFFF0000,                                                                                        \
    .maskA = 0x000000000000FFFF,                                                                                        \
    .smpl_sz = 16                                                                                                       \
}                                                                        


#define GEN_CURSED_IMG(w,h) \
    ((cursed_img){ \
        .height = (h), \
        .width = (w), \
        .px_fmt = CURSED_RGBA64_PXFMT, \
        .pxs = malloc((w)*(h)*sizeof(tcursed_pix)) \
    })

#define RELEASE_CURSED_IMG(img) do { \
    free((img).pxs); \
    (img).pxs = NULL; \
} while(0)

#endif