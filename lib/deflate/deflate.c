
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
  int i=0;
    for(i=0;i<30;i++){
        if(distance<DISTANCE_BASE[i]){
            return i-1;
        }
  }
  return 29;
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

const unsigned short LENCODE_LOOKUP[259] = {
/*0*/  0,
/*1*/  0,
/*2*/  0,

/*3*/  257,  /*4*/  258,  /*5*/  259,  /*6*/  260,
/*7*/  261,  /*8*/  262,  /*9*/  263,  /*10*/ 264,

/*11*/ 265,  /*12*/ 265,
/*13*/ 266,  /*14*/ 266,
/*15*/ 267, /*16*/ 267,
/*17*/ 268, /*18*/ 268,

/*19*/ 269, /*20*/ 269, /*21*/ 269, /*22*/ 269,
/*23*/ 270, /*24*/ 270, /*25*/ 270, /*26*/ 270,
/*27*/ 271, /*28*/ 271, /*29*/ 271, /*30*/ 271,
/*31*/ 272, /*32*/ 272, /*33*/ 272, /*34*/ 272,

/*35*/ 273, /*36*/ 273, /*37*/ 273, /*38*/ 273, /*39*/ 273, /*40*/ 273, /*41*/ 273, /*42*/ 273,
/*43*/ 274, /*44*/ 274, /*45*/ 274, /*46*/ 274, /*47*/ 274, /*48*/ 274, /*49*/ 274, /*50*/ 274,

/*51*/ 275, /*52*/ 275, /*53*/ 275, /*54*/ 275, /*55*/ 275, /*56*/ 275, /*57*/ 275, /*58*/ 275,
/*59*/ 276, /*60*/ 276, /*61*/ 276, /*62*/ 276, /*63*/ 276, /*64*/ 276, /*65*/ 276, /*66*/ 276,

/*67*/ 277, /*68*/ 277, /*69*/ 277, /*70*/ 277, /*71*/ 277, /*72*/ 277, /*73*/ 277, /*74*/ 277, /*75*/ 277, /*76*/ 277, /*77*/ 277, /*78*/ 277, /*79*/ 277, /*80*/ 277, /*81*/ 277, /*82*/ 277,
/*83*/ 278, /*84*/ 278, /*85*/ 278, /*86*/ 278, /*87*/ 278, /*88*/ 278, /*89*/ 278, /*90*/ 278, /*91*/ 278, /*92*/ 278, /*93*/ 278, /*94*/ 278, /*95*/ 278, /*96*/ 278, /*97*/ 278, /*98*/ 278,

/*99*/ 279,  /*100*/279,  /*101*/279,  /*102*/279, /*103*/279,  /*104*/279,  /*105*/279,  /*106*/279, /*107*/279,  /*108*/279,  /*109*/279,  /*110*/279, /*111*/279,  /*112*/279,  /*113*/279,  /*114*/279,

/*115*/280, /*116*/280, /*117*/280, /*118*/280, /*119*/280, /*120*/280, /*121*/280, /*122*/280, /*1280*/280, /*124*/280, /*125*/280, /*126*/280, /*127*/280, /*128*/280, /*129*/280, /*130*/280,

/*131*/281, /*132*/281, /*133*/281, /*134*/281,
/*135*/281, /*136*/281, /*137*/281, /*138*/281,
/*139*/281, /*140*/281, /*141*/281, /*142*/281,
/*143*/281, /*144*/281, /*145*/281, /*146*/281,
/*147*/281, /*148*/281, /*149*/281, /*150*/281,
/*151*/281, /*152*/281, /*153*/281, /*154*/281,
/*155*/281, /*156*/281, /*157*/281, /*158*/281,
/*159*/281, /*160*/281, /*161*/281, /*162*/281,

/*163*/282, /*164*/282, /*165*/282, /*166*/282,
/*167*/282, /*168*/282, /*169*/282, /*170*/282,
/*171*/282, /*172*/282, /*173*/282, /*174*/282,
/*175*/282, /*176*/282, /*177*/282, /*178*/282,
/*179*/282, /*180*/282, /*181*/282, /*182*/282,
/*183*/282, /*184*/282, /*185*/282, /*186*/282,
/*187*/282, /*188*/282, /*189*/282, /*190*/282,
/*191*/282, /*192*/282, /*193*/282, /*194*/282,

/*195*/283, /*196*/283, /*197*/283, /*198*/283,
/*199*/283, /*200*/283, /*201*/283, /*202*/283,
/*203*/283, /*204*/283, /*205*/283, /*206*/283,
/*207*/283, /*208*/283, /*209*/283, /*210*/283,
/*211*/283, /*212*/283, /*213*/283, /*214*/283,
/*215*/283, /*216*/283, /*217*/283, /*218*/283,
/*219*/283, /*220*/283, /*221*/283, /*222*/283,
/*223*/283, /*224*/283, /*225*/283, /*226*/283,

/*227*/284, /*228*/284, /*229*/284, /*230*/284,
/*231*/284, /*232*/284, /*233*/284, /*234*/284,
/*235*/284, /*236*/284, /*237*/284, /*238*/284,
/*239*/284, /*240*/284, /*241*/284, /*242*/284,
/*243*/284, /*244*/284, /*245*/284, /*246*/284,
/*247*/284, /*248*/284, /*249*/284, /*250*/284,
/*251*/284, /*252*/284, /*253*/284, /*254*/284,
/*255*/284, /*256*/284, /*257*/284,

/*258*/285
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
  /*if(symbol>=LCODEBASE){
    symbol = LCODEBASE+LENCODE_LOOKUP[symbol-LCODEBASE];
  }*/
    int c;
  long* r = &(literalcodes->freq[symbol]); 
  if(*r==0){
    literalcodes->table[symbol]=literalcodes->distinct;
    global_nodes[0][literalcodes->distinct] = make_node(symbol,0,LEAF);
    literalcodes->distinct++;
  }
  (*r)++;    
  c=literalcodes->table[symbol];
  /*LOG_I("Counting literal: %hd\n", literalcodes->table[symbol]);*/
  global_nodes[0][literalcodes->table[symbol]]->freq++;

}

void count_ldcodes(short length, short distance){
  huffmancoder* dlcodes= global_codingtable[1];
  int dcode;
  long* r;
  dcode = get_distance_code(distance);
  global_distancecodes[0][dlcodes->data_count++]=dcode;

  r = &dlcodes->freq[dcode]; 
  if(*r==0){
    dlcodes->table[dcode]=dlcodes->distinct;

    global_nodes[1][dlcodes->distinct] = make_node(dcode,0,LEAF);
    dlcodes->distinct++;
  }
  (*r)++;    
  global_nodes[1][dlcodes->table[dcode]]->freq++;
  count_literals(LENCODE_LOOKUP[length]);
}

int deflate(bitarray* bBuffer, char* data,size_t input_sz){

    char* input,distance_codes[WINDOW_SIZE];
    int i,j,min,f,offset,offset1,pos,dist,match_len,ptr_count,next_ptr,next_pos,input_size;
    long *r;
    short codetable[HUFFMAN_ALPHABET_SZ],sym,lcode;
    huffnode* cnodes[HUFFMAN_ALPHABET_SZ],*dnodes[29],*clnodes[18];
    slzss_pointer lzss_ptrs[WINDOW_SIZE];
    huffmancoder o_huffman,cl_huffman,d_huffman;
    bitarray* bh;
    
    bh = bBuffer;

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
    
    
    global_codingtable[0] = &o_huffman;  
    global_codingtable[1] = &d_huffman;
    global_codingtable[2] = &cl_huffman;
    
    global_nodes[0] = cnodes;
    global_nodes[1] = dnodes;
    global_nodes[2] = clnodes;
    
    global_distancecodes[0] = distance_codes;
    LOG_I("LZSS portion inputsize: %d\n",input_size);
    ptr_count = generate_lzss_pointers(input,input_size,lzss_ptrs,WINDOW_SIZE,&pos,count_ldcodes,count_literals); 
    /*Stops processing if we run out of ptr space in the stack*/
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
    packbits(bh,1,1);      /*BFINAL =1*/
    PRINT_BINARY(1,1);

    packbits(bh,2,2);      /*BTYPE = 10*/
    PRINT_BINARY(2,2);
    
    packbits(bh,HLIT,5);   
    PRINT_BINARY(HLIT,5);
    packbits(bh,HDIST,5);  
    PRINT_BINARY(HDIST,5);
    packbits(bh,HCLEN,4);  
    PRINT_BINARY(reverse_bits(HCLEN,4),4);
    LOG_I("\n\nPacked bits for code lens\n");
    
    for(i=0;i<HCLEN+4;i++){
        int cl = cl_lensbuffer[i];
        packbits(bh,cl,3);
        PRINT_BINARY(cl,3);
        LOG_I(" ");
    }
    LOG_I("\n\nPacked bits for Literal & Distance lens\n");
    
    for(i=0;i<nc;i++){
        int _sym = compressedlens[i].sym;
        int cl = cl_huffman.revcodetable[_sym]->code;
        packbits(bh,cl,cl_huffman.revcodetable[_sym]->codelen);
        PRINT_BINARY(cl,cl_huffman.revcodetable[_sym]->codelen);
        
        if(_sym>15){
            LOG_I(":[%d R%d]",_sym,compressedlens[i].repeat);
            int ebits = CODELEN_EBITS[compressedlens[i].sym];
            packbits(bh,compressedlens[i].repeat-CODELEN_EBITS_BASE[compressedlens[i].sym],ebits);
            PRINT_BINARY(compressedlens[i].repeat-CODELEN_EBITS_BASE[compressedlens[i].sym],ebits);
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
            lcode = LENCODE_LOOKUP[sym];
            packbits(bh,o_huffman.revcodetable[lcode]->code,o_huffman.revcodetable[lcode]->codelen);
            PRINT_BINARY(o_huffman.revcodetable[lcode]->code,o_huffman.revcodetable[lcode]->codelen);
            packbits(bh,sym-LENCODE_BASE[lcode-LCODEBASE],LENCODE_EBITS[lcode-LCODEBASE]);/*write length literal*/
            PRINT_BINARY(sym-LENCODE_BASE[lcode-LCODEBASE],LENCODE_EBITS[lcode-LCODEBASE]);
            LOG_I("<%d|%d ",lcode,sym);
            sym = lzss_ptrs[next_ptr].distance;
            lcode = global_distancecodes[0][next_ptr];
            LOG_I(" %d|%d>",lcode,sym);
            packbits(bh,d_huffman.revcodetable[lcode]->code,d_huffman.revcodetable[lcode]->codelen);
            PRINT_BINARY(d_huffman.revcodetable[lcode]->code,d_huffman.revcodetable[lcode]->codelen);
            packbits(bh,sym-DISTANCE_BASE[lcode],DISTANCE_LENS[lcode]);/*write distance*/
            PRINT_BINARY(sym-DISTANCE_BASE[lcode],DISTANCE_LENS[lcode]);
            i+=lzss_ptrs[next_ptr].length;
            next_pos = lzss_ptrs[++next_ptr].position;
        }else{
            packbits(bh,o_huffman.revcodetable[input[i]]->code,o_huffman.revcodetable[input[i]]->codelen);
            PRINT_BINARY(o_huffman.revcodetable[input[i]]->code,o_huffman.revcodetable[input[i]]->codelen);
            i++;
        }
    }
    LOG_I("\nEOB\n");
    packbits(bh,o_huffman.revcodetable[EOBCODE]->code,o_huffman.revcodetable[EOBCODE]->codelen);
    
    free_huffman_heap(&o_huffman,HUFFMAN_ALPHABET_SZ);
    
    free_huffman_heap(&d_huffman,30);
    
    free_huffman_heap(&cl_huffman,nc);

    return pos;
}
