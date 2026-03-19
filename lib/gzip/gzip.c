#include "gzip.h"
#include "../deflate/deflate.h"
#include "../cursedhelpers.h"

int crc32(int crc, const unsigned char *buf, size_t len) {
    static long table[256];
    static int have_table = 0;
    int rem;
    int i, j;
    if (!have_table) {
        for (i = 0; i < 256; i++) {
            rem = i;
            for (j = 0; j < 8; j++) {
                if (rem & 1) {
                    rem = (rem >> 1) ^ 0xEDB88320;
                } else {
                    rem >>= 1;
                }
            }
            table[i] = rem;
        }
        have_table = 1;
    }
    crc = ~crc;
    while (len--) {
        crc = (crc >> 8) ^ table[(crc & 0xFF) ^ *buf++];
    }
    return ~crc;
}

int get_file_crc_fptr_fsz(const char* filename,FILE** f_out,int* size_out){
    LOG_I("Calculating CRC32 for file: %s\n", filename);
    *f_out = fopen(filename,"rb");
    if(!f_out) return -1;
    unsigned char buf[4096];
    size_t read;
    int crc = 0;
    while((read = fread(buf,1,sizeof(buf),*f_out))>0){
        /*LOG_I("Reading file in chunks to calculate CRC32...%x\n",crc);*/
        crc = crc32(crc,buf,read);
    }
    *size_out = ftell(*f_out);
    LOG_V("Done CRC: %x ,sz: %d\n", crc,*size_out); 
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

void make_gzip_footer(gzip_footer* footer, int crc_chksum, size_t size) {
    footer->crc32 = crc_chksum;
    footer->isize = size & 0xFFFFFFFF; /* only lower 32 bits */
}

int write_gzip_from_file(const char* filename, bitarray* bData) {
    long crc_cs,processed;
    gzip_header header;
    gzip_footer footer;
    FILE* f;
    char* fbuffer;
    int file_sz;
    make_gzip_header(&header);
    packbytes_aligned(bData, (unsigned char*)&header, 10);

    crc_cs = get_file_crc_fptr_fsz(filename, &f, &file_sz);
    if (!f) {
        LOG_E("Failed to open file for reading: %s\n", filename);
        return -1;
    }
    rewind(f);
    
    fbuffer = malloc(file_sz);
    LOG_I("Alloc fbuffer [%d]\n", file_sz);
    
    if (!fbuffer) {
        LOG_E("Failed to allocate memory for file buffer\n");
        fclose(f);
        return -1;
    }
    LOG_I("Allocated %d bytes for file buffer\n", file_sz);
    if (fread(fbuffer, 1, file_sz, f) != (size_t)file_sz) {
        LOG_E("Failed to read file into buffer\n");
        free(fbuffer);
        fclose(f);
        return -1;
    }
    fclose(f);
    processed = deflate(bData, (char*)fbuffer, file_sz);
    if (processed < 0) {
        LOG_E("Compression failed\n");
        free(fbuffer);
        return -1;
    }
    bitarray_flush(bData);
    make_gzip_footer(&footer,crc_cs ,file_sz);
    packbytes_aligned(bData, (unsigned char*)&footer, sizeof(footer));

    free(fbuffer);
    return bData->used;
}