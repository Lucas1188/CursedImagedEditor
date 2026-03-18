#include "../lib/cursedhelpers.h"   
#include "../lib/deflate/deflate.h"

int compress_text(char* data,size_t input_sz,bitarray* bBuffer){
  return deflate(bBuffer,data,input_sz);
}

int main(int argv, char* argc[]){
    /*Text mode only*/
    int sz = 0;
    bitarray bBuffer;
    if(argv<2){
        printf("Usage: %s <input string>\n",argc[0]);
        return 1;
    }

    while(argc[1][sz]) sz++;
    
    if(!compress_text(argc[1],sz,&bBuffer)){
        LOG_E("Compression failed\n");
        return 1;
    }
    LOG_V("Compressed data (%zu bytes):\n", bBuffer.used);
    return 0;
}