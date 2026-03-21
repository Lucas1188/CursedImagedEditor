#include "adler32.h"
#include <stdio.h>

uint32_t adler32(uint8_t byte, uint32_t* s1, uint32_t* s2){
    uint32_t l,z;
    uint8_t* ll,*zz;
    const uint32_t m = 65521;
    *s1 =  (*s1+byte)%m;
    *s2 =  (*s1+*s2)%m;
    l = (*s2<<16) | *s1;
    ll = (uint8_t*)&l;
    zz = (uint8_t*)&z;
    zz[0] = ll[3];
    zz[1] = ll[2];
    zz[2] = ll[1];
    zz[3] = ll[0];
    return z;
} 

#ifdef STANDALONE_AD32

int main(int argv, char** argc){
    FILE* f;
    uint8_t buffer[4096];
    int read,i,t;
    uint32_t ad32,s1,s2;
    s1 = 1; s2=0; t=0;
    if(argv<2) return 1;
    f = fopen(argc[1],"rb");
    if(!f){
        return 1;
    }
    while((read=fread(buffer,1,sizeof(buffer),f))>0){
        for(i=0;i<read;i++){
            ad32 = adler32(buffer[i],&s1,&s2);
        }
        t+=read;
    }
    printf("AD32: 0x%x\nRead: %d\n",ad32,t);
    return 0;
}
#endif