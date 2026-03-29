#ifndef BITMAP_CURSED_H
#define BITMAP_CURSED_H

#include "bitmap.h"
#include "../cursedlib/image/image.h"

/*
    bitmap_to_cursed:
        Converts a bitmap (8-bit RGBA) into a cursed_img (16-bit RGBA64).
        Caller is responsible for freeing the result with free_cursed_img().

    cursed_to_bitmap:
        Converts a cursed_img (16-bit RGBA64) back into a bitmap (8-bit RGBA).
        Caller is responsible for freeing the result with free_bitmap().
*/

cursed_img* bitmap_to_cursed(const bitmap* bmp);
bitmap*     cursed_to_bitmap(const cursed_img* img);

#endif
