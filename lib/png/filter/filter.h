#include <stdio.h>
#include <stdlib.h>
#include "../png.h"

uint8_t* filter_row(const uint8_t* row, uint8_t* prev_row, uint8_t* wBuffer, const size_t scanline_sz,const size_t pix_sz);

