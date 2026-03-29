#ifndef CHANNEL_H
#define CHANNEL_H

#include "../image.h"
#include <stddef.h>
#include <stdint.h>

/*
    A cursedimg_channel is a lightweight view into one channel (R, G, B, or A)
    of a cursed_img's pixel buffer.  No data is copied — start points directly
    into img->pxs.  Use channel_of() to create one.

    To read/write a channel value for pixel (x, y):
        uint64_t v = channel_get(&ch, x, y);
        channel_set(&ch, x, y, new_val);
*/
typedef struct{
    uint8_t* start;    /* raw pixel buffer (same memory as cursed_img->pxs) */
    size_t   h;
    size_t   w;
    uint64_t mask;     /* bit mask for this channel within the packed pixel   */
    uint8_t  offset;   /* right-shift to isolate the channel value            */
    uint8_t  smpl_sz;  /* channel bit depth (e.g. 16 for RGBA64)             */
} cursedimg_channel;

/* Build a channel view for 'ch' = 'R', 'G', 'B', or 'A'. */
cursedimg_channel channel_of(const cursed_img* img, char ch);

/* Get the channel value for pixel at (x, y). */
uint64_t          channel_get(const cursedimg_channel* ch, size_t x, size_t y);

/* Set the channel value for pixel at (x, y). */
void              channel_set(cursedimg_channel* ch, size_t x, size_t y, uint64_t value);

#endif
