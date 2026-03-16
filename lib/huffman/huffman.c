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

struct huffcode* make_code(short c,unsigned char len, unsigned short code){
  huffcode* hc;
  hc = malloc(sizeof(huffcode));
  hc->c = c;
  hc->codelen = len;
  hc->code = code;
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
  int lidx,n,offset,c;
  huffnode* l, * r,*p,movednodes[HUFFMAN_ALPHABET_SZ];
  offset = HN_FREQ_OFFSET;
  lidx = size-1;
  n = size;
  if(n == 1){
    huffnode *only = nodes[0];
    huffnode *dummy;

    printf("Makesingle %ld %d\n", only->freq, only->c);

    /* create a dummy leaf to form a valid tree */
    dummy = make_p_node(0, 0, NULL, NULL, LEAF);
    dummy->c = only->c;   /* value won't be used */

    /* create root with two children */
    nodes[0] = make_p_node(0, only->freq, only, dummy, ROOT);

    return nodes[0];
  }
  radix_sort_xb((char**)nodes,offset,sizeof(long)*8,n,1);
  while(n>1){
    /*print_nodes(nodes,n);*/
    l = nodes[n-1];
    r = nodes[n-2];
    nodes[n-2] = make_p_node(0,l->freq+r->freq,l,r,PARENT);
    nodes[n-1] = (huffnode*)NULL;
    n--;
    c = n - 1;
    while (c > 0 && nodes[c]->freq > nodes[c-1]->freq) {
      p = nodes[c-1];
      nodes[c-1] = nodes[c];
      nodes[c] = p;
      c--;
    }
  }
  nodes[0]->ntype=ROOT;
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
  unsigned char n0code, n1code;
  huffcode* hc;

  if(cnode==NULL){
    return;
  }
  if(cnode->ntype==LEAF){
    code_list[cnode->c] = make_code(cnode->c,clen,ccode);
    return;
  }
  n0code = (ccode<<1)|0;
  n1code = (ccode<<1)|1;
  ++clen;
  generate_codes(cnode->node0,code_list,clen,n0code);
  generate_codes(cnode->node1,code_list,clen,n1code); 
}

void limit_code_length(huffcode** code_list, size_t sz,size_t maxbits){
  int i,k,maxk;
  k=0;
  /*
  printf("Limiting code lengths to %ld\n",maxbits);
  */
  maxk = (1 << maxbits) - 1;
  for(i=0;i<sz;i++){
    if(code_list[i]->codelen>maxbits){
  /*
      printf("Truncate code idx:%d\n",i);
  */
      code_list[i]->codelen = maxbits;
    }
    k+= 1<<(maxbits-code_list[i]->codelen);
  }
  for(i=sz-1;i>0;i--){
    while(code_list[i]->codelen<maxbits){
      ++code_list[i]->codelen;
      k-= 1<<(maxbits-code_list[i]->codelen);
      /*
      printf("Back k:%d %d\n",k,i);
      */
    } 
  }
  for(i=0;i<sz;i++){
    while(k+(1<<(maxbits-code_list[i]->codelen))<=maxk){
      k+=1<<(maxbits-code_list[i]->codelen);
      --code_list[i]->codelen;
      /*
      printf("Forward k:%d %d\n",k,i);
      */
    }
  }
}

void make_deflate_codes(huffcode** code_list, size_t sz)
{
  int i, cl;
  int code = 0;
  int bits;
  int bl_count[16];
  int next_code[16];
  printf("making deflate codes of array sz: %ld\n",sz);
  memset(bl_count, 0, sizeof(bl_count));
  memset(next_code, 0, sizeof(next_code));

  /* count code lengths */
  for(i=0;i<sz;i++){
    if(code_list[i]->codelen > 0){
      bl_count[code_list[i]->codelen]++;
    }
  }

  /* compute canonical start codes */
  for(bits = 1; bits <= 15; bits++){
    code = (code + bl_count[bits-1]) << 1;
    next_code[bits] = code;
  }

  /* assign codes */
  for(i=0;i<sz;i++){
    cl = code_list[i]->codelen;

    if(cl != 0){
      code_list[i]->code = next_code[cl];
      next_code[cl]++;
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
  /*printf("\nCreating Table with distinct: %d\n",hobj->distinct);*/
  hobj->root = build_huffman_tree(nodes,hobj->distinct);
  
  generate_codes(hobj->root,hobj->codetable,0,0);
  /*printf("Generated huffman\n");*/
  insertion_sort_xxB((char**)hobj->codetable, HC_CLEN_OFFSET, HC_SYM_OFFSET, HC_CLEN_SIZE, HC_SYM_SIZE, stack_sz, 0);
  /*
  print_codes(hobj->codetable,hobj->distinct);
  */
  limit_code_length(hobj->codetable,hobj->distinct,limit_len);
  insertion_sort_xxB((char**)hobj->codetable, HC_CLEN_OFFSET, HC_SYM_OFFSET, HC_CLEN_SIZE, HC_SYM_SIZE, stack_sz, 0);
  
  /*print_codes(hobj->codetable,hobj->distinct);*/

  make_deflate_codes(hobj->codetable,hobj->distinct);
  radix_sort_xb((char**)hobj->codetable,HC_SYM_OFFSET,HC_SYM_SIZE*8,hobj->distinct,0);
  print_codes(hobj->codetable,hobj->distinct);
  make_reverse_codes(hobj);
  /*printf("\nDone Creating Table\n");*/
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
  input0[4181] = '\0';
  input0[4180] = '|';
  _t_make_test_string(input0);
  input = input0;/*argc[1];*/
  input = argc[1];
  ptr_count = 0;
  /*LZSS*/
  input_size = 19;
  
  for(i=0;i<WINDOW_SIZE;i++) head[i] = -1;
  for(i=0;i<WINDOW_SIZE;i++) prev[i] = -1;
  printf("LZSS portion\n");
  /*
  for(pos = 0; pos < input_size - MIN_MATCH; pos++)
  {
    dist = 0;
    int remaining = input_size - pos;
    int maxlen = remaining < LOOKAHEAD_SIZE ? remaining : LOOKAHEAD_SIZE;
    match_len = find_match(input, pos, maxlen, &dist);
    if(match_len >= MIN_MATCH)
    {
      emit_pointer(&lzss_ptrs[ptr_count++], pos, match_len, dist);
      for(i=0;i<match_len;i++){
        insert_hash(input, pos+i);
      }
      pos += match_len - 1;
    }
    else
    {
      insert_hash(input, pos);
    }
  }*/
  global_codingtable[0] = &o_huffman;  
  global_codingtable[1] = &d_huffman;
  global_codingtable[2] = &cl_huffman;
  
  global_nodes[0] = cnodes;
  global_nodes[1] = dnodes;
  global_nodes[2] = clnodes;
  
  global_distancecodes[0] = distance_codes;
  
  ptr_count = generate_lzss_pointers(input,input_size,lzss_ptrs,WINDOW_SIZE,&pos,count_ldcodes,count_literals);
  count_literals(EOBCODE);
  
  printf("Total ptrs: %d ->%d\n",ptr_count,pos);
  /*
  i=0;
  while(input[i]!='\0'&&o_huffman.distinct<HUFFMAN_ALPHABET_SZ){
  
    printf("CInput: %c\n",input[i]);
  
    r = &o_huffman.freq[input[i]]; 
    if(*r==0){
      o_huffman.table[input[i]]=o_huffman.distinct;
      cnodes[o_huffman.distinct] = make_node(input[i],0,LEAF);
  
      printf("Put %c into track slot: %d\n",input[i],o_huffman.distinct);
  
      o_huffman.distinct++;
    }
    (*r)++;    
    cnodes[o_huffman.table[input[i]]]->freq++;
    i++;
  }
  */
  
  printf("Parsed %d %d symbols\n",pos,o_huffman.distinct);
  /*
  offset = (char*)&hn.freq-(char*)&hn;
  
  o_huffman.root = build_huffman_tree(cnodes,o_huffman.distinct);
  
  generate_codes(o_huffman.root,o_huffman.codetable,0,0);
  
  offset = (char*)&hc.codelen - (char*)&hc;
  offset1 = (char*)&hc.c - (char*)&hc;
  insertion_sort_xxB((char**)o_huffman.codetable, offset, offset1, sizeof(hc.codelen), sizeof(hc.c), HUFFMAN_ALPHABET_SZ, 0);
  print_codes(o_huffman.codetable,o_huffman.distinct);
  limit_code_length(o_huffman.codetable,o_huffman.distinct,15);
  insertion_sort_xxB((char**)o_huffman.codetable, offset, offset1, sizeof(hc.codelen), sizeof(hc.c), HUFFMAN_ALPHABET_SZ, 0);
  print_codes(o_huffman.codetable,o_huffman.distinct);
  make_deflate_codes(o_huffman.codetable,o_huffman.distinct);
  radix_sort_xb((char**)o_huffman.codetable,0,sizeof(unsigned short)*8,o_huffman.distinct,0);
  print_codes(o_huffman.codetable,o_huffman.distinct);
  make_reverse_codes(&o_huffman);
  */
  create_table(&o_huffman,cnodes,HUFFMAN_ALPHABET_SZ,15);
  create_table(&d_huffman,dnodes,30,5);
  /*
  print_codes(o_huffman.revcodetable,o_huffman.distinct);
  */
  next_ptr = 0,i=0;
  next_pos = -1;
  if(next_ptr<WINDOW_SIZE){
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
  

  /*
  for(i=0;i<o_huffman.distinct;i++){
    codetable[o_huffman.codetable[i]->c] = o_huffman.codetable[i]->codelen;
  }
  */
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
  /*
  for(i=0;i<lit_n;i++){
    printf("%d %d\n",i,lit_codelens[i]);
  }
  */
  /*
  for(i=0;i<d_n;i++){
    printf("%d %d\n",i,d_codelens[i]);
  }*/
  int nc = rle_codelens(combined,n,compressedlens);
  printf("Code len [%d] processed: %d\n",n,nc);
  for(i=0;i<nc;i++){
    printf("%d %d\n",i,compressedlens[i].sym);
    count_clcodes(compressedlens[i].sym);
  }
  create_table(&cl_huffman,clnodes,nc,3);
  
  /*Write Header*/
  printf("Header\n");
  int cl_n = dump_codelens(cl_codelens,&cl_huffman,nc);

  int HLIT = lit_n-LCODEBASE;
  int HDIST = d_n-1;
  int HCLEN = cl_n-4;

  printf("HLIT:%d HDIST:%d HCLEN:%d\n",HLIT,HDIST,HCLEN);
  packbits(&bh,1,1);      /*BFINAL =1*/
  
  packbits(&bh,2,2);      /*BTYPE = 10*/
  packbits(&bh,HLIT,5);   
  packbits(&bh,HDIST,5);  
  packbits(&bh,HCLEN,4);  
  
  printf("\n");
  char cl_lensbuffer [19]={0};
  for(i=0;i<cl_n;i++){
      printf("%d\n",cl_huffman.revcodetable[i]->c);
      cl_lensbuffer[CODELEN_ORDER[cl_huffman.revcodetable[i]->c]] = cl_huffman.revcodetable[i]->code;
  }
  for(i=0;i<cl_n;i++){
  for(i=0;i<n;i++){
    packbits(&bh,cl_lensbuffer[i],3);
    print_binary(cl_lensbuffer[i],3);
  }
  
  free(ba.data);
  
  free_huffman_heap(&o_huffman,HUFFMAN_ALPHABET_SZ);
  
  free_huffman_heap(&d_huffman,29);
  
  free_huffman_heap(&cl_huffman,nc);
  
  
  
}
