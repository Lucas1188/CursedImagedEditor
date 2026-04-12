#ifndef BITHELPER_H
#define BITHELPER_H

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct bitarray{
    uint8_t *data;
    size_t   size;      /* allocated bytes */
    size_t   used;      /* bytes written */
    uint32_t bitbuf;    /* accumulator */
    uint64_t bitcount;  /* bits currently in bitbuf */
}bitarray;

int ensure_capacity(bitarray *bw, size_t extra_bytes);

size_t packbytes_aligned(bitarray *bw, const uint8_t *data, size_t n);

size_t packbits(bitarray *bw, uint32_t value, uint16_t bits);

int bitarray_flush(bitarray *bw);

uint16_t reverse_bits(uint16_t v, int bits);
uint32_t reverse_bits_int(uint32_t v, int bits);


int read_bit(const uint8_t *data, int *bitpos, int *bytepos);

uint32_t read_bits(const uint8_t *data, int *bitpos, int *bytepos, int n);

#endif
