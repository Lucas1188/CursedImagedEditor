#include "../lib/cursedhelpers.h"  
#include "../lib/gzip/gzip.h" 
#include "../lib/deflate/deflate.h"

int compress_text(char* data,size_t input_sz,bitarray* bBuffer){
    int wrote_b = 0,total_written = 0,i=0;
    while(total_written<input_sz){
        wrote_b = deflate(bBuffer,data,input_sz);
        if(wrote_b<0){
            LOG_E("Deflate error\n");
            return 0;
        }
        LOG_V("Deflate Blk [%d]: Processed %d bytes\n",i,wrote_b);
        total_written += wrote_b;
        i++;
    }
    bitarray_flush(bBuffer);
    return bBuffer->used;
}

int main(int argv, char* argc[]){
    /*Text mode only*/
    int sz = 0;
    bitarray bBuffer;
    FILE* wf;
    if(argv<2){
        printf("Usage: %s <input string>\n",argc[0]);
        return 1;
    }
    LOG_I("Input [1]: %s\n",argc[1]);
    LOG_I("Input [2]: %s\n",argc[2]);

    if(argc[1][0]=='-'){
        if(argc[1][1] == 'f'){
            LOG_I("Writing GZIP file [%c]\n",argc[1][1]);
            if(write_gzip_from_file(argc[2],&bBuffer)>0){
                bitarray_flush(&bBuffer);
                wf = fopen(argc[3],"wb");
                if(!wf){
                    LOG_E("Failed to open output file: %s\n",argc[3]);
                    return 1;
                }
                fwrite(bBuffer.data,1,bBuffer.used,wf);
                fclose(wf);
                LOG_I("GZIP file written successfully to: %s\n",argc[3]);
            }

        }
    }else{
        while(argc[1][sz++]);
        if(!compress_text(argc[1],sz,&bBuffer)){
            LOG_E("Compression failed\n");
            return 1;
        }
    }   
    if(bBuffer.used){
        free(bBuffer.data);
    }
    LOG_V("Compressed data (%ld bytes):\n", bBuffer.used);
    return 0;
}