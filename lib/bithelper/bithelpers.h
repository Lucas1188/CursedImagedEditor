#ifndef BITHELPER_H
#define BITHELPER_H

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct bitarray{
    unsigned char *data;
    size_t   size;      /* allocated bytes */
    size_t   used;      /* bytes written */
    unsigned int bitbuf;    /* accumulator */
    unsigned long bitcount;  /* bits currently in bitbuf */
}bitarray;

static int ensure_capacity(bitarray *bw, size_t extra_bytes);

size_t packbytes_aligned(bitarray *bw, const unsigned char *data, size_t n);

size_t packbits(bitarray *bw, unsigned int value, unsigned short bits);

int bitarray_flush(bitarray *bw);

unsigned short reverse_bits(unsigned short v, int bits);
unsigned int reverse_bits_int(unsigned int v, int bits);


int read_bit(const unsigned char *data, int *bitpos, int *bytepos);

unsigned read_bits(const unsigned char *data, int *bitpos, int *bytepos, int n);

#endif
