#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "b64tbl.h"

typedef enum RCODES{
  OK=1,
  ERROR=2,
  INPUT_ERROR=4,
  INVALID_FILENAME=8,
  FILE_ERROR=16,
  INSUF_MEM = 32,
  INVALID_B64 = 64,
  BAD_WRITE = 128,
}RCODES;

static int remap3bytes(const unsigned char* readbytes, unsigned char* writebuffer,size_t input_size){
  unsigned char b1,b2;
  if(input_size>3) return INPUT_ERROR;
  b1 = input_size>1?readbytes[1]>>4:0;
  b2 = input_size>2?readbytes[2]>>6:0;
  writebuffer[0] = B64URL_TABLE[readbytes[0]>>2];
  writebuffer[1] = B64URL_TABLE[((readbytes[0]&3)<<4)|b1];
  writebuffer[2] = input_size>1?  B64URL_TABLE[((readbytes[1]&15)<<2)|b2] : B64PAD;
  writebuffer[3] = input_size>2?  B64URL_TABLE[readbytes[2]&63] : B64PAD;
  return OK;
}
static int remap4bytes(const unsigned char* readbytes, unsigned char* writebuffer){
  int b3,b4;
  
  b3 = readbytes[2] == B64PAD;
  b4 = readbytes[3] == B64PAD;
  
  writebuffer[0] = (B64URL_REV_TABLE[readbytes[0]]<<2)|(B64URL_REV_TABLE[readbytes[1]]>>4);
  if(!b3){
    writebuffer[1] = (B64URL_REV_TABLE[readbytes[1]]<<4)|(B64URL_REV_TABLE[readbytes[2]]>>2);
    if(!b4){
      writebuffer[2] = (B64URL_REV_TABLE[readbytes[2]]<<6)|(B64URL_REV_TABLE[readbytes[3]]);  
    }
  }
  return OK;
}

int readstring_to_mem(const char* str, unsigned char** buffer, size_t* size_out){
  size_t s = 0;
  char c;
  while(1){
    c = str[s];
    s++;
    if(c=='\0') break;
  }
  *buffer = malloc(s);
  memcpy(*buffer,str,s);
  *size_out = s-1;
  return OK;
}

int readfile_to_mem(const char* filename, unsigned char** buffer,size_t* size_out){
  FILE* f;
  long size;
  if(filename== NULL) return ERROR|INVALID_FILENAME;
  f = fopen(filename, "rb");
  if (!f) return ERROR|INVALID_FILENAME;

  /* Seek to end */
  if (fseek(f, 0, SEEK_END) != 0) {
      fclose(f);
      return ERROR|FILE_ERROR;
  }

  size = ftell(f);
  if (size < 0) {
      fclose(f);
      return ERROR|FILE_ERROR;
  }

  rewind(f);
  if(*buffer!=NULL) free(*buffer);
  *buffer = malloc(size);
  if (!buffer) {
      fclose(f);
      return ERROR|INSUF_MEM;
  }

  size_t read = fread(*buffer, sizeof(**buffer), size, f);
  fclose(f);

  if (read != (size_t)size) {
      free(*buffer);
      return ERROR;
  }

  *size_out = size;
  return OK;
}
int writefile_from_mem(const char* filename, const unsigned char* buffer,size_t size){
  FILE* f;
  size_t n;
  f = fopen(filename,"wb");
  n = fwrite(buffer,sizeof(char),size,f);
  fclose(f);
  printf("Wrote %ld bytes to file: %s\n",n,filename);
  if(n!=size){ return ERROR|BAD_WRITE;}
  else{ return OK;}
}

int _getb64size(long fsize,long* bsize_out, long* remainder_out, long* blocks_out){
  *remainder_out = fsize%3;
  *blocks_out = fsize/3+(*remainder_out>0?1:0);
  *bsize_out = fsize/3*4+(fsize%3>0?4:0)+1;
  return OK;
}

int _getb64decodesize(const unsigned char* b64, long fsize, long* wsize_out, long* blocks_out, long* pad_out){
  if((fsize-1)%4!=0) return ERROR| INVALID_B64;
  *blocks_out = (fsize-1)/4;
  *pad_out = b64[fsize-2]==B64PAD?b64[fsize-3]==B64PAD?2:1:0;
  *wsize_out = *blocks_out*3-*pad_out;
  return OK;
}

