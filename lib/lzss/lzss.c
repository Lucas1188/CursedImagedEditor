#include "../cursedhelpers.h"
#include "lzss.h"
#include <stdint.h>

void emit_pointer(slzss_pointer* sp, uint64_t pos, uint16_t match_len, uint16_t dist){
  sp->position = pos;
  sp->length = match_len;
  sp->distance = dist;
  LOG_V("LZPTR: %ld %ld %ld\n",sp->position,sp->length,sp->distance);
  /*
  printf("LZPTR: %ld %ld %ld\n",sp->position,sp->length,sp->distance);
  */
}

unsigned int hash3(const uint8_t *buf) {
  return (buf[0] | (buf[1] << 8) | (buf[2]<<16))&((1<<24)-1);
}

void insert_hash(const uint8_t* window, long int pos, long int input_size){
  if (pos + 2 >= input_size) {
        return; 
    }
  unsigned int h = hash3(window + pos);
    int wpos = pos & (WINDOW_SIZE - 1);

    prev[wpos] = head[h];
    head[h] = pos;
}

int find_match(const uint8_t* window, size_t input_size,
               size_t pos, int maxlen, int *best_dist)
{
    int len, best_len = 0;
    int next;
    int chain = LOOKAHEAD_SIZE;

    size_t limit = (pos > WINDOW_SIZE) ? pos - WINDOW_SIZE : 0;

    /* prevent hash OOB */
    if (pos + 2 >= input_size)
        return 0;

    unsigned int h = hash3(window + pos);
    int cur = head[h];

    while (cur >= 0 && (size_t)cur >= limit && chain--)
    {
        if ((size_t)cur >= pos)
            break;

        if (window[cur] == window[pos]) {
            len = 0;

            while (len < maxlen &&
                   pos + len < input_size &&
                   (size_t)cur + len < input_size &&
                   window[cur + len] == window[pos + len]) {
                len++;
            }

            if (len > best_len){
                best_len = len;
                *best_dist = (int)(pos - (size_t)cur);
            }

            if (best_len >= maxlen)
                break;
        }

        next = prev[cur & (WINDOW_SIZE - 1)];

        if (next >= cur)
            break;

        cur = next;
    }

    return best_len;
}

/*return ptr count*/
int generate_lzss_pointers(uint8_t* input,long int input_size,slzss_pointer* lzss_ptrs,int ptr_n,long int* data_cnt,
                            f_on_emit_lzss_ptr fn_olptr,
                            f_count_distinct_literal fn_literalcount
                            ){
  
  int ptr_count,remaining,maxlen,match_len,dist,i;
  long int pos;
  uint16_t symbol=0;
  uint8_t* s;
  ptr_count = 0;
  for(pos = 0; pos < input_size; pos++){
    dist = 0;
    remaining = input_size - pos;
    maxlen = remaining < LOOKAHEAD_SIZE ? remaining : LOOKAHEAD_SIZE;
    match_len = find_match(input, input_size, pos, maxlen, &dist);
    if(match_len >= MIN_MATCH){

        /*LOG_I("<%d,%d> \n",match_len,dist);*/
        emit_pointer(&lzss_ptrs[ptr_count++], pos, (uint16_t)match_len, (uint16_t)dist);
        

        fn_olptr((uint16_t)match_len,(uint16_t)dist);

        for(i=0;i<match_len;i++){
            insert_hash((const uint8_t*)input, pos+i, input_size);
            s = (uint8_t*)&symbol;
            s[0] = input[pos+i];
            s[1] = 0;
            fn_literalcount(symbol);
        }

        pos += match_len - 1;
    }
    else{
      insert_hash((const uint8_t*) input, pos, input_size);
      s = (uint8_t*)&symbol;
      s[0] = input[pos];
      s[1] = 0;
      fn_literalcount(symbol);
      /*printf("%c",input[pos]);*/
    }
    if(ptr_count==ptr_n){
      pos++;
      break;
    }
  }
  *data_cnt = pos;
  return ptr_count; 
}