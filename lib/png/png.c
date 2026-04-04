#include <stdlib.h>
#include <string.h>
#include "png.h"
#include "filter/filter.h"
#include "../huffman/huffman.h"
#include "../crc32/crc32.h"
#include "../deflate/deflate.h"
#include "../cursedhelpers.h"
#include "../zlib/zlib.h"
#include "../adler32/adler32.h"

uint32_t bswap32(uint32_t x) {
    return ((x & 0x000000FF) << 24) |
           ((x & 0x0000FF00) << 8)  |
           ((x & 0x00FF0000) >> 8)  |
           ((x & 0xFF000000) >> 24);
}

uint16_t bswap16(uint16_t x) {
    return (x << 8) | (x >> 8);
}

uint8_t* copy_header(const ihdr_chunk* header){
    uint8_t* wBuffer = malloc(13);
    uint32_t h,w;
    h = bswap32(header->height);
    w = bswap32(header->width);
    memcpy(&wBuffer[0],&w,4);
    memcpy(&wBuffer[4],&h,4);
    memcpy(&wBuffer[8],&header->bit_depth,1);
    memcpy(&wBuffer[9],&header->color_type,1);
    memcpy(&wBuffer[10],&header->compression_method,1);
    memcpy(&wBuffer[11],&header->filter_method,1);
    memcpy(&wBuffer[12],&header->interlace_method,1);
    return wBuffer;
}

png_s create_png_container(ihdr_chunk header){
    
    png_s p;
    png_chunk hdr,end,*idat;

    static const uint8_t PNG_MAGIC_BYTES[8]= PNG_MAGIC;
    
    uint8_t* hdrb;
    
    hdrb = copy_header(&header);
    uint32_t c;
    c = crc32(0xFFFFFFFF, IHDR_ID, 4);
    c = crc32(c, hdrb, 13);
    c ^= 0xFFFFFFFF; 
    
    hdr = (png_chunk){
        .LENGTH = 13,
        .CHUNK_TYPE = {0x49, 0x48, 0x44, 0x52},
        .CHUNK_DATA = hdrb,
        .CRC = c
    };
    end = (png_chunk){
        .LENGTH =0,
        .CHUNK_TYPE = {0x49, 0x45, 0x4E, 0x44},
        .CRC = crc32(0xFFFFFFFF, IEND_ID, 4) ^ 0xFFFFFFFF
    };
    p = (png_s){
        .MAGIC = {137, 80, 78, 71, 13, 10, 26, 10},
        .pihdr = hdr,
        .pidat_chunks = NULL,
        .piend = end
    };
    return p;
}


int make_idat_chunks(const ihdr_chunk* header, const uint8_t* rawpx, const size_t pxsz,idat_chunk*** idat_s){
    size_t row = 0, scanline_sz = header->width,nchunk=1,deflatedat,wdat=0,tidat;
    uint8_t *prow,*dd,*ad;
    uint8_t* upperpad_ = calloc(header->width*pxsz,1);
    uint8_t* write_b = calloc(header->height*(header->width*pxsz+1),1);
    int i=0;
    bitarray cba;
    idat_chunk* current_chunk;
    prow = upperpad_;
    dd = write_b;
    LOG_I("Creating rows witn %d pixels width=%d\n", header->width * header->height, header->width);
    for(;row<header->height;row++){
        LOG_I("Row %ld\n",row);
        filter_row(&rawpx[row*header->width*pxsz],prow,dd,header->width*pxsz,pxsz);
        prow = dd;
        dd = dd+(header->width*pxsz+1);
    }
    deflatedat = (header->height)*(header->width*pxsz+1);
    tidat = deflatedat;
    LOG_I("Create ZLIB Header\n");
    memset(&cba,0,sizeof(bitarray));

    zlib_header zlh;
    zlh.CMF = make_cmf(CINFO_DEFLATE_WINDOW,CM_DEFLATE);
    zlh.FLG = make_flg(LDEFAULT,0,zlh.CMF);
    packbytes_aligned(&cba, &zlh.CMF, 1);
    packbytes_aligned(&cba, &zlh.FLG, 1);
    ad = write_b;
    size_t t;
    uint32_t ad32,s1,s2;
    s1 = 1; s2=0;
    t = deflatedat;
    LOG_I("Calculate ADLER32 with %ld bytes\n",t);
    while(t>0){
        ad32 = adler32(*ad,&s1,&s2);
        ad++;
        t--;
    }
    LOG_I("Creating Deflate\n");
    dd = write_b;
    
    /* 1. Run deflate until all raw data is consumed into the single cba bitarray */
    do {
        LOG_I("Start Deflate pass\n");
        wdat = deflate(&cba, dd, deflatedat);
        if(wdat <= 0) {
            LOG_E("[PNG] DEFLATE failed with %ld remaining from %ld\n", deflatedat, tidat);
            break;
        }   
        deflatedat -= wdat;
        dd += wdat;
    } while(deflatedat > 0);
    
    bitarray_flush(&cba);
    LOG_I("End Deflate with total compressed len: %ld\n", cba.used);

    /* 2. Append ADLER32 checksum */
    LOG_I("AD32: %x\n", ad32);
    /* ZLIB spec requires Adler32 to be Big-Endian */
    uint32_t ad32_be = bswap32(ad32); 
    packbytes_aligned(&cba, (uint8_t*)&ad32_be, sizeof(ad32_be));

    /* 3. Slice the completed ZLIB stream into separate IDAT chunks */
    size_t max_idat_size = 32768; /* Standard chunk size (can safely be up to 65536) */
    size_t remaining_bytes = cba.used;
    uint8_t* cba_ptr = cba.data;
    nchunk = 0; /* Reset and use as a standard 0-indexed counter */

    while(remaining_bytes > 0) {
        size_t chunk_sz = (remaining_bytes > max_idat_size) ? max_idat_size : remaining_bytes;

        *idat_s = (idat_chunk**)realloc(*idat_s, (nchunk + 1) * sizeof(idat_chunk*));
        current_chunk = (idat_chunk*)calloc(1, sizeof(idat_chunk));
        (*idat_s)[nchunk] = current_chunk;

        current_chunk->sz = chunk_sz;
        /* Allocate isolated memory for this chunk to prevent double-frees later */
        current_chunk->data = (uint8_t*)malloc(chunk_sz);
        memcpy(current_chunk->data, cba_ptr, chunk_sz);

        cba_ptr += chunk_sz;
        remaining_bytes -= chunk_sz;
        nchunk++;
    }

    LOG_I("Wrote %ld bytes to deflate stream across %ld IDAT chunks\n", tidat, nchunk);

    /* Clean up the temporary bitarray buffer since we copied the data out */
    if (cba.data) {
        free(cba.data);
    }

    free(upperpad_);
    free(write_b);
    return nchunk;
}

void encapsulate_idatchunks(png_s* p,idat_chunk** chunks, size_t nchunks){
    int i = 0;
    p->pidat_chunks = realloc(p->pidat_chunks,sizeof(png_chunk*)*nchunks);
    for(i=0;i<nchunks;i++){
        p->pidat_chunks[i] = calloc(1,sizeof(png_chunk));
        memcpy(&p->pidat_chunks[i]->CHUNK_TYPE,IDAT_ID,4);
        p->pidat_chunks[i]->LENGTH = chunks[i]->sz;
        p->pidat_chunks[i]->CHUNK_DATA = chunks[i]->data;
        p->pidat_chunks[i]->CRC = crc32(0xFFFFFFFF,IDAT_ID,4);
        p->pidat_chunks[i]->CRC = crc32(p->pidat_chunks[i]->CRC,(uint8_t*)(p->pidat_chunks[i]->CHUNK_DATA),p->pidat_chunks[i]->LENGTH);
        p->pidat_chunks[i]->CRC ^= 0xFFFFFFFF;
    }
}
static void write_chunk(FILE* f, png_chunk* c) {
    uint32_t len_be = bswap32(c->LENGTH);
    uint32_t type_be = *(uint32_t*)c->CHUNK_TYPE;

    fwrite(&len_be, 4, 1, f);
    fwrite(&type_be, 4, 1, f);

    if (c->LENGTH > 0 && c->CHUNK_DATA) {
        fwrite(c->CHUNK_DATA, c->LENGTH, 1, f);
    }
    uint32_t crc_be = bswap32(c->CRC);
    fwrite(&crc_be, 4, 1, f);
}

int write_png(const char* filename, png_s* p){
    size_t j;
    FILE* f = fopen(filename, "wb");
    if (!f) {
        perror("fopen");
        return 1;
    }
    fwrite(p->MAGIC,8,1,f);
    /* IHDR */
    write_chunk(f, &p->pihdr);
    /* IDAT(s) */
    for (j = 0; j < p->n_idatchunks; j++) {
        write_chunk(f, p->pidat_chunks[j]);
    }

    /* IEND */
    write_chunk(f, &p->piend);
    fclose(f);
    return 0;
}

png_s* create_png(const ihdr_chunk* header, const uint8_t* rawpx, const size_t pxsz){
    png_s* p = malloc(sizeof(png_s));
    *p = create_png_container(*header);
    idat_chunk** idat_c_ptrs = NULL;
    p->n_idatchunks = make_idat_chunks(header, rawpx, pxsz, &idat_c_ptrs);
    encapsulate_idatchunks(p, idat_c_ptrs, p->n_idatchunks);
    return p;
}

int free_png(png_s* p){
    size_t j;
    if(!p) return 1;
    free(p->pihdr.CHUNK_DATA);
    for (j = 0; j < p->n_idatchunks; j++) {
        free(p->pidat_chunks[j]->CHUNK_DATA);
        free(p->pidat_chunks[j]);
    }
    free(p->pidat_chunks);
    free(p);
    return 0;
}
#ifdef TEST_PNGDRIVER
int main(int argv, char** argc){
    ihdr_chunk ihdr;
    uint8_t* px;
    size_t i,h,w;
    png_s* png;
    h = 400; w = 400;
    px = (uint8_t*)calloc(4*h*w, sizeof(uint8_t));
    /*Make Red*/
    LOG_I("Calloc Raw bytes\n");
    for(i=0;i<h*w;i++){
            /* Indexing: [R, G, B, A]*/
        px[i*4]     = i*96979%0xFF; /* Red*/           
        px[i*4 + 1] = i*w*104729<<i/h%0xFF;               /* Green*/
        px[i*4 + 2] = i/w/2;               /* Blue*/
        px[i*4 + 3] = 0xFF; /* Alpha (MUST be set to see the color)*/
    }
    ihdr = IHDR_TRUECOLOR8_A8(w, h);
    png = create_png(&ihdr, (uint8_t*)px, 4);
    if (!png) {
        LOG_E("Failed to create PNG\n");
        return 1;
    }
    if (write_png(argc[1], png) != 0) {
        free_png(png);
        return 1;
    }
    free_png(png);
    return 0;   
}
#endif
#ifdef STANDALONE_PNG
int main(int argv, char** argc){
    
    ihdr_chunk ihdrc;
    idat_chunk** idat_c_ptrs;
    uint8_t* raw_px;
    uint16_t* px;
    size_t h=400,w=400,j;
    int i;
    FILE* f;
    png_s p;
    LOG_I("Try Calloc Raw bytes\n");

    px = (uint16_t*)calloc(4*h*w, sizeof(uint16_t));
    /*Make Red*/
    raw_px = (uint8_t*)px;
    LOG_I("Calloc Raw bytes\n");
    for(i=0;i<h*w;i++){
            /* Indexing: [R, G, B, A]*/
        px[i*4]     = i*96979%0xFFFF; /* Red*/           
        px[i*4 + 1] = i*w*104729<<i/h%0xFFFF;               /* Green*/
        px[i*4 + 2] = i/w/2;               /* Blue*/
        px[i*4 + 3] = 0xFFFF; /* Alpha (MUST be set to see the color)*/
    }
    LOG_I("Creating Header\n");
    ihdrc = IHDR_TRUECOLOR16_A16(w,h);
    LOG_I("Wrote Header\n");
    p = create_png_container(ihdrc);
    LOG_I("Writing chunks\n");
    idat_c_ptrs = NULL;
    p.n_idatchunks = make_idat_chunks(&ihdrc,raw_px,8,&idat_c_ptrs);
    LOG_I("Writing chunks in containers\n");
    encapsulate_idatchunks(&p,idat_c_ptrs,p.n_idatchunks);
    /* ---- WRITE TO FILE ---- */
    f = fopen("out.png", "wb");
    if (!f) {
        perror("fopen");
        return 1;
    }
    fwrite(p.MAGIC,8,1,f);
    /* IHDR */
    write_chunk(f, &p.pihdr);
    /* IDAT(s) */
    for (j = 0; j < p.n_idatchunks; j++) {
        write_chunk(f, p.pidat_chunks[j]);
    }

    /* IEND */
    write_chunk(f, &p.piend);
    fclose(f);

    LOG_I("PNG written to out.png\n");

    /* cleanup */
    free(p.pihdr.CHUNK_DATA);
    free(raw_px);
    for (j = 0; j < p.n_idatchunks; j++) {
        free(p.pidat_chunks[j]->CHUNK_DATA);
        free(p.pidat_chunks[j]);
        free(idat_c_ptrs[j]);
    }
    free(p.pidat_chunks);
    free(idat_c_ptrs);

    return 0;

}
#endif