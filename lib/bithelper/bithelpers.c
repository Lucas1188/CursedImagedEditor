#include "../cursedhelpers.h"
#include "bithelpers.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>


static int ensure_capacity(bitarray *bw, size_t extra_bytes)
{
  size_t newsize;
  unsigned char *tmp;
  if (bw->used + extra_bytes <= bw->size){
    return 1;
  }
  newsize = bw->size ? bw->size * 2 : 64;
  while (newsize < bw->used + extra_bytes){
    newsize *= 2;
  }
  tmp = realloc(bw->data, newsize);
  if (!tmp){ 
    return 0;
  }
  bw->data = tmp;
  bw->size = newsize;
  return 1;
}
size_t packbytes_aligned(bitarray *bw, const unsigned char *data, size_t n){
  if(!bw){
    return 0;
  }
  if(bw->bitcount!=0){
    LOG_E("Bit buffer not empty, cannot pack bytes\n");
    return 0;
  }
  if (!ensure_capacity(bw, n)){
    LOG_E("Cannot make space for bitarray\n");
    return 0;
  }
  memcpy(bw->data + bw->used, data, n);
  bw->used += n;
  return n;
}

size_t packbits(bitarray *bw, unsigned int value, unsigned short bits)
{
  if (!bw || bits > 16){
    return 0;
  }
  bw->bitbuf |= value << bw->bitcount;
  bw->bitcount += bits;

  /* Flush full bytes */
  while (bw->bitcount >= 8) {
    if (!ensure_capacity(bw, 1)){
      return 0;
    }
    bw->data[bw->used++] = bw->bitbuf & 0xFF;
    bw->bitbuf >>= 8;
    bw->bitcount -= 8;
  }

  return bits;
}

int bitarray_flush(bitarray *bw)
{
  if (bw->bitcount > 0) {
    if (!ensure_capacity(bw, 1)){
      return 0;
    }
    bw->data[bw->used++] = bw->bitbuf & 0xFF;
    bw->bitbuf = 0;
    bw->bitcount = 0;
  }
  return 1;
}

unsigned short reverse_bits(unsigned short v, int bits)
{
    v = ((v & 0x5555) << 1) | ((v >> 1) & 0x5555);
    v = ((v & 0x3333) << 2) | ((v >> 2) & 0x3333);
    v = ((v & 0x0F0F) << 4) | ((v >> 4) & 0x0F0F);
    v = (v << 8) | (v >> 8);
    return v >> (16 - bits);
}

unsigned int reverse_bits_int(unsigned int v, int bits)
{
    v = ((v & 0x55555555) << 1) | ((v >> 1) & 0x55555555);
    v = ((v & 0x33333333) << 2) | ((v >> 2) & 0x33333333);
    v = ((v & 0x0F0F0F0F) << 4) | ((v >> 4) & 0x0F0F0F0F);
    v = ((v & 0x00FF00FF) << 8) | ((v >> 8) & 0x00FF00FF);
    v = (v << 16) | (v >> 16);
    return v >> (32 - bits);
}
int read_bit(const unsigned char *data, int *bitpos, int *bytepos)
{
  int bit = (data[*bytepos] >> *bitpos) & 1;
  (*bitpos)++;
  if (*bitpos == 8) {
      *bitpos = 0;
      (*bytepos)++;
  }
  return bit;
}

unsigned read_bits(const unsigned char *data, int *bitpos, int *bytepos, int n)
{
  unsigned val = 0;
  int i;
  for(i = 0; i < n; i++){
      val |= read_bit(data, bitpos, bytepos) << i;
  }
  return val;
}
