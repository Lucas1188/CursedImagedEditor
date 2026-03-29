#ifndef PNG_CURSED_H
#define PNG_CURSED_H

#include "png.h"
#include "../cursedlib/image/image.h"

/*
    png_to_cursed:
        Converts a png_img (8 or 16-bit RGBA) into a cursed_img (16-bit RGBA64).

        8-bit input:  each channel is scaled 8->16 via left-bit replication,
                      the same method used by bitmap_to_cursed.
        16-bit input: big-endian byte pairs are reassembled directly into the
                      16-bit channel value -- no scaling, fully lossless.

        Caller frees the result with free_cursed_img().

    cursed_to_png:
        Converts a cursed_img (16-bit RGBA64) back into a png_img.

        bit_depth controls the output channel width:
            8  -- the high byte of each 16-bit channel is taken (lossy downscale)
            16 -- channels are written as big-endian uint16 pairs (lossless)

        Pass the bit_depth of the original png_img to round-trip without
        changing bit depth.

        Caller frees the result with free_png_img().
*/

cursed_img* png_to_cursed(const png_img* img);
png_img*    cursed_to_png(const cursed_img* img, uint8_t bit_depth);

#endif
