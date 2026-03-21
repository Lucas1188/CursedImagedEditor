
#include <stdint.h>

/*
    +---------------+               
    |   Prop Bits   |               
    +---------------+               

    For example, the hypothetical chunk type name "bLOb" has the
    bLOb  <-- 32 bit chunk type code represented in text form
    ||||
    |||+- Safe-to-copy bit is 1 (lower case letter; bit 5 is 1)
    ||+-- Reserved bit is 0     (upper case letter; bit 5 is 0)
    |+--- Private bit is 0      (upper case letter; bit 5 is 0)
    +---- Ancillary bit is 1    (lower case letter; bit 5 is 1)
*/
typedef enum {
    SAFETOCOPY  ,
    RESERVED    ,
    PRIVATE     ,
    ANCILLARY   
}PROP_BIT5_BYTEPOS;

/*
    +-------------------+ 
    |   FILTER TYPES    |
    +-------------------+

    0       None
    1       Sub
    2       Up
    3       Average
    4       Paeth
*/

typedef enum{
    
    FILTER_NONE,
    FILTER_SUB,
    FILTER_UP,
    FILTER_AVERAGE,
    FILTER_PAETH

}FILTER_TYPES;

/*
    +---------------+
    |   BIT DEPTHS  |
    +---------------+
*/

typedef enum{
    BDEPTH_1 = 1,
    BDEPTH_2 = 2,
    BDEPTH_4 = 4,
    BDEPTH_8 = 8,
    BDEPTH_16 = 16
}BIT_DEPTHS;

/*
    +---------------+
    |   COLOR TYPE  |
    +---------------+
*/
/*
    Color    Allowed    Interpretation
    Type    Bit Depths

    0       1,2,4,8,16  Each pixel is a grayscale sample.

    2       8,16        Each pixel is an R,G,B triple.

    3       1,2,4,8     Each pixel is a palette index;
                        a PLTE chunk must appear.

    4       8,16        Each pixel is a grayscale sample,
                        followed by an alpha sample.

    6       8,16        Each pixel is an R,G,B triple,
                        followed by an alpha sample.
    
    The sample depth is the same as the bit depth except in the
    case of color type 3, in which the sample depth is always 8
    bits.
*/

typedef enum{

    CT_GRAYSCALE    = 0,
    CT_RGB          = 2,
    CT_PALETE       = 3,
    CT_GRAY_ALPHA   = 4,
    CT_RGB_ALPHA    = 6

}COLOR_TYPE;

/*                          
    +-----------------------+
    |   Interlace Methods   |
    +-----------------------+
*/

typedef enum{
    INTL_NONE,
    INTL_ADAM7
}INTERLACE_TYPES;

/*
    Critical chunks (must appear in this order, except PLTE
        is optional):

    Name  Multiple  Ordering constraints
            OK?

    IHDR    No      Must be first
    PLTE    No      Before IDAT
    IDAT    Yes     Multiple IDATs must be consecutive
    IEND    No      Must be last

    Ancillary chunks (need not appear in this order):

    Name  Multiple  Ordering constraints
            OK?

    cHRM    No      Before PLTE and IDAT
    gAMA    No      Before PLTE and IDAT
    sBIT    No      Before PLTE and IDAT
    bKGD    No      After PLTE; before IDAT
    hIST    No      After PLTE; before IDAT
    tRNS    No      After PLTE; before IDAT
    pHYs    No      Before IDAT
    tIME    No      None
    tEXt    Yes     None
    zTXt    Yes     None
*/

/*
    Standard keywords for tEXt and zTXt chunks:

    Title            Short (one line) title or caption for image
    Author           Name of image's creator
    Description      Description of image (possibly long)
    Copyright        Copyright notice
    Creation Time    Time of original image creation
    Software         Software used to create the image
    Disclaimer       Legal disclaimer
    Warning          Warning of nature of content
    Source           Device used to create the image
    Comment          Miscellaneous comment; conversion from
                    GIF comment
*/

/*
    +-----------+
    |   CHUNK   |
    +-----------+
*/

typedef struct{
    
    uint32_t LENGTH;        /*Although encoders and decoders should treat the length as unsigned, 
                            its value must not exceed (2^31)-1 bytes.*/

    uint32_t CHUNK_TYPE;    /*A 4-byte chunk type code.  For convenience in description and
                            in examining PNG files, type codes are restricted to consist of
                            uppercase and lowercase ASCII letters (A-Z and a-z, or 65-90
                            and 97-122 decimal).  However, encoders and decoders must treat
                            the codes as fixed binary values, not character strings.  For
                            example, it would not be correct to represent the type code
                            IDAT by the EBCDIC equivalents of those letters. */

    uint8_t* CHUNK_DATA;    /*The data bytes appropriate to the chunk type, if any.  This
                            field can be of zero length.*/

    uint32_t CRC;           /*A 4-byte CRC (Cyclic Redundancy Check) calculated on the
                            preceding bytes in the chunk, including the chunk type code and
                            chunk data fields, but not including the length field. The CRC
                            is always present, even for chunks containing no data.*/
}png_chunk;

typedef struct{
    uint32_t width;                     /*  Zero is an invalid value. The maximum for each
                                            is (2^31)-1 in order to accommodate languages that have
                                            difficulty with unsigned 4-byte values.
                                        */

    uint32_t height;                    /*  See Above   
                                        */

    uint8_t bit_depth;                  /*  Bit depth is a single-byte integer giving the number of bits
                                            per sample or per palette index (not per pixel).  Valid values
                                            are 1, 2, 4, 8, and 16, although not all values are allowed for
                                            all color types.
                                        */

    uint8_t color_type;                 /*  Color type is a single-byte integer that describes the
                                            interpretation of the image data.  Color type codes represent
                                            sums of the following values: 1 (palette used), 2 (color used),
                                            and 4 (alpha channel used). Valid values are 0, 2, 3, 4, and 6.
                                        */

    uint8_t compression_method;         /*  Compression method is a single-byte integer that indicates the
                                            method used to compress the image data.  At present, only
                                            compression method 0 (deflate/inflate compression with a 32K
                                            sliding window) is defined. 
                                        */

    FILTER_TYPES filter_method;         /*  Byte operation
                                        */

    INTERLACE_TYPES interlace_method;   /*  None or Adam7
                                        */
}ihdr_chunk;

/*

    +---------------+
    |   PLTE CHUNK  |
    +---------------+

    The PLTE chunk contains from 1 to 256 palette entries, each a
    three-byte series of the form:

    Red:   1 byte (0 = black, 255 = red)
    Green: 1 byte (0 = black, 255 = green)
    Blue:  1 byte (0 = black, 255 = blue)

    The number of entries is determined from the chunk length.  A
    chunk length not divisible by 3 is an error.

    This chunk must appear for color type 3, and can appear for
    color types 2 and 6; it must not appear for color types 0 and
    4. If this chunk does appear, it must precede the first IDAT
    chunk.  There must not be more than one PLTE chunk.

    For color type 3 (indexed color), the PLTE chunk is required.
    The first entry in PLTE is referenced by pixel value 0, the
    second by pixel value 1, etc.  The number of palette entries
    must not exceed the range that can be represented in the image
    bit depth (for example, 2^4 = 16 for a bit depth of 4).  It is
    permissible to have fewer entries than the bit depth would
    allow.  In that case, any out-of-range pixel value found in the
    image data is an error.

    For color types 2 and 6 (truecolor and truecolor with alpha),
    the PLTE chunk is optional.  If present, it provides a
    suggested set of from 1 to 256 colors to which the truecolor
    image can be quantized if the viewer cannot display truecolor
    directly.  If PLTE is not present, such a viewer will need to
    select colors on its own, but it is often preferable for this
*/

/*
    +---------------+
    |   IDAT CHUNK  |
    +---------------+

    The IDAT chunk contains the actual image data.  To create this
    data:

        * Begin with image scanlines represented as described in
        Image layout (Section 2.3); the layout and total size of
        this raw data are determined by the fields of IHDR.
        * Filter the image data according to the filtering method
        specified by the IHDR chunk.  (Note that with filter
        method 0, the only one currently defined, this implies
        prepending a filter type byte to each scanline.)
        * Compress the filtered data using the compression method
        specified by the IHDR chunk.

    The IDAT chunk contains the output datastream of the
    compression algorithm.

    To read the image data, reverse this process.

    There can be multiple IDAT chunks; if so, they must appear
    consecutively with no other intervening chunks.  The compressed
    datastream is then the concatenation of the contents of all the
    IDAT chunks.  The encoder can divide the compressed datastream
    into IDAT chunks however it wishes.  (Multiple IDAT chunks are
    allowed so that encoders can work in a fixed amount of memory;
    typically the chunk size will correspond to the encoder's
    buffer size.) It is important to emphasize that IDAT chunk
    boundaries have no semantic significance and can occur at any
    point in the compressed datastream.  A PNG file in which each
    IDAT chunk contains only one data byte is legal, though
    remarkably wasteful of space.  (For that matter, zero-length
    IDAT chunks are legal, though even more wasteful.)
*/

typedef struct{
    
    uint64_t size;                          /*Not to be written*/

    uint8_t* data;
    
}idat_chunk;


typedef struct{
    
    const uint8_t MAGIC [8] = {
        137, 80, 78, 71, 13, 10, 26, 10
    };

    

}png_format;