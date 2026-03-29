#include "channel.h"

cursedimg_channel channel_of(const cursed_img* img, char ch){
    cursedimg_channel out;
    const spixel_fmt* fmt = &img->px_fmt;
    const scolor_fmt* sc;
    uint64_t          mask;

    switch(ch){
        case 'R': sc = &fmt->info.R; mask = fmt->maskR; break;
        case 'G': sc = &fmt->info.G; mask = fmt->maskG; break;
        case 'B': sc = &fmt->info.B; mask = fmt->maskB; break;
        case 'A': sc = &fmt->info.A; mask = fmt->maskA; break;
        default:  sc = &fmt->info.R; mask = fmt->maskR; break;
    }

    out.start   = (uint8_t*)img->pxs;
    out.h       = img->height;
    out.w       = img->width;
    out.mask    = mask;
    out.offset  = sc->bit_offset;
    out.smpl_sz = sc->sz;
    return out;
}

uint64_t channel_get(const cursedimg_channel* ch, size_t x, size_t y){
    tcursed_pix pix = ((const tcursed_pix*)ch->start)[y * ch->w + x];
    return (pix & ch->mask) >> ch->offset;
}

void channel_set(cursedimg_channel* ch, size_t x, size_t y, uint64_t value){
    tcursed_pix* pix = &((tcursed_pix*)ch->start)[y * ch->w + x];
    *pix = (*pix & ~ch->mask) | (((tcursed_pix)value << ch->offset) & ch->mask);
}
