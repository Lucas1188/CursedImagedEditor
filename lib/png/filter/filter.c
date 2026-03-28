#include "filter.h"
#include "../../cursedhelpers.h"

typedef int64_t (* write_filter)(const uint8_t* row, uint8_t* prev_row, uint8_t* wBuffer,const size_t scanline_sz,const size_t pix_sz);

int64_t none_func(const uint8_t* row, uint8_t* prev_row, uint8_t* wBuffer,const size_t scanline_sz,const size_t pix_sz){
    int i;
    int64_t sum;
    for(i=0;i<scanline_sz;i++){
        sum += row[i];
    }
    return sum; 
}
int64_t sub_func(const uint8_t* row, uint8_t* prev_row, uint8_t* wBuffer,const size_t scanline_sz,const size_t pix_sz){
    int i;
    int64_t sum;
    uint8_t* refByte;
    uint64_t leftpad =0;
    wBuffer[0] = FILTER_SUB;
    refByte = &((uint8_t*)&leftpad)[8-pix_sz];
    for(i=0;i<scanline_sz;i++){
        wBuffer[i+1] = row[i] - *refByte;
        sum += wBuffer[i+1];
        refByte++;
    }
    return sum; 
}
int64_t up_func(const uint8_t* row, uint8_t* prev_row, uint8_t* wBuffer,const size_t scanline_sz,const size_t pix_sz){
    int i;
    int64_t sum;
    uint8_t* refByte;
    uint64_t leftpad =0;
    wBuffer[0] = FILTER_UP;
    for(i=0;i<scanline_sz;i++){
        wBuffer[i+1] = row[i] - prev_row[i];
        sum += wBuffer[i+1];
    }
    return sum; 
}

int64_t avg_func(const uint8_t* row, uint8_t* prev_row, uint8_t* wBuffer,const size_t scanline_sz,const size_t pix_sz){
    int i;
    int64_t sum;
    uint8_t* refByte;
    uint64_t leftpad =0;
    wBuffer[0] = FILTER_AVERAGE;
    refByte = &((uint8_t*)&prev_row)[8-pix_sz];
    for(i=0;i<scanline_sz;i++){
        wBuffer[i+1] = row[i] - (((uint16_t)*refByte+(uint16_t)prev_row[i])>>1);
        sum += wBuffer[i+1];
        refByte++;
    }
    return sum; 
}

uint8_t* paeth_pred(uint8_t* a,uint8_t* b,uint8_t* c){
    uint16_t _a,_b, _c;
    int p,_pa,_pb,_pc;
    _a  = *a;
    _b  = *b;
    _c  = *c;
    p = _a +_b-_c;
    _pa = abs(p-_a);
    _pb = abs(p-_b);
    _pc = abs(p-_c);
    if(_pa<=_pb && _pa<=_pc) return a;
    if(_pb<=_pc) return b;
    return c;
}

int64_t paeth_func(const uint8_t* row, uint8_t* prev_row, uint8_t* wBuffer,const size_t scanline_sz,const size_t pix_sz){
    int i;
    int64_t sum;
    uint8_t* refByte,*prevByte,*pRefByte;
    uint64_t leftpad =0;
    wBuffer[0] = FILTER_PAETH;
    refByte = &((uint8_t*)&prev_row)[8-pix_sz];

    for(i=0;i<scanline_sz;i++){
        pRefByte = i<pix_sz? (uint8_t*)&leftpad:&prev_row[i-pix_sz];
        wBuffer[i+1] = row[i] - *paeth_pred(refByte,&prev_row[i],pRefByte);
        sum += wBuffer[i+1];
        refByte++;
    }
    return sum; 
}
const write_filter filter_funcs[5] = {
    none_func,
    sub_func,
    up_func,
    avg_func,
    paeth_func
};

uint8_t* filter_row(const uint8_t* row, uint8_t* prev_row, uint8_t* wBuffer, const size_t scanline_sz,const size_t pix_sz){
    int i,j,sum,min_fil,nsum;
    uint8_t* tmp;
    
    min_fil = 0;
    sum = filter_funcs[FILTER_NONE](row,prev_row,wBuffer,scanline_sz,pix_sz);
    for(i = FILTER_NONE+1;i<=FILTER_PAETH;i++){
        nsum = filter_funcs[i](row,prev_row,wBuffer,scanline_sz,pix_sz);
        if(nsum<sum){
            sum = nsum;
            min_fil = i;
        }
    }
    filter_funcs[min_fil](row,prev_row,wBuffer,scanline_sz,pix_sz);
    return wBuffer;
}



