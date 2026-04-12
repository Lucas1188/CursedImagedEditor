#ifndef LZSS_H
#define LZSS_H

#define LBASE 3
#define WINDOW_SIZE 32768
#define LOOKAHEAD_SIZE 258
#define MIN_MATCH 3
#define LAYERS 3
#include <stdio.h>
#include <stdint.h>

typedef struct slzss_pointer{
  uint64_t position;
  uint64_t length; /*Literal*/
  uint64_t distance;
}slzss_pointer;

typedef struct slzss{
  size_t lookahead,
    search,
    window,
    pos,
    inputsize;
  uint8_t* inputbuffer;
  uint8_t* outputbuffer;
  size_t lastwritepos;
}slzss;

typedef void (*f_on_emit_lzss_ptr)(uint16_t length, uint16_t distance);
typedef void (*f_count_distinct_literal)(uint16_t symbol);

extern void emit_pointer(slzss_pointer* sp, uint64_t pos, uint16_t match_len, uint16_t dist);

static uint32_t head[1<<(24-1)];   /* head of chain for each hash */
static uint32_t prev[1<<(24-1)]; /* previous positions in the same hash */

extern uint32_t hash3(const uint8_t *buf);
extern void insert_hash(const uint8_t* window, long int pos, long int input_size);

int find_match(const uint8_t* window, size_t input_size, size_t pos, int maxlen, int *best_dist);
extern int generate_lzss_pointers(uint8_t* input,long int input_size,slzss_pointer* lzss_ptrs,int ptr_n,long int* data_cnt,
                            f_on_emit_lzss_ptr fn_olptr,
                            f_count_distinct_literal fn_literalcount
                            );
#endif
