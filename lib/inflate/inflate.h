#ifndef INFLATE_H
#define INFLATE_H

#include <stdint.h>
#include <stddef.h>

/*
    inflate_zlib:
        Decompresses a ZLIB-wrapped DEFLATE stream (RFC 1950 + RFC 1951).
        Supports stored (type 0), fixed Huffman (type 1), and dynamic
        Huffman (type 2) blocks. Verifies the trailing ADLER32 checksum.

        src     - pointer to the start of the ZLIB stream
        src_sz  - number of bytes in src
        dst     - caller-allocated output buffer
        dst_sz  - in:  capacity of dst in bytes
                  out: actual number of bytes written on success

        Returns 1 on success, 0 on failure.
*/
int inflate_zlib(const uint8_t* src, size_t src_sz,
                 uint8_t* dst, size_t* dst_sz);

#endif
