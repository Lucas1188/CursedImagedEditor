#ifndef HUFFMAN_H
#define HUFFMAN_H

#include "../bithelper/bithelpers.h"

#ifndef HUFFMAN_ALPHABET_SZ
#define HUFFMAN_ALPHABET_SZ 286
#endif

#ifndef HUFFMAN_MAXBITS
#define HUFFMAN_MAXBITS 15
#endif

extern unsigned int fib[17];

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

extern huffmancoder*  global_codingtable[4];
extern huffnode**     global_nodes[4];
extern char*          global_distancecodes[4];

extern void count_clcodes(unsigned short symbol);

struct huffnode* make_node(unsigned short c, int freq, enum HUFF_NODE_TYPE type);

struct huffnode* build_huffman_tree(huffnode* nodes[], int size);

void free_huffman_heap(huffmancoder* hobj,size_t stack_sz);

void make_reverse_codes(huffmancoder* obj);

void limit_code_length(huffcode** code_list, size_t sz,size_t maxbits);

void make_deflate_codes(huffcode** code_list, size_t sz,int maxbits);

void create_table(huffmancoder* hobj,huffnode** nodes,int stack_sz,int limit_len);

int dump_codelens(uint16_t* buffer,huffmancoder* hobj,const int stack_sz);

/*return 1 if EOB else 0*/
typedef int (*fdecode_symbol_fn)(short symbol, unsigned char* data, int* bitpos, int* bytepos);

void decode_bitstream(huffmancoder* hobj, unsigned char* data, size_t size, fdecode_symbol_fn fdsym);

void rebuild_huffman_tree(huffmancoder* hobj, short* code_lens);

#endif