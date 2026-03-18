
#include "../bithelper/bithelpers.h"

#include "deflate.h"
#include "../cursedhelpers.h"
#include "../huffman/huffman.h"
#include "../lzss/lzss.h"
const unsigned short DISTANCE_BASE[30]={
  1,    2,    3,    4,    5,    7,
  9,    13,   17,   25,   33,   49,
  65,   97,   129,  193,  257,  385,
  513,  769,  1025, 1537, 2049, 3073,
  4097, 6145, 8193, 12289,  16385,  24577
};

const unsigned char DISTANCE_LENS[30]={
  0,    0,    0,    0,    1,    1,
  2,    2,    3,    3,    4,    4,
  5,    5,    6,    6,    7,    7,
  8,    8,    9,    9,    10,    10,
  11,   11,   12,   12,   13,   13
};

int get_distance_code(short distance){
  int lo = 0;
  int hi = 29;
  int mid;

  while (lo <= hi) {
    mid = (lo + hi) >> 1;
    if (DISTANCE_BASE[mid] <= distance) {
      if (mid == 29 || DISTANCE_BASE[mid + 1] > distance)
        return mid;
      lo = mid + 1;
    }else {
      hi = mid - 1;
    }
  }
  return 0;
} 

const unsigned char LENCODE_EBITS[29]={/*Non literal codes*/
  0,  0,  0,  0,  0,  0,
  0,  0,  1,  1,  1,  1,
  2,  2,  2,  2,  3,  3,
  3,  3,  4,  4,  4,  4,
  5,  5,  5,  5,  0
};
const unsigned short LENCODE_BASE[29]={/*Non literal codes*/
  3,    4,    5,    6,    7,    8,
  9,    10,   11,   13,   15,   17,
  19,   23,   27,   31,   35,   43,
  51,   59,   67,   83,   99,   115,
  131,  163,  195,  227,  258   
};

const unsigned char LENCODE_LOOKUP[259] = {
/*0*/  0,
/*1*/  0,
/*2*/  0,

/*3*/  0,  /*4*/  1,  /*5*/  2,  /*6*/  3,
/*7*/  4,  /*8*/  5,  /*9*/  6,  /*10*/ 7,

/*11*/ 8,  /*12*/ 8,
/*13*/ 9,  /*14*/ 9,
/*15*/ 10, /*16*/ 10,
/*17*/ 11, /*18*/ 11,

/*19*/ 12, /*20*/ 12, /*21*/ 12, /*22*/ 12,
/*23*/ 13, /*24*/ 13, /*25*/ 13, /*26*/ 13,
/*27*/ 14, /*28*/ 14, /*29*/ 14, /*30*/ 14,
/*31*/ 15, /*32*/ 15, /*33*/ 15, /*34*/ 15,

/*35*/ 16, /*36*/ 16, /*37*/ 16, /*38*/ 16,
/*39*/ 16, /*40*/ 16, /*41*/ 16, /*42*/ 16,
/*43*/ 17, /*44*/ 17, /*45*/ 17, /*46*/ 17,
/*47*/ 17, /*48*/ 17, /*49*/ 17, /*50*/ 17,

/*51*/ 18, /*52*/ 18, /*53*/ 18, /*54*/ 18,
/*55*/ 18, /*56*/ 18, /*57*/ 18, /*58*/ 18,
/*59*/ 19, /*60*/ 19, /*61*/ 19, /*62*/ 19,
/*63*/ 19, /*64*/ 19, /*65*/ 19, /*66*/ 19,

/*67*/ 20, /*68*/ 20, /*69*/ 20, /*70*/ 20,
/*71*/ 20, /*72*/ 20, /*73*/ 20, /*74*/ 20,
/*75*/ 20, /*76*/ 20, /*77*/ 20, /*78*/ 20,
/*79*/ 20, /*80*/ 20, /*81*/ 20, /*82*/ 20,

/*83*/ 21, /*84*/ 21, /*85*/ 21, /*86*/ 21,
/*87*/ 21, /*88*/ 21, /*89*/ 21, /*90*/ 21,
/*91*/ 21, /*92*/ 21, /*93*/ 21, /*94*/ 21,
/*95*/ 21, /*96*/ 21, /*97*/ 21, /*98*/ 21,

/*99*/ 22,  /*100*/22,  /*101*/22,  /*102*/22,
/*103*/22,  /*104*/22,  /*105*/22,  /*106*/22,
/*107*/22,  /*108*/22,  /*109*/22,  /*110*/22,
/*111*/22,  /*112*/22,  /*113*/22,  /*114*/22,

/*115*/23, /*116*/23, /*117*/23, /*118*/23,
/*119*/23, /*120*/23, /*121*/23, /*122*/23,
/*123*/23, /*124*/23, /*125*/23, /*126*/23,
/*127*/23, /*128*/23, /*129*/23, /*130*/23,

/*131*/24, /*132*/24, /*133*/24, /*134*/24,
/*135*/24, /*136*/24, /*137*/24, /*138*/24,
/*139*/24, /*140*/24, /*141*/24, /*142*/24,
/*143*/24, /*144*/24, /*145*/24, /*146*/24,
/*147*/24, /*148*/24, /*149*/24, /*150*/24,
/*151*/24, /*152*/24, /*153*/24, /*154*/24,
/*155*/24, /*156*/24, /*157*/24, /*158*/24,
/*159*/24, /*160*/24, /*161*/24, /*162*/24,

/*163*/25, /*164*/25, /*165*/25, /*166*/25,
/*167*/25, /*168*/25, /*169*/25, /*170*/25,
/*171*/25, /*172*/25, /*173*/25, /*174*/25,
/*175*/25, /*176*/25, /*177*/25, /*178*/25,
/*179*/25, /*180*/25, /*181*/25, /*182*/25,
/*183*/25, /*184*/25, /*185*/25, /*186*/25,
/*187*/25, /*188*/25, /*189*/25, /*190*/25,
/*191*/25, /*192*/25, /*193*/25, /*194*/25,

/*195*/26, /*196*/26, /*197*/26, /*198*/26,
/*199*/26, /*200*/26, /*201*/26, /*202*/26,
/*203*/26, /*204*/26, /*205*/26, /*206*/26,
/*207*/26, /*208*/26, /*209*/26, /*210*/26,
/*211*/26, /*212*/26, /*213*/26, /*214*/26,
/*215*/26, /*216*/26, /*217*/26, /*218*/26,
/*219*/26, /*220*/26, /*221*/26, /*222*/26,
/*223*/26, /*224*/26, /*225*/26, /*226*/26,

/*227*/27, /*228*/27, /*229*/27, /*230*/27,
/*231*/27, /*232*/27, /*233*/27, /*234*/27,
/*235*/27, /*236*/27, /*237*/27, /*238*/27,
/*239*/27, /*240*/27, /*241*/27, /*242*/27,
/*243*/27, /*244*/27, /*245*/27, /*246*/27,
/*247*/27, /*248*/27, /*249*/27, /*250*/27,
/*251*/27, /*252*/27, /*253*/27, /*254*/27,
/*255*/27, /*256*/27, /*257*/27,

/*258*/28
};

const unsigned char CODELEN_ORDER[19] = {
  16, 17, 18, 0,  8,  7,  9,  6,  10, 5,  /*10*/11, 4,  12, 3,  13, 2,  14, 1, 15
};
const unsigned char CODELEN_IDX_ORDER[19] = {
  3, 17, 15, 13,  11,  9,  7,  5,  4,   6,  /*10*/8,  10,  12, 14,  16,   18,  0,   1,  2
};
const unsigned char CODELEN_EBITS[19] = {
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  2,  3, 7
};
const unsigned char CODELEN_EBITS_BASE[19] = {
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  3,  3, 11
};

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
        LOG_I("length %d starts at code 0x%x\n", bits, code);*/
    }

    /* Step 3: assign codes to symbols*/
    for (n = 0; n < max_code; n++) {
      len = table[n]->codelen;
      if (len != 0) {
        table[n]->code = next_code[len];
        next_code[len]++;
      }
    }
}

int rle_codelens(const unsigned short *codelens,
                 int sz,
                 rletoken *out)
{
  int i = 0;
  int w = 0;

  while (i < sz) {

    unsigned short cur = codelens[i];
    int run = 1;

    /* count run length */
    while (i + run < sz && codelens[i + run] == cur)
      run++;
    int origin_run = run;
    if (cur == 0) {
      /* encode long zero runs */
      while (run >= 11) {
        int cnt = run > 138 ? 138 : run;
        out[w++] = (rletoken){18, cnt};
        run -= cnt;
      }
      if (run >= 3) {
        int cnt = run > 10 ? 10 : run;
        out[w++] = (rletoken){17, cnt};
        run -= cnt;
      }
      while (run > 0) {
        out[w++] = (rletoken){0,1};
        run--;
      }
    }
    else {
      /* emit first literal length */
      out[w++] = (rletoken){cur,1};
      run--;

      /* encode repeats of previous length */
      while (run >= 3) {
        int cnt = run > 6 ? 6 : run;
        out[w++] = (rletoken){16, cnt};
        run -= cnt;
      }

      while (run > 0) {
        out[w++] = (rletoken){cur,1};
        run--;
      }
    }
    i += origin_run;
  }
  return w;
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
  /*LOG_I("Put dsym: %d into idx: %d",distance, dlcodes->data_count-1);*/
  r = &dlcodes->freq[dcode]; 
  if(*r==0){
    /*LOG_I("\nDistance bucket %d\n",distance);*/
    dlcodes->table[dcode]=dlcodes->distinct;
    global_nodes[1][dlcodes->distinct] = make_node(dcode,0,LEAF);
    dlcodes->distinct++;
  }
  (*r)++;    
  global_nodes[1][dlcodes->table[dcode]]->freq++;
  count_literals(length+LCODEBASE);
}

int deflate(bitarray* bBuffer, char* data,size_t input_sz){

    char* input,input0[4182],distance_codes[WINDOW_SIZE];
  int i,j,min,f,offset,offset1,pos,dist,match_len,ptr_count,next_ptr,next_pos,input_size;
  long *r;
  short codetable[HUFFMAN_ALPHABET_SZ],sym,lcode;
  huffnode* cnodes[HUFFMAN_ALPHABET_SZ],*dnodes[29],*clnodes[18];
  slzss_pointer lzss_ptrs[WINDOW_SIZE];
  huffmancoder o_huffman,cl_huffman,d_huffman;
  bitarray bh;
  
  init_s_huffmancode(&o_huffman);
  init_s_huffmancode(&d_huffman);
  init_s_huffmancode(&cl_huffman);
  
  i=0;
  input = data;
  ptr_count = 0;
  /*LZSS*/
  input_size = input_sz;
  
  for(i=0;i<WINDOW_SIZE;i++) head[i] = -1;
  for(i=0;i<WINDOW_SIZE;i++) prev[i] = -1;
  LOG_I("LZSS portion inputsize: %d\n",input_size);
  
  global_codingtable[0] = &o_huffman;  
  global_codingtable[1] = &d_huffman;
  global_codingtable[2] = &cl_huffman;
  
  global_nodes[0] = cnodes;
  global_nodes[1] = dnodes;
  global_nodes[2] = clnodes;
  
  global_distancecodes[0] = distance_codes;
  
  ptr_count = generate_lzss_pointers(input,input_size,lzss_ptrs,WINDOW_SIZE,&pos,count_ldcodes,count_literals);
  count_literals(EOBCODE);
  
  LOG_I("\nTotal ptrs: %d ->%d\n",ptr_count,pos);
  
  LOG_I("Parsed %d %d symbols\n",pos,o_huffman.distinct);
  
  create_table(&o_huffman,cnodes,HUFFMAN_ALPHABET_SZ,15);
  create_table(&d_huffman,dnodes,30,15);
  
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
    if(combined[i])LOG_I("%d: [%d] \n",i,combined[i]);
  }
  int nc = rle_codelens(combined,n,compressedlens);
  LOG_I("Code len [%d] processed: %d\n",nc,n);
  for(i=0;i<nc;i++){
    count_clcodes(compressedlens[i].sym);
    LOG_I("%d %d\n",i,compressedlens[i].sym);
  }
  create_table(&cl_huffman,clnodes,nc,15);
  
  /*Write Header*/
  LOG_I("\n\n==========Header===========\n");
  int cl_n = dump_codelens(cl_codelens,&cl_huffman,nc);

  int HLIT = lit_n-LCODEBASE;
  int HDIST = d_n-1;
  
  LOG_I("\n");
  char cl_lensbuffer [19]={0};
  char max_cl=4;
  for(i=0;i<19;i++){
    if(!cl_huffman.revcodetable[i]) continue;
    cl_lensbuffer[CODELEN_IDX_ORDER[cl_huffman.revcodetable[i]->c]] = cl_huffman.revcodetable[i]->codelen;
    max_cl = cl_huffman.revcodetable[i]->codelen==0?max_cl:CODELEN_IDX_ORDER[i]>max_cl?CODELEN_IDX_ORDER[i]:max_cl;
  }
  for(i=0;i<=max_cl;i++){
    LOG_I("%d\t[%d]: \t%d\n",i,CODELEN_ORDER[i],cl_lensbuffer[i]);
  }
  LOG_I("\n");
  int HCLEN = max_cl-3;

  LOG_V("HLIT:%d HDIST:%d HCLEN:%d\n",HLIT,HDIST,HCLEN);
  packbits(&bh,1,1);      /*BFINAL =1*/
  PRINT_BINARY(1,1);

  packbits(&bh,2,2);      /*BTYPE = 10*/
  PRINT_BINARY(2,2);
  
  packbits(&bh,reverse_bits(HLIT,5),5);   
  PRINT_BINARY(reverse_bits(HLIT,5),5);
  packbits(&bh,reverse_bits(HDIST,5),5);  
  PRINT_BINARY(reverse_bits(HDIST,5),5);
  packbits(&bh,HCLEN,4);  
  PRINT_BINARY(reverse_bits(HCLEN,4),4);
  LOG_I("\n\nPacked bits for code lens\n");
  
  for(i=0;i<HCLEN+4;i++){
    int cl = reverse_bits(cl_lensbuffer[i],3);
    packbits(&bh,cl,3);
    PRINT_BINARY(cl,3);
    LOG_I(" ");
  }
  LOG_I("\n\nPacked bits for Literal & Distance lens\n");
  
  for(i=0;i<nc;i++){
    int _sym = compressedlens[i].sym;
    int cl = cl_huffman.revcodetable[_sym]->code;
    packbits(&bh,cl,cl_huffman.revcodetable[_sym]->codelen);
    PRINT_BINARY(cl,cl_huffman.revcodetable[_sym]->codelen);
    
    if(_sym>15){
      LOG_I(":[%d]",_sym);
      int ebits = CODELEN_EBITS[compressedlens[i].sym];
      packbits(&bh,reverse_bits(compressedlens[i].repeat-CODELEN_EBITS_BASE[compressedlens[i].sym],ebits),ebits);
      PRINT_BINARY(reverse_bits(compressedlens[i].repeat-CODELEN_EBITS_BASE[compressedlens[i].sym],ebits),ebits);
    }
  }

  LOG_I("\n\nPacked bits for Data\n");

  next_ptr = 0,i=0;
  next_pos = -1;
  if(next_ptr<WINDOW_SIZE && ptr_count>0){
    next_pos = lzss_ptrs[0].position;
  }

  while(i<input_size){
    if(i==next_pos){    
      sym = lzss_ptrs[next_ptr].length;
      lcode = LENCODE_LOOKUP[sym]+LCODEBASE;
      packbits(&bh,o_huffman.revcodetable[lcode]->code,o_huffman.revcodetable[lcode]->codelen);
      PRINT_BINARY(o_huffman.revcodetable[lcode]->code,o_huffman.revcodetable[lcode]->codelen);
      packbits(&bh,reverse_bits(sym,LENCODE_EBITS[lcode-LCODEBASE]),LENCODE_EBITS[lcode-LCODEBASE]);/*write length literal*/
      PRINT_BINARY(reverse_bits(sym,LENCODE_EBITS[lcode-LCODEBASE]),LENCODE_EBITS[lcode-LCODEBASE]);
      sym = lzss_ptrs[next_ptr].distance;
      lcode = global_distancecodes[0][next_ptr];
      packbits(&bh,d_huffman.revcodetable[lcode]->code,d_huffman.revcodetable[lcode]->codelen);
      PRINT_BINARY(d_huffman.revcodetable[lcode]->code,d_huffman.revcodetable[lcode]->codelen);
      packbits(&bh,reverse_bits(sym,DISTANCE_LENS[lcode]),DISTANCE_LENS[lcode]);/*write distance*/
      PRINT_BINARY(reverse_bits(sym,DISTANCE_LENS[lcode]),DISTANCE_LENS[lcode]);
      i+=lzss_ptrs[next_ptr].length;
      next_pos = lzss_ptrs[++next_ptr].position;
    }else{
      packbits(&bh,o_huffman.revcodetable[input[i]]->code,o_huffman.revcodetable[input[i]]->codelen);
      PRINT_BINARY(o_huffman.revcodetable[input[i]]->code,o_huffman.revcodetable[input[i]]->codelen);
      i++;
    }
  }
  LOG_I("\nEOB\n");
  packbits(&bh,o_huffman.revcodetable[EOBCODE]->code,o_huffman.revcodetable[EOBCODE]->codelen);
  bitarray_flush(&bh);
  LOG_I("\n");
  LOG_I("BH L: %ld bits: %ld\n\n",bh.used,bh.bitcount);

  free(bh.data);
  
  free_huffman_heap(&o_huffman,HUFFMAN_ALPHABET_SZ);
  
  free_huffman_heap(&d_huffman,30);
  
  free_huffman_heap(&cl_huffman,nc);
}
