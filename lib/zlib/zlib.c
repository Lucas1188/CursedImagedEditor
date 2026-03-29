#include <stdio.h>

#include "zlib.h"
#include "../bithelper/bithelpers.h"
#include "../adler32/adler32.h"
#include "../deflate/deflate.h"
#include "../cursedhelpers.h"

uint32_t get_file_ad32_fptr_fsz(const char* filename,FILE** f_out,size_t* size_out){
    size_t t,i;
    uint32_t ad32,s1,s2;
    s1 = 1; s2=0; t=0;

    LOG_I("Calculating AD32 for file: %s\n", filename);
    *f_out = fopen(filename,"rb");
    if(!*f_out){LOG_E("File ptr closed without warning!\n");};
    uint8_t buf[4096];
    size_t read;
    size_t total =0;
    
    while((read = fread(buf,1,sizeof(buf),*f_out))>0){
        for(i=0;i<read;i++){
            ad32 = adler32(buf[i],&s1,&s2);
        }
        t+=read;
    }
    
    *size_out = t;
    LOG_V("Done AD32: %x ,sz: %ld\n", ad32,*size_out); 
    return ad32;
}

uint8_t make_cmf(uint8_t CINFO, uint8_t CM){
    return CINFO<<4 | CM;
}

uint8_t make_flg(uint8_t FLVL, uint8_t FDICT_PRESENT, uint8_t CMF){
    uint8_t flg,fchk;
    uint16_t cs;
    flg = (FLVL&3)<<6|(FDICT_PRESENT&1<<5);
    cs = CMF<<8|flg;
    fchk = 31-(cs%31);
    flg = fchk==0?flg:flg|fchk;
    return flg;
}

int write_zlib_from_file(const char* filename, bitarray* bData){
    long processed;
    zlib_header header;
    FILE* f;
    uint8_t* fbuffer;
    uint32_t ad32_cs;
    size_t file_sz;

    ad32_cs = get_file_ad32_fptr_fsz(filename, &f, &file_sz);
    if (!f) {
        LOG_E("Failed to open file for reading: %s\n", filename);
        return -1;
    }
    rewind(f);
    
    fbuffer = malloc(file_sz);
    LOG_I("Alloc fbuffer [%ld]\n", file_sz);
    
    if (!fbuffer) {
        LOG_E("Failed to allocate memory for file buffer\n");
        fclose(f);
        return -1;
    }
    LOG_I("Allocated %ld bytes for file buffer\n", file_sz);
    if (fread(fbuffer, 1, file_sz, f) != (size_t)file_sz) {
        LOG_E("Failed to read file into buffer\n");
        free(fbuffer);
        fclose(f);
        return -1;
    }
    fclose(f);
    header.CMF = make_cmf(CINFO_DEFLATE_WINDOW,CM_DEFLATE);
    header.FLG = make_flg(LDEFAULT,0,header.CMF);

    packbytes_aligned(bData,(uint8_t*)&header,sizeof(zlib_header));

    processed = deflate(bData, (uint8_t*)fbuffer, file_sz);
    if (processed < 0) {
        LOG_E("Compression failed\n");
        free(fbuffer);
        return -1;
    }
    bitarray_flush(bData);
    LOG_I("AD32: %x\n",ad32_cs);
    packbytes_aligned(bData,(uint8_t*)&ad32_cs,sizeof(ad32_cs));

    free(fbuffer);
    return bData->used;
}

int write_zlib_from_buf(const uint8_t* data, size_t sz, bitarray* bData)
{
    zlib_header header;
    uint32_t    s1, s2, adler;
    uint8_t     adler_be[4];
    size_t      i;
    long        processed;

    /* Compute ADLER32 of the uncompressed data (big-endian in ZLIB trailer) */
    s1 = 1; s2 = 0;
    for(i = 0; i < sz; i++) adler32(data[i], &s1, &s2);
    adler = (s2 << 16) | s1;

    header.CMF = make_cmf(CINFO_DEFLATE_WINDOW, CM_DEFLATE);
    header.FLG = make_flg(LDEFAULT, 0, header.CMF);
    packbytes_aligned(bData, (uint8_t*)&header, sizeof(zlib_header));

    processed = deflate(bData, (uint8_t*)data, sz);
    if(processed < 0) {
        LOG_E("write_zlib_from_buf: deflate failed\n");
        return -1;
    }
    bitarray_flush(bData);

    /* ADLER32 written big-endian as required by RFC 1950 */
    adler_be[0] = (uint8_t)(adler >> 24);
    adler_be[1] = (uint8_t)(adler >> 16);
    adler_be[2] = (uint8_t)(adler >>  8);
    adler_be[3] = (uint8_t)(adler);
    packbytes_aligned(bData, adler_be, 4);

    return (int)bData->used;
}

#ifdef STANDALONE_ZLIB

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
            LOG_I("Writing ZLIB Block [%c]\n",argc[1][1]);
            if(write_zlib_from_file(argc[2],&bBuffer)>0){
                wf = fopen(argc[3],"wb");
                if(!wf){
                    LOG_E("Failed to open output file: %s\n",argc[3]);
                    return 1;
                }
                fwrite(bBuffer.data,1,bBuffer.used,wf);
                fclose(wf);
                LOG_I("ZLIB Block written successfully to: %s\n",argc[3]);
            }
        }
    }
    if(bBuffer.used){
        free(bBuffer.data);
    }
    LOG_V("Compressed data (%ld bytes):\n", bBuffer.used);
    return 0;
}
#endif