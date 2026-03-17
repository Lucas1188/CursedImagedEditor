#ifndef HUFFMAN_H
#define HUFFMAN_H

#include "../bithelpers.h"

#ifndef HUFFMAN_ALPHABET_SZ
#define HUFFMAN_ALPHABET_SZ 286
#endif

#ifndef HUFFMAN_MAXBITS
#define HUFFMAN_MAXBITS 15
#endif


unsigned int fib[17] = {
  1, 1, 2, 3, 5, 8, 13, 21,
  34, 55, 89, 144, 233, 377,
  610, 987, 1597   /* include this for >15 bits */
};

typedef enum HUFF_NODE_TYPE{
  ROOT,
  LEAF,
  PARENT
}HUFF_KEYWORD;

typedef struct huffnode{
  unsigned short c;
  enum HUFF_NODE_TYPE ntype;
  long freq;
  struct huffnode * node0;
  struct huffnode * node1;
}huffnode;

typedef struct huffcode{
  unsigned short c;
  unsigned short codelen;
  unsigned short code;
  unsigned int freq;
}huffcode;

#define HN_FREQ_OFFSET ((size_t)&(((huffnode*)0)->freq))
#define HN_FREQ_SIZE sizeof(((huffnode*)0)->freq)
#define HN_SYM_OFFSET ((size_t)&(((huffnode*)0)->c))
#define HN_SYM_SIZE sizeof(((huffnode*)0)->c)
#define HC_SYM_OFFSET ((size_t)&(((huffcode*)0)->c))
#define HC_SYM_SIZE sizeof(((huffcode*)0)->c)
#define HC_CLEN_OFFSET ((size_t)&(((huffcode*)0)->codelen))
#define HC_CLEN_SIZE sizeof(((huffcode*)0)->codelen)
#define HC_FREQ_OFFSET ((size_t)&(((huffcode*)0)->freq))
#define HC_FREQ_SIZE sizeof(((huffcode*)0)->freq)

typedef struct huffmancoder{
  short table[HUFFMAN_ALPHABET_SZ];
  long freq[HUFFMAN_ALPHABET_SZ];
  int distinct;
  int data_count;
  struct huffnode* root;
  struct huffcode* revcode_ptr;
  struct huffcode* codetable[HUFFMAN_ALPHABET_SZ];
  struct huffcode* revcodetable[HUFFMAN_ALPHABET_SZ];
}huffmancoder;

static void init_s_huffmancode(huffmancoder* obj){
  memset(obj,0,sizeof(struct huffmancoder));
}

static huffmancoder*  global_codingtable[4];
static huffnode**     global_nodes[4];
static char*          global_distancecodes[4];

struct huffnode* make_node(short c, int freq, enum HUFF_NODE_TYPE type);

struct huffnode* build_huffman_tree(huffnode* nodes[], int size);

void free_huffman_heap(huffmancoder* hobj,size_t stack_sz);

void make_reverse_codes(huffmancoder* obj);

void limit_code_length(huffcode** code_list, size_t sz,size_t maxbits);

void make_deflate_codes(huffcode** code_list, size_t sz,int maxbits);

/*return 1 if EOB else 0*/
typedef int (*fdecode_symbol_fn)(short symbol, unsigned char* data, int* bitpos, int* bytepos);

void decode_bitstream(huffmancoder* hobj, unsigned char* data, size_t size, fdecode_symbol_fn fdsym);

void rebuild_huffman_tree(huffmancoder* hobj, short* code_lens);

void _t_make_test_string(char* b){
  int i,s,w;
  s =0;
  for(i=0;i<17;i++){
    for(w=0;w<fib[i];w++){
      b[s++] = 'a'+i;
    }
  }
}

#endif