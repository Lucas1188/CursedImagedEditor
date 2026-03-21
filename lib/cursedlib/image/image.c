#include "image.h"
#include <stdio.h>

void make_spixel_fmt(const spixel_fmt_info* pixel_info, spixel_fmt* px_fmtout){
    
    px_fmtout->pixel_sz += pixel_info->R.sz;
    px_fmtout->pixel_sz += pixel_info->G.sz;
    px_fmtout->pixel_sz += pixel_info->B.sz;
    px_fmtout->pixel_sz += pixel_info->A.sz;
    px_fmtout->pixel_sz += pixel_info->X.sz;
    px_fmtout->maskR = ((pixel_info->R.sz<<1)-1)<<pixel_info->R.bit_offset;
    px_fmtout->maskG = ((pixel_info->G.sz<<1)-1)<<pixel_info->G.bit_offset;
    px_fmtout->maskB = ((pixel_info->B.sz<<1)-1)<<pixel_info->B.bit_offset;
    px_fmtout->maskA = ((pixel_info->A.sz<<1)-1)<<pixel_info->A.bit_offset;
    memcpy(&px_fmtout->info,pixel_info,sizeof(spixel_fmt_info));
    
}
