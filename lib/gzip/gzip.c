#include "gzip.h"
#include "../deflate/deflate.h"
#include "../cursedhelpers.h"
#include "../crc32/crc32.h"

uint32_t get_file_crc_fptr_fsz(const char* filename,FILE** f_out,size_t* size_out){
    LOG_I("Calculating CRC32 for file: %s\n", filename);
    *f_out = fopen(filename,"rb");
    if(!*f_out){LOG_E("File ptr closed without warning!\n");};
    uint8_t buf[4096];
    size_t read;
    size_t total =0;
    uint32_t crc = 0xFFFFFFFF;
    while((read = fread(buf,1,sizeof(buf),*f_out))>0){
        /*LOG_I("Reading file in chunks to calculate CRC32...%x\n",crc);*/
        crc = crc32(crc,buf,read);
        total+=read;
    }
    crc = crc ^ 0xFFFFFFFF;
    *size_out = total;
    LOG_V("Done CRC: %x ,sz: %ld\n", crc,*size_out); 
    return crc;
}
void make_gzip_header(gzip_header* header) {
    header->id1 = ID1;
    header->id2 = ID2;
    header->cm = CM_DEFLATE;
    header->flg = 0;
    header->mtime = 0;
    header->xfl = 0;
    header->os = OS_UNIX;
}

void make_gzip_footer(gzip_footer* footer, uint32_t crc_chksum, size_t size) {
    footer->crc32 = crc_chksum;
    footer->isize = size & 0xFFFFFFFF; /* only lower 32 bits */
}

uint32_t write_gzip_from_file(const char* filename, bitarray* bData) {
    long processed;
    gzip_header header;
    gzip_footer footer;
    FILE* f;
    uint8_t* fbuffer;
    uint32_t crc_cs;
    size_t file_sz;
    make_gzip_header(&header);
    packbytes_aligned(bData, (uint8_t*)&header, 10);

    crc_cs = get_file_crc_fptr_fsz(filename, &f, &file_sz);
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
    processed = deflate(bData, (uint8_t*)fbuffer, file_sz);
    if (processed < 0) {
        LOG_E("Compression failed\n");
        free(fbuffer);
        return -1;
    }
    bitarray_flush(bData);
    make_gzip_footer(&footer, crc_cs ,file_sz);
    packbytes_aligned(bData, (uint8_t*)&footer, sizeof(footer));

    free(fbuffer);
    return bData->used;
}