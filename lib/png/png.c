#include <stdlib.h>
#include <string.h>
#include "png.h"
#include "filter/filter.h"
#include "../huffman/huffman.h"
#include "../crc32/crc32.h"
#include "../deflate/deflate.h"
#include "../cursedhelpers.h"

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
    
    hdr = (png_chunk){
        .LENGTH = 13,
        .CHUNK_TYPE = {0x49, 0x48, 0x44, 0x52},
        .CHUNK_DATA = hdrb,
        .CRC = crc32(0xFFFFFFFF,hdrb,13)
    };
    end = (png_chunk){
        .LENGTH =0,
        .CHUNK_TYPE = {0x49, 0x45, 0x4E, 0x44}
    };
    p = (png_s){
        .MAGIC = {137, 80, 78, 71, 13, 10, 26, 10},
        .pihdr = hdr,
        .pidat_chunks = NULL,
        .piend = end
    };
    return p;
}


int make_idat_chunks(const ihdr_chunk* header, const uint8_t* rawpx, const pxsz,idat_chunk*** idat_s){
    size_t row = 0, scanline_sz = header->width,nchunk=1,deflatedat,wdat=0,tidat;
    uint8_t *prow,*dd;
    uint8_t* upperpad_ = calloc(header->width*pxsz,1);
    uint8_t* write_b = calloc(header->height*(header->width+1)*pxsz,1);
    int i=0;
    bitarray cba;
    prow = upperpad_;
    dd = write_b;
    LOG_I("Creating rows\n");
    for(;row<header->height;row++){
        dd = dd+(row*(header->width+1));
        filter_row(&rawpx[row*header->width],prow,dd,header->width,pxsz);
        prow = dd+1;
    }
    deflatedat = (header->height)*(header->width+1)*pxsz;
    tidat = deflatedat;
    LOG_I("Creating Deflate\n");
    dd = write_b;
    do{
        (*idat_s) = realloc((*idat_s),nchunk*sizeof(idat_chunk*));
        LOG_I("Realloc idat_s with chunk n = %ld\n",nchunk);

        (*idat_s)[nchunk-1] = calloc(sizeof(idat_chunk),1);

        LOG_I("Start Deflate\n");
        wdat = deflate(&cba,dd,deflatedat);
        if(wdat<=0){
            LOG_E("[PNG] DEFLATE failed with %ld remaining from %ld\n",deflatedat,tidat);
            break;
        }   
        bitarray_flush(&cba);
        (*idat_s)[nchunk-1]->sz = cba.used;
        (*idat_s)[nchunk-1]->data = cba.data;
        memset(&cba,0,sizeof(bitarray));
        dd += wdat;
        deflatedat-=wdat;
        nchunk++;
    }while(deflatedat>0);
    LOG_I("Wrote %ld bytes to deflate stream with %ld chunks",tidat,nchunk-1);
    free(upperpad_);
    free(write_b);
    return nchunk-1;
}

void encapsulate_idatchunks(png_s* p,idat_chunk** chunks, size_t nchunks){
    int i = 0;
    p->pidat_chunks = realloc(p->pidat_chunks,sizeof(png_chunk*)*nchunks);
    for(i=0;i<nchunks;i++){
        p->pidat_chunks[i] = calloc(1,sizeof(png_chunk));
        memcpy(&p->pidat_chunks[i]->CHUNK_TYPE,IDAT_ID,4);
        p->pidat_chunks[i]->LENGTH = chunks[i]->sz;
        p->pidat_chunks[i]->CHUNK_DATA = chunks[i]->data;
        p->pidat_chunks[i]->CRC = crc32(0xFFFFFFFF,(uint8_t*)&(p->pidat_chunks[i]->CHUNK_TYPE),4);
        p->pidat_chunks[i]->CRC = crc32(p->pidat_chunks[i]->CRC,(uint8_t*)&(p->pidat_chunks[i]->CHUNK_DATA),p->pidat_chunks[i]->LENGTH);

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
        px[i*4] = 1<<15;
    }
    LOG_I("Creating Header\n");
    ihdrc = IHDR_TRUECOLOR16_A16(100,100);
    LOG_I("Wrote Header\n");
    p = create_png_container(ihdrc);
    LOG_I("Writing chunks\n");
    idat_c_ptrs = NULL;
    p.n_idatchunks = make_idat_chunks(&ihdrc,raw_px,100*100,&idat_c_ptrs);
    LOG_I("Writing chunks in containers\n");
    encapsulate_idatchunks(&p,idat_c_ptrs,p.n_idatchunks);
    /* ---- WRITE TO FILE ---- */
    f = fopen("out.png", "wb");
    if (!f) {
        perror("fopen");
        return 1;
    }
    fwrite(p.MAGIC,8,1,f);
    write_chunk(f,&p.pihdr);
    /* IHDR */
    write_chunk(f, &p.pihdr);

    /* IDAT(s) */
    for (j = 0; j < p.n_idatchunks; j++) {
        write_chunk(f, p.pidat_chunks[j]);
    }

    /* IEND */
    write_chunk(f, &p.piend);
    fclose(f);

    printf("PNG written to out.png\n");

    /* cleanup */
    free(raw_px);
    for (j = 0; j < p.n_idatchunks; j++) {
        free(p.pidat_chunks[j]->CHUNK_DATA);
        free(p.pidat_chunks[j]);
    }
    free(p.pidat_chunks);

    return 0;

}
#endif