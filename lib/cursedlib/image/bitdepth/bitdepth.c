
#include "bitdepth.h"

const uint8_t BITD_BYTE_BOUNDARY[16]= {
    0,  1,  1,  1,  1,  1,  1,  1,  
    1,  2,  2,  2,  2,  2,  2,  2
};

unsigned int max(unsigned int lhs, unsigned int rhs){
    return lhs>=rhs?lhs:rhs;
}
unsigned int min(unsigned int lhs, unsigned int rhs){
    return lhs<=rhs?lhs:rhs;
}
uint8_t _resample_8b_8b(    const uint8_t* in,
                            uint8_t* out, 
                            int ls,
                            int rs,
                            const size_t sz){
    size_t i =0;
    while(i<sz){
        out[i] = (in[i]<<ls)|(in[i]>>rs);
        i++;
    }
    return i;
}

uint8_t _resample_8b_16b(   const uint8_t* in,
                            uint8_t* out, 
                            int ls,
                            int rs,
                            const size_t sz){
    size_t i =0;
    uint16_t* nout,t;
    nout = (uint16_t*)out;
    while(i<sz){
        t = in[i];
        nout[i] = (t<<ls)|(t>>rs);
        i++;
    }
    return i;
}

uint8_t _resample_16b_16b(    const uint8_t* in,
                            uint8_t* out, 
                            int ls,
                            int rs,
                            const size_t sz){
    size_t i =0;    
    while(i<sz){
        out[i] = (in[i]<<ls)|(in[i]>>rs);
        i++;
    }
    return i;
}

/*
                    +-----------------------------+
                    |   Linear Sample Rescaling   |
                    +-----------------------------+

      When encoding input samples that have a sample depth that cannot
      be directly represented in PNG, the encoder must scale the samples
      up to a sample depth that is allowed by PNG.  The most accurate
      scaling method is the linear equation

         output = ROUND(input * MAXOUTSAMPLE / MAXINSAMPLE)

      where the input samples range from 0 to MAXINSAMPLE and the
      outputs range from 0 to MAXOUTSAMPLE (which is (2^sampledepth)-1).

      A close approximation to the linear scaling method can be achieved
      by "left bit replication", which is shifting the valid bits to
      begin in the most significant bit and repeating the most
      significant bits into the open bits.  This method is often faster
      to compute than linear scaling.  As an example, assume that 5-bit
      samples are being scaled up to 8 bits.  If the source sample value
      is 27 (in the range from 0-31), then the original bits are:

         4 3 2 1 0
         ---------
         1 1 0 1 1

      Left bit replication gives a value of 222:

         7 6 5 4 3  2 1 0
         ----------------
         1 1 0 1 1  1 1 0
         |=======|  |===|
             |      Leftmost Bits Repeated to Fill Open Bits
             |
         Original Bits

      which matches the value computed by the linear equation.  Left bit
      replication usually gives the same value as linear scaling, and is
      never off by more than one.
*/

int resample_bitdepth(  const uint8_t* in,
                        uint8_t* out, 
                        const uint8_t indepth,
                        const uint8_t outdepth,
                        const size_t sz){
        
    int ls,rs;
    if(outdepth<=indepth) return 0;
    
    ls = outdepth-indepth; /*extra bits to fill*/
    rs = indepth-ls; 

    switch(BITD_BYTE_BOUNDARY[indepth]+BITD_BYTE_BOUNDARY[outdepth]){
        case 2:
            return _resample_8b_8b(in,out,ls,rs,sz);
        case 3:
            return _resample_8b_16b(in,out,ls,rs,sz);
        case 4:
            return _resample_16b_16b(in,out,ls,rs,sz);
        default:
            return 0;
    }
}
