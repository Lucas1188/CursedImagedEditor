#include <stdio.h>
#include "../lib/base64encoder/b64tbl.h"
#include "../cursedhelpers.h"
int main(int argv, char** argc){
  int i,j;
  size_t fsize, bsize, wsize, remainder, blocks, pad;
  unsigned char* str, *encodebuffer,*fbuffer;
  /*
  readstring_to_mem(argc[1],&str,&fsize);
  */
  
  readfile_to_mem(argc[1],&str,&fsize);
  
  _getb64size(fsize,&bsize,&remainder,&blocks);
  encodebuffer = (unsigned char*) malloc(bsize);
  
  LOG_I(("Got string size %ld allocated %ld bytes for encoding with %ld blocks remainder: %ld\n",fsize,bsize,blocks,remainder));
  
  for(i=0;i<fsize/3;i++){
  /*
    printf("Remapping block: %d\n",i);
  */
    remap3bytes(&str[i*3],&encodebuffer[i*4],3);
  }
  /*
  printf("Remap Complete!\n");
  */
  if(remainder!=0){
    remap3bytes(&str[fsize-remainder],&encodebuffer[bsize-5],remainder);
  } 
  encodebuffer[bsize-1] = '\0';
  /*
  printf("\nResult:\n%s\n<end>\n%ld\n",encodebuffer,strlen(encodebuffer));
  */
  free(str);
  /*
    Decode
  */
    
  _getb64decodesize(encodebuffer, bsize, &wsize, &blocks, &pad);
  LOG_I(("Got file size %ld allocated %ld bytes for decoding with %ld blocks pad: %ld\n",bsize,wsize,blocks,pad));
  fbuffer = malloc(wsize);
  for(j=0;j<blocks;j++){
    remap4bytes(&encodebuffer[j*4], &fbuffer[j*3]);
  }
  /*
  str[wsize]='\0';
  printf("Result: %s\n",str);
  */
  
  writefile_from_mem(argc[2],fbuffer,wsize);
  
  free(encodebuffer);
  free(fbuffer);
  return 0;
}
