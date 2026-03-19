#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "huffman.h"
#include "../deflate/deflate.h"
#include "../sort.h"
#include "../lzss/lzss.h"
#include "../cursedhelpers.h"

#ifndef GLOBAL_HUFFMAN_TABLES
#define GLOBAL_HUFFMAN_TABLES

huffmancoder*  global_codingtable[4];
huffnode**     global_nodes[4];
char*          global_distancecodes[4];
#endif
unsigned int fib[17] = {
  1, 1, 2, 3, 5, 8, 13, 21,
  34, 55, 89, 144, 233, 377,
  610, 987, 1597   /* include this for >15 bits */
};

struct huffnode* make_p_node(unsigned short c, int freq,struct huffnode* l,struct huffnode* r, enum HUFF_NODE_TYPE type){
  huffnode * hn = make_node(c,freq,type);
  hn->node0=l;
  hn->node1=r;
  return hn;
}

struct huffnode* make_node(unsigned short c, int freq, enum HUFF_NODE_TYPE type){
  huffnode * hn;
  hn = malloc(sizeof(huffnode));
  hn->c = c;
  hn->freq = freq;
  hn->ntype = type;
  return hn;
}

struct huffcode* make_code(unsigned short c,unsigned short len, unsigned short code, unsigned int frequency){
  huffcode* hc;
  hc = malloc(sizeof(huffcode));
  hc->c = c;
  hc->codelen = len;
  hc->code = code;
  hc->freq = frequency;
  return hc;
}
void free_huffnode(huffnode* node){
  if(!node) return;
  if(node->ntype!=LEAF){
    free_huffnode(node->node0);
    free_huffnode(node->node1);
  }
  free(node);
}
void free_huffman_heap(huffmancoder* hobj,size_t stack_sz){
  int i;
  if(! hobj) return;
  
  
  if(hobj->codetable){

    for(i=0;i<stack_sz;i++){
      if(hobj->codetable[i]){
        free(hobj->codetable[i]);
      }
    }
  }
  if(hobj->revcode_ptr){
    free(hobj->revcode_ptr);
  }
  if(hobj->root){
    free_huffnode(hobj->root);
  }
}

static void print_nodes(huffnode* cnodes[],size_t j){
  int i;
  for(i=0;i<j;i++){
    if(cnodes[i]==NULL) {
      LOG_I("\nSize was wrong, check your stack, broke on idx %d\n",i);
      return;
    }
    if(cnodes[i]->ntype==PARENT){
      LOG_I("[%d %d]:%ld ",cnodes[i]->node0->c,cnodes[i]->node1->c,cnodes[i]->freq);
    }else if(cnodes[i]->ntype==ROOT){
      LOG_I("[ROOT]:%ld ",cnodes[i]->freq);
    }
    else{
      LOG_I("%d:%ld ",cnodes[i]->c,cnodes[i]->freq);
    }
  }
  LOG_I("\n");
}

static void print_codes(huffcode* codes[],size_t sz){
  int i,n;
  for(i=0;i<sz;i++){
    if(codes[i]!=NULL){
      n = codes[i]->code;
      LOG_I("[%d\t|%d\t|0b ",codes[i]->c,codes[i]->codelen);
      PRINT_BINARY(n,codes[i]->codelen);
      LOG_I("]\n");
    }else{
      LOG_I("NULL in stack [%d]\n",i);
    }
  }
}

huffnode* build_huffman_tree(huffnode* nodes[], int size){
    int n = size;
    huffnode *l, *r, *p;
    huffnode* dummy;

    if(n == 1){
      huffnode *only = nodes[0];
      LOG_I("Makesingle %ld %d\n", only->freq, only->c);

      dummy = make_p_node(0, 0+only->freq, nodes[0], NULL, ROOT);
      dummy->c = (only->c == 0 ? 1 : 0); /* dummy value */
      
      nodes[0] = dummy;
      return nodes[0];
    }

    radix_sort_xb((char**)nodes, HN_FREQ_OFFSET, sizeof(long)*8, n, 0);

    while(n > 1){
      int i;
      /*
      print_nodes(nodes, n);
      */
      l = nodes[0];
      r = nodes[1];
      p = make_p_node(0, l->freq + r->freq, l, r, PARENT);

      for(i = 2; i < n; i++) nodes[i-2] = nodes[i];
      n -= 2;

      for(i = n-1; i >= 0 && nodes[i]->freq > p->freq; i--){
          nodes[i+1] = nodes[i];
      }
      nodes[i+1] = p;
      n++;
    }

    nodes[0]->ntype = ROOT;
    return nodes[0];
}

void make_reverse_codes(huffmancoder* obj){
  int i;
  int sym;
  huffcode* tbl = malloc(sizeof(huffcode)*obj->distinct);
  obj->revcode_ptr = tbl;
  for(i=0;i<obj->distinct;i++){
    sym = obj->codetable[i]->c;
    obj->revcodetable[sym] = &tbl[i];
    obj->revcodetable[sym]->c = sym;
    obj->revcodetable[sym]->codelen = obj->codetable[i]->codelen;
    obj->revcodetable[sym]->code = reverse_bits(obj->codetable[i]->code, obj->codetable[i]->codelen);
  }
}

void rebuild_huffman_tree(huffmancoder* hobj, short* code_lens)
{
  int i,b;
  int bl_count[HUFFMAN_MAXBITS+1] = {0};
  int next_code[HUFFMAN_MAXBITS+1] = {0};
  int code = 0, len, c, bit;

  /* allocate root */
  hobj->root = malloc(sizeof(huffnode));
  hobj->root->ntype = PARENT;
  hobj->root->node0 = NULL;
  hobj->root->node1 = NULL;

  /* count codes per bit length */
  for(i=0;i<HUFFMAN_ALPHABET_SZ;i++){
    if(code_lens[i] > 0)
      bl_count[code_lens[i]]++;
  }

  /* compute canonical starting codes */
  for(b=1;b<=HUFFMAN_MAXBITS;b++){
    code = (code + bl_count[b-1]) << 1;
    next_code[b] = code;
  }

  /* assign codes and build tree */
  for(i=0;i<HUFFMAN_ALPHABET_SZ;i++){

    len = code_lens[i];
    if(len == 0) continue;

    c = next_code[len]++;

    huffnode *cur = hobj->root;

    for(b=len-1;b>=0;b--){

      bit = (c>>b)&1;

      if(bit==0){
        if(!cur->node0){
          cur->node0 = malloc(sizeof(huffnode));
          cur->node0->node0 = NULL;
          cur->node0->node1 = NULL;
          cur->node0->ntype = PARENT;
        }
        cur = cur->node0;
      }
      else{
        if(!cur->node1){
          cur->node1 = malloc(sizeof(huffnode));
          cur->node1->node0 = NULL;
          cur->node1->node1 = NULL;
          cur->node1->ntype = PARENT;
        }
        cur = cur->node1;
      }
    }

    cur->ntype = LEAF;
    cur->c = i;

    /*LOG_I("T|%c|%d|0b ", i, len);
    PRINT_BINARY(c, len);*/
  }
}

void generate_codes(huffnode* cnode,huffcode** code_list,unsigned short clen,unsigned short ccode){
  unsigned short n0code, n1code;
  huffcode* hc;

  if(cnode==NULL){
    return;
  }
  if(cnode->ntype==LEAF){
    code_list[cnode->c] = make_code(cnode->c,clen,ccode,cnode->freq);
    return;
  }
  n0code = (ccode<<1)|0;
  n1code = (ccode<<1)|1;
  ++clen;
  generate_codes(cnode->node0,code_list,clen,n0code);
  generate_codes(cnode->node1,code_list,clen,n1code); 
}

#define MAX_CODELEN 64  /* safe upper bound*/

void limit_code_length(huffcode** code_list, size_t n, size_t maxbits)
{
    size_t i;

    int bl_count[MAX_CODELEN] = {0};
    for (i = 0; i < n; i++) {
      int len = code_list[i]->codelen;
      if (len >= MAX_CODELEN) len = MAX_CODELEN - 1;
      bl_count[len]++;
    }

    int overflow = 0;
    for (i = maxbits + 1; i < MAX_CODELEN; i++) {
      overflow += bl_count[i];
      
    }

    bl_count[maxbits] += overflow;

    while (overflow > 0) {
      int bits = maxbits - 1;

      /* find largest available shorter length */
      while (bits > 0 && bl_count[bits] == 0)
          bits--;

      /* split one node */
      bl_count[bits]--;
      bl_count[bits + 1] += 2;

      overflow--;
    }

    for (i = maxbits + 1; i < MAX_CODELEN; i++) {
      bl_count[i] = 0;
    }

    size_t idx = 0;
    int len,count;

    for (len = 1; len <= (int)maxbits; len++) {
        count = bl_count[len];

        while (count > 0 && idx < n) {
            code_list[idx++]->codelen = len;
            count--;
        }
    }

    if (idx != n) {
        LOG_E("Mismatch after limiting: idx=%ld, n=%ld\n", idx, n);
    }

}




void decode_bitstream(huffmancoder* hobj, unsigned char* data, size_t size, fdecode_symbol_fn fdsym)
{
  int bytepos;
  int bitpos,bit;
  int sym;
  huffnode *root;
  huffnode *cur;
  root = hobj->root;
  cur = root;
  bitpos = 0;
  bytepos = 0;
  while (bytepos < size) {
    bit = read_bit(data,&bitpos,&bytepos);
    if(bit == 0){
      cur = cur->node0;
    }
    else{
      cur = cur->node1;
    }
    if(!cur){
      LOG_I("Decode error\n");
      return;
    }

    /* reached symbol */
    if(cur->ntype == LEAF){
      sym = cur->c;
      if(fdsym(sym,data,&bitpos,&bytepos)){
        LOG_I("\nEOB\n");
        return;
      }
      cur = root;
    }
  }
}

int parse_symbol(short symbol, unsigned char* data, int* bitpos, int* bytepos){
  if(symbol<EOBCODE){ 
    putchar(symbol);
    return 0;
  }
  if(symbol==EOBCODE){
    return 1;
  }
}

void count_clcodes(unsigned short symbol){
  huffmancoder* clcodes= global_codingtable[2];
  long* r = &clcodes->freq[symbol];
  if(*r==0){
    clcodes->table[symbol]=clcodes->distinct;
    global_nodes[2][clcodes->distinct] = make_node(symbol,0,LEAF);
    clcodes->distinct++;
  }
  (*r)++;    
  global_nodes[2][clcodes->table[symbol]]->freq++;
}

void create_table(huffmancoder* hobj,huffnode** nodes,int stack_sz,int limit_len){
  int i;
  LOG_I("\nCreating Table with distinct: %d\n\n",hobj->distinct);
  if(hobj->distinct == 0){
    LOG_I("No symbols to encode\n");
    return;
  }
  hobj->root = build_huffman_tree(nodes,hobj->distinct);
  
  LOG_I("Gen Codes:");
  
  generate_codes(hobj->root,hobj->codetable,0,0);

  LOG_I(" Ok\n\nSort (Length|Symbol) ASC:\n");

  insertion_sort_xxB((char**)hobj->codetable, HC_CLEN_OFFSET, HC_SYM_OFFSET, HC_CLEN_SIZE, HC_SYM_SIZE, HUFFMAN_ALPHABET_SZ, 0);
  
  print_codes(hobj->codetable,hobj->distinct);
  
  LOG_I(" Ok\n\nLimit Length:\n");
  limit_code_length(hobj->codetable,hobj->distinct,limit_len);
  LOG_I(" Ok\n\nResorting:\n");

  insertion_sort_xxB((char**)hobj->codetable, HC_CLEN_OFFSET, HC_SYM_OFFSET, HC_CLEN_SIZE, HC_SYM_SIZE, HUFFMAN_ALPHABET_SZ, 0);
  
  print_codes(hobj->codetable,hobj->distinct);
  LOG_I(" Ok\n\nMake deflate codes:\n\n");
  
  make_deflate_codes(hobj->codetable,hobj->distinct,limit_len);
  radix_sort_xb((char**)hobj->codetable,HC_SYM_OFFSET,HC_SYM_SIZE*8,hobj->distinct,0);
  print_codes(hobj->codetable,hobj->distinct);
  make_reverse_codes(hobj);
  LOG_I("\nDone Creating Table\n");
}

int dump_codelens(unsigned short* buffer,const huffmancoder* hobj,const int stack_sz){
  int i,lastidx = 0;
  for(i=0;i<stack_sz;i++){
    if(hobj->revcodetable[i]){
      buffer[i] = hobj->revcodetable[i]->codelen;
      lastidx = i;
    }else{
      buffer[i] = 0;
    }
  }
  return lastidx+1;
}