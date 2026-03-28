#include "../image.h"
#include <stdio.h>
#include <stdint.h>

typedef struct{
    uint8_t* start;
    size_t h;
    size_t w;
    size_t smpl_sz;
}cursedimg_channel;

