#ifndef BITDEPTH_H
#define BITDEPTH_H
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>


extern const uint8_t BITD_BYTE_BOUNDARY[16];

int resample_bitdepth(
    const uint8_t* in,
    uint8_t* out,
    const uint8_t indepth,
    const uint8_t outdepth,
    const size_t sz
);

#endif