#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "huffman.h"
#include "../deflate/deflate.h"
#include "../sort.h"
#include "../lzss/LZSS.h"

struct huffnode* make_p_node(short c, int freq,struct huffnode* l,struct huffnode* r, enum HUFF_NODE_TYPE type){
  huffnode * hn = make_node(c,freq,type);
  hn->node0=l;
  hn->node1=r;
  return hn;
}

struct huffnode* make_node(short c, int freq, enum HUFF_NODE_TYPE type){
  huffnode * hn;
  hn = malloc(sizeof(huffnode));
  hn->c = c;
  hn->freq = freq;
  hn->ntype = type;
  return hn;
}

struct huffcode* make_code(short c,unsigned char len, unsigned short code, unsigned int frequency){
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
  if(hobj->revcode_ptr){/*
    for(i=0;i<stack_sz;i++){
      if(hobj->revcodetable[i]){
        free(hobj->revcodetable[i]);
      }
    }
    */
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
      printf("\nSize was wrong, check your stack, broke on idx %d\n",i);
      return;
    }
    if(cnodes[i]->ntype==PARENT){
      printf("[%d %d]:%ld ",cnodes[i]->node0->c,cnodes[i]->node1->c,cnodes[i]->freq);
    }else if(cnodes[i]->ntype==ROOT){
      printf("[ROOT]:%ld ",cnodes[i]->freq);
    }
    else{
      printf("%d:%ld ",cnodes[i]->c,cnodes[i]->freq);
    }
  }
  printf("\n");
}

static void print_binary(int code,int len){
  for (len = len - 1; len >= 0; len--) {
    if (code & (1 << len)){
      printf("1");
    }
    else{
      printf("0");
    }
  }
}

static void print_codes(huffcode* codes[],size_t sz){
  int i,n;
  for(i=0;i<sz;i++){
    if(codes[i]!=NULL){
      n = codes[i]->code;
      printf("[%d\t|%d\t|0b ",codes[i]->c,codes[i]->codelen);
      print_binary(n,codes[i]->codelen);
      printf("]\n");
    }else{
      printf("NULL in stack [%d]\n",i);
    }
  }
}

huffnode* build_huffman_tree(huffnode* nodes[], int size){
    int n = size;
    huffnode *l, *r, *p;
    huffnode* dummy;

    if(n == 1){
        huffnode *only = nodes[0];
        printf("Makesingle %ld %d\n", only->freq, only->c);

        dummy = make_p_node(0, 0, NULL, NULL, LEAF);
        dummy->c = (only->c == 0 ? 1 : 0); /* dummy value */

        nodes[0] = make_p_node(0, only->freq, only, dummy, ROOT);
        return nodes[0];
    }

    radix_sort_xb((char**)nodes, HN_FREQ_OFFSET, sizeof(long)*8, n, 0);

    while(n > 1){
        int i;
        print_nodes(nodes, n);

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

    /*printf("T|%c|%d|0b ", i, len);
    print_binary(c, len);*/
  }
}

void generate_codes(huffnode* cnode,huffcode** code_list,unsigned char clen,unsigned short ccode){
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

    /* -----------------------------
       Step 1: build FULL histogram
       (no clamping!)
       ----------------------------- */
    for (i = 0; i < n; i++) {
        int len = code_list[i]->codelen;
        if (len >= MAX_CODELEN) len = MAX_CODELEN - 1;
        bl_count[len]++;
    }

    /* -----------------------------
       Step 2: count overflow
       ----------------------------- */
    int overflow = 0;
    for (i = maxbits + 1; i < MAX_CODELEN; i++) {
        overflow += bl_count[i];
    }

    /* -----------------------------
       Step 3: move overflow into maxbits
       ----------------------------- */
    bl_count[maxbits] += overflow;

    /* -----------------------------
       Step 4: redistribute
       ----------------------------- */
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

    /* -----------------------------
       Step 5: zero out invalid buckets
       ----------------------------- */
    for (i = maxbits + 1; i < MAX_CODELEN; i++) {
        bl_count[i] = 0;
    }

    /* -----------------------------
       Step 6: write back lengths
       (IMPORTANT: relies on sorted order)
       ----------------------------- */
    size_t idx = 0;
    int len;
    for (len = 1; len <= (int)maxbits; len++) {
        int count = bl_count[len];
        while (count-- > 0) {
            code_list[idx++]->codelen = len;
        }
    }
}


void make_deflate_codes(huffcode **table, size_t max_code, int maxbits) {
    int bl_count[16] = {0};     
    int next_code[16] = {0};    
    int code, bits, n, len;

    /*Step 1: count codes per code length*/
    for (n = 0; n < max_code; n++) {
        len = table[n]->codelen;
        if (len > 0 && len <= maxbits) {
            bl_count[len]++;
        }
    }

    /*Step 2: compute starting code for each length*/
    code = 0;
    bl_count[0] = 0;  
    for (bits = 1; bits <= maxbits; bits++) {
        code = (code + bl_count[bits - 1]) << 1;
        next_code[bits] = code;
        /*Optional debug
         printf("length %d starts at code 0x%x\n", bits, code);*/
    }

    /* Step 3: assign codes to symbols*/
    for (n = 0; n < max_code; n++) {
        len = table[n]->codelen;
        if (len != 0) {
            table[n]->code = next_code[len];
            next_code[len]++;
            /* Optional debug
             printf("sym %d len %d code 0x%x\n", n, len, table[n]->code);
             */
        }
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
      printf("Decode error\n");
      return;
    }

    /* reached symbol */
    if(cur->ntype == LEAF){
      sym = cur->c;
      if(fdsym(sym,data,&bitpos,&bytepos)){
        printf("\nEOB\n");
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


void count_literals(short symbol){
  huffmancoder* literalcodes= global_codingtable[0];
  if(symbol>=LCODEBASE){
    symbol = LCODEBASE+LENCODE_LOOKUP[symbol-LCODEBASE];
  }
  long* r = &literalcodes->freq[symbol]; 
  if(*r==0){
    literalcodes->table[symbol]=literalcodes->distinct;
    global_nodes[0][literalcodes->distinct] = make_node(symbol,0,LEAF);
    literalcodes->distinct++;
  }
  (*r)++;    
  global_nodes[0][literalcodes->table[symbol]]->freq++;
}

void count_ldcodes(short length, short distance){
  huffmancoder* dlcodes= global_codingtable[1];
  int dcode;
  long* r;
  dcode = get_distance_code(distance);
  global_distancecodes[0][dlcodes->data_count++]=dcode;
  /*printf("Put dsym: %d into idx: %d",distance, dlcodes->data_count-1);*/
  r = &dlcodes->freq[dcode]; 
  if(*r==0){
    /*printf("\nDistance bucket %d\n",distance);*/
    dlcodes->table[dcode]=dlcodes->distinct;
    global_nodes[1][dlcodes->distinct] = make_node(dcode,0,LEAF);
    dlcodes->distinct++;
  }
  (*r)++;    
  global_nodes[1][dlcodes->table[dcode]]->freq++;
  count_literals(length+LCODEBASE);
}

void count_clcodes(short symbol){
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
  printf("\nCreating Table with distinct: %d\n\n",hobj->distinct);
  if(hobj->distinct == 0){
    printf("No symbols to encode\n");
    return;
  }
  hobj->root = build_huffman_tree(nodes,hobj->distinct);
  printf("Gen Codes:");
  generate_codes(hobj->root,hobj->codetable,0,0);

  printf(" Ok\n\nSort (Length|Symbol) ASC:\n");
  /*radix_sort_xb((char**)dense,HC_FREQ_OFFSET,HC_FREQ_SIZE*8,hobj->distinct,0);
  
  for (i = 0; i < HUFFMAN_ALPHABET_SZ; i++) {
    if (i<hobj->distinct) {
      hobj->codetable[i] = dense[i];
    }else{
      hobj->codetable[i] = NULL;
    }
  }
  */
  insertion_sort_xxB((char**)hobj->codetable, HC_CLEN_OFFSET, HC_SYM_OFFSET, HC_CLEN_SIZE, HC_SYM_SIZE, HUFFMAN_ALPHABET_SZ, 0);
  
  print_codes(hobj->codetable,hobj->distinct);
  
  printf(" Ok\nLimit Length:\n");
  limit_code_length(hobj->codetable,hobj->distinct,limit_len);
  
  insertion_sort_xxB((char**)hobj->codetable, HC_CLEN_OFFSET, HC_SYM_OFFSET, HC_CLEN_SIZE, HC_SYM_SIZE, HUFFMAN_ALPHABET_SZ, 0);
  
  print_codes(hobj->codetable,hobj->distinct);
  printf(" Ok\n\nMake deflate codes:\n\n");
  
  make_deflate_codes(hobj->codetable,hobj->distinct,limit_len);
  radix_sort_xb((char**)hobj->codetable,HC_SYM_OFFSET,HC_SYM_SIZE*8,hobj->distinct,0);
  print_codes(hobj->codetable,hobj->distinct);
  make_reverse_codes(hobj);
  printf("\nDone Creating Table\n");
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

/*encoder block*/
int main(int argv, char** argc){
  char* input,input0[4182],distance_codes[WINDOW_SIZE];
  int i,j,min,f,offset,offset1,pos,dist,match_len,ptr_count,next_ptr,next_pos,input_size;
  long *r;
  short codetable[HUFFMAN_ALPHABET_SZ],sym,lcode;
  huffnode* cnodes[HUFFMAN_ALPHABET_SZ],*dnodes[29],*clnodes[18];
  slzss_pointer lzss_ptrs[WINDOW_SIZE];
  /*
  huffnode hn;
  huffcode hc;
  */
  huffmancoder o_huffman,i_huffman,cl_huffman,d_huffman;
  bitarray ba;
  
  init_s_huffmancode(&o_huffman);
  init_s_huffmancode(&d_huffman);
  init_s_huffmancode(&cl_huffman);
  init_s_huffmancode(&i_huffman);
  
  i=0;
  input0[4180] = '\0';
  _t_make_test_string(input0);
  input = input0;/*argc[1];*/
  input = argc[1];
  ptr_count = 0;
  /*LZSS*/
  input_size = argv==3?strtol(argc[2], NULL, 10):4179;
  
  for(i=0;i<WINDOW_SIZE;i++) head[i] = -1;
  for(i=0;i<WINDOW_SIZE;i++) prev[i] = -1;
  printf("LZSS portion inputsize: %d\n",input_size);
  
  global_codingtable[0] = &o_huffman;  
  global_codingtable[1] = &d_huffman;
  global_codingtable[2] = &cl_huffman;
  
  global_nodes[0] = cnodes;
  global_nodes[1] = dnodes;
  global_nodes[2] = clnodes;
  
  global_distancecodes[0] = distance_codes;
  
  ptr_count = generate_lzss_pointers(input,input_size,lzss_ptrs,WINDOW_SIZE,&pos,count_ldcodes,count_literals);
  count_literals(EOBCODE);
  
  printf("\nTotal ptrs: %d ->%d\n",ptr_count,pos);
  
  
  printf("Parsed %d %d symbols\n",pos,o_huffman.distinct);
  
  create_table(&o_huffman,cnodes,HUFFMAN_ALPHABET_SZ,15);
  create_table(&d_huffman,dnodes,30,15);
  
  next_ptr = 0,i=0;
  next_pos = -1;
  if(next_ptr<WINDOW_SIZE && ptr_count>0){
    next_pos = lzss_ptrs[0].position;
  }

  while(i<input_size){
    if(i==next_pos){
    /*
      printf("np %d ptr: %d\n",next_pos,next_ptr);
    */
      sym = lzss_ptrs[next_ptr].length;
      /*printf("symbol: %d\n",sym);*/
      lcode = LENCODE_LOOKUP[sym]+LCODEBASE;
      /*printf("code len: %d\n",lcode);*/
      packbits(&ba,o_huffman.revcodetable[lcode]->code,o_huffman.revcodetable[lcode]->codelen);
      /*printf("grab literal: %d\n",lcode);*/
      packbits(&ba,sym,LENCODE_EBITS[lcode-LCODEBASE]);/*write length literal*/
      
      sym = lzss_ptrs[next_ptr].distance;
      /*printf("GrabSymbol %d\n",next_ptr);*/
      lcode = global_distancecodes[0][next_ptr];
      /*printf("DCode: %d\n",lcode);*/
      packbits(&ba,d_huffman.revcodetable[lcode]->code,d_huffman.revcodetable[lcode]->codelen);
      packbits(&ba,sym,DISTANCE_LENS[lcode-LCODEBASE]);/*write distance*/
      i+=lzss_ptrs[next_ptr].length;
      next_pos = lzss_ptrs[++next_ptr].position;
      /*printf("i: %d nxtpos: %d\n",i,next_pos);*/
    }else{
      printf("%c %d",input[i],o_huffman.revcodetable[input[i]]->codelen);
      packbits(&ba,o_huffman.revcodetable[input[i]]->code,o_huffman.revcodetable[input[i]]->codelen);
      i++;
    }
  }
  printf("\nEOB\n");
  packbits(&ba,o_huffman.revcodetable[EOBCODE]->code,o_huffman.revcodetable[EOBCODE]->codelen);
  bitarray_flush(&ba);
  printf("\n");
  printf("BA L: %ld bits: %ld\n\n",ba.used,ba.bitcount);
  
  bitarray bh;
  unsigned short lit_codelens[HUFFMAN_ALPHABET_SZ];
  unsigned short d_codelens[30];
  unsigned short cl_codelens[19];
  unsigned short combined[HUFFMAN_ALPHABET_SZ+30];
  rletoken compressedlens[HUFFMAN_ALPHABET_SZ+30];
  int lit_n = dump_codelens(lit_codelens, &o_huffman,HUFFMAN_ALPHABET_SZ);
  int d_n = dump_codelens(d_codelens,&d_huffman,30);
  
  int n = 0;
  for(i=0;i<lit_n;i++)
      combined[n++] = lit_codelens[i];
  for(i=0;i<d_n;i++)
      combined[n++] = d_codelens[i];
  for(i=0;i<n;i++){
    if(combined[i])printf("%d: [%d] \n",i,combined[i]);
  }
  int nc = rle_codelens(combined,n,compressedlens);
  printf("Code len [%d] processed: %d\n",n,nc);
  for(i=0;i<nc;i++){
    printf("%d %d\n",i,compressedlens[i].sym);
    count_clcodes(compressedlens[i].sym);
  }
  create_table(&cl_huffman,clnodes,nc,15);
  
  /*Write Header*/
  printf("\n\n==========Header===========\n\n");
  int cl_n = dump_codelens(cl_codelens,&cl_huffman,nc);

  int HLIT = lit_n-LCODEBASE;
  int HDIST = d_n-1;
  packbits(&bh,1,1);      /*BFINAL =1*/
  
  packbits(&bh,2,2);      /*BTYPE = 10*/
  packbits(&bh,HLIT,5);   
  packbits(&bh,HDIST,5);  
  
  printf("\n");
  char cl_lensbuffer [19]={0};
  char max_cl=4;
  for(i=0;i<19;i++){
      if(!cl_huffman.revcodetable[i]) continue;
      cl_lensbuffer[CODELEN_IDX_ORDER[cl_huffman.revcodetable[i]->c]] = cl_huffman.revcodetable[i]->codelen;
      max_cl = cl_huffman.revcodetable[i]->codelen==0?max_cl:CODELEN_IDX_ORDER[i]>max_cl?CODELEN_IDX_ORDER[i]:max_cl;
  }
  for(i=0;i<=max_cl;i++){
    printf("%d\t[%d]: \t%d\n",i,CODELEN_ORDER[i],cl_lensbuffer[i]);
  }
  printf("\n");

  int HCLEN = max_cl-3;

  printf("HLIT:%d HDIST:%d HCLEN:%d\n",HLIT,HDIST,HCLEN);

  packbits(&bh,HCLEN,4);  

  printf("Pack bits for code lens\n");
  for(i=0;i<HCLEN+4;i++){
    int cl = reverse_bits(cl_lensbuffer[i],3);
    packbits(&bh,cl,3);
    print_binary(cl,3);
    printf(" ");
  }
  
  free(ba.data);
  
  free_huffman_heap(&o_huffman,HUFFMAN_ALPHABET_SZ);
  
  free_huffman_heap(&d_huffman,29);
  
  free_huffman_heap(&cl_huffman,nc);
   
  
}
