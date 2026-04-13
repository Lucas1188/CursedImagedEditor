#ifndef B64E_H
#define B64E_H

#include <stddef.h> /* Required for size_t */

/* * Return codes used across the Base64 module.
 * Bitwise formatting allows checking multiple error states if needed.
 */
typedef enum RCODES {
    OK               = 1,
    ERROR            = 2,
    INPUT_ERROR      = 4,
    INVALID_FILENAME = 8,
    FILE_ERROR       = 16,
    INSUF_MEM        = 32,
    INVALID_B64      = 64,
    BAD_WRITE        = 128
} RCODES;

/* --- Memory & File I/O Helpers --- */
/* Note: Remove 'static' from these in your .c file to use them! */

int readstring_to_mem(const char* str, unsigned char** buffer, size_t* size_out);
int readfile_to_mem(const char* filename, unsigned char** buffer, size_t* size_out);
int writefile_from_mem(const char* filename, const unsigned char* buffer, size_t size);

/* --- Base64 Size Calculators --- */

/* * Calculates the required buffer size for encoding.
 * fsize: Input file/buffer size in bytes.
 * bsize_out: Output B64 buffer size (including null terminator).
 * remainder_out: Leftover bytes that need padding (fsize % 3).
 * blocks_out: Total 3-byte blocks to process.
 */
int _getb64size(long fsize, long* bsize_out, long* remainder_out, long* blocks_out);

/* * Calculates the required buffer size for decoding.
 * b64: The Base64 string buffer.
 * fsize: The size of the Base64 string (including null terminator).
 * wsize_out: Output binary buffer size.
 * blocks_out: Total 4-byte blocks to process.
 * pad_out: Number of '=' padding characters at the end (0, 1, or 2).
 */
int _getb64decodesize(const unsigned char* b64, long fsize, long* wsize_out, long* blocks_out, long* pad_out);

/* * Encodes raw binary data into a Base64 string.
 * in: Raw binary buffer.
 * in_size: Number of bytes to encode.
 * out: Pre-allocated buffer for the Base64 string.
 * out_size: Pointer to store the final length of the Base64 string.
 */
int b64_encode(const unsigned char* in, size_t in_size, unsigned char* out, size_t* out_size);

/* * Decodes a Base64 string back into raw binary data.
 * in: Base64 string buffer.
 * in_size: Length of the Base64 string (excluding null terminator).
 * out: Pre-allocated buffer for the raw binary data.
 * out_size: Pointer to store the final length of the decoded binary data.
 */
int b64_decode(const unsigned char* in, size_t in_size, unsigned char* out, size_t* out_size);
#endif /* B64E_H */