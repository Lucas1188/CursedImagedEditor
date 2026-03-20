#include <stdint.h>

typedef uint32_t sDICTID;

typedef struct{
    uint8_t CMF;                        /*CMF (Compression Method and flags)
                                        This byte is divided into a 4-bit compression method and a 4-
                                        bit information field depending on the compression method.

                                        bits 0 to 3  CM     Compression method
                                        bits 4 to 7  CINFO  Compression info*/

    uint8_t FLG;                        /*FLG (FLaGs)
                                        This flag byte is divided as follows:
                                        bits 0 to 4  FCHECK  (check bits for CMF and FLG)
                                        bit  5       FDICT   (preset dictionary)
                                        bits 6 to 7  FLEVEL  (compression level)*/
} zlib_header;

const uint8_t CM_DEFLATE = 8;

const uint8_t CINFO_DEFLATE_WINDOW = 7;

/*
    FLEVEL (Compression level)
    
    These flags are available for use by specific compression
    methods.  The "deflate" method (CM = 8) sets these flags as
    follows:

    0 - compressor used fastest algorithm
    1 - compressor used fast algorithm
    2 - compressor used default algorithm
    3 - compressor used maximum compression, slowest algorithm

    The information in FLEVEL is not needed for decompression; it
    is there to indicate if recompression might be worthwhile.
*/

typedef enum{
    LFASTEST,
    LFAST,
    LDEFAULT,
    LMAX_SLOW
}EFLEVEL;