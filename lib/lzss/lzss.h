#ifndef LZSS_H
#define LZSS_H

#define LBASE 3
#define WINDOW_SIZE 32768
#define LOOKAHEAD_SIZE 258
#define MIN_MATCH 3
#define LAYERS 3
#include <stdio.h>

typedef struct slzss_pointer{
  unsigned long position;
  unsigned long length; /*Literal*/
  unsigned long distance;
}slzss_pointer;

typedef struct slzss{
  size_t lookahead,
    search,
    window,
    pos,
    inputsize;
  char* inputbuffer;
  char* outputbuffer;
  size_t lastwritepos;
}slzss;

typedef void (*f_on_emit_lzss_ptr)(short length, short distance);
typedef void (*f_count_distinct_literal)(short symbol);

extern void emit_pointer(slzss_pointer* sp, unsigned long pos, unsigned short match_len, unsigned short dist);

static unsigned int head[1<<24-1];   /* head of chain for each hash */
static unsigned int prev[1<<24-1]; /* previous positions in the same hash */

extern unsigned int hash3(const unsigned char *buf);
extern void insert_hash(const unsigned char* window, int pos);

extern int find_match(const char* window, int pos, int maxlen, int *best_dist);
/*return ptr count*/
extern int generate_lzss_pointers(char* input,int input_size,slzss_pointer* lzss_ptrs,int ptr_n,int* data_cnt,
                            f_on_emit_lzss_ptr fn_olptr,
                            f_count_distinct_literal fn_literalcount
                            );
#endif
