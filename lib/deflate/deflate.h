#ifndef DEFLATE_H
#define DEFLATE_H

#include "../bithelper/bithelpers.h"

#include <stdio.h>
#include <stdlib.h>

typedef enum EBTYPE{
  NOCOMPRESS  =  0, /*  Uncompressed  */
  FIXEDCODE   =  1,  /*  Fixed codes   */
  DYNCODE     =  2,    /*  Dynamic codes */
  RES         =  3  /*  Reserved      */
}EBTYPE;

#define EOBCODE 256
#define LCODEBASE 257
#define MAX_DISTANCE 32768

extern const uint16_t DISTANCE_BASE[30];

extern const uint8_t DISTANCE_LENS[30];

int get_distance_code(short distance);

extern const uint8_t LENCODE_EBITS[29];

extern const uint8_t LENCODE_EBITS[29];

extern const uint16_t LENCODE_BASE[29];

extern const uint16_t LENCODE_LOOKUP[259];

extern const uint8_t CODELEN_ORDER[19];
extern const uint8_t CODELEN_IDX_ORDER[19];
extern const uint8_t CODELEN_EBITS[19];
extern const uint8_t CODELEN_EBITS_BASE[19];

typedef struct {
    uint16_t sym;
    uint16_t repeat; /* only used for 16,17,18 */
}rletoken;

/*return bytes written*/
int deflate(bitarray* bBuffer, uint8_t* data,size_t input_sz);

#endif
