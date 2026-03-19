#include "../cursedhelpers.h"
#include "lzss.h"

void emit_pointer(slzss_pointer* sp, unsigned long pos, unsigned short match_len, unsigned short dist){
  sp->position = pos;
  sp->length = match_len;
  sp->distance = dist;
  /*
  printf("LZPTR: %ld %ld %ld\n",sp->position,sp->length,sp->distance);
  */
}

unsigned int hash3(const unsigned char *buf) {
  return (buf[0] | (buf[1] << 8) | (buf[2]<<16))&((1<<24)-1);
}

void insert_hash(const unsigned char* window, int pos) {
  unsigned int h = hash3(window + pos);
    int wpos = pos & (WINDOW_SIZE - 1);

    prev[wpos] = head[h];
    head[h] = pos;
}

int find_match(const char* window, int pos, int maxlen, int *best_dist)
{
    int len, cur, next,best_len = 0;
    int limit = (pos > WINDOW_SIZE) ? pos - WINDOW_SIZE : 0;
    int chain = LOOKAHEAD_SIZE;
    unsigned int h = hash3((const unsigned char*)(window + pos));
    cur = head[h];
    while (cur >= limit && chain--)
    {
      if (cur >= pos)
        break;   /*cannot match future/self*/

      if (window[cur] == window[pos]) {
        len = 0;
        while (len < maxlen && window[cur + len] == window[pos + len]){
          len++;
        }
        if (len > best_len){
          best_len = len;
          *best_dist = pos - cur;
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
int generate_lzss_pointers(char* input,int input_size,slzss_pointer* lzss_ptrs,int ptr_n,int* data_cnt,
                            f_on_emit_lzss_ptr fn_olptr,
                            f_count_distinct_literal fn_literalcount
                            ){
  
  int ptr_count,remaining,maxlen,match_len,pos,dist,i;
  unsigned short symbol;
  unsigned char* s;
  ptr_count = 0;
  for(pos = 0; pos < input_size /*- MIN_MATCH*/; pos++){
    dist = 0;
    remaining = input_size - pos;
    maxlen = remaining < LOOKAHEAD_SIZE ? remaining : LOOKAHEAD_SIZE;
    match_len = find_match(input, pos, maxlen, &dist);
    if(match_len >= MIN_MATCH){

        /*LOG_I("<%d,%d> \n",match_len,dist);*/
        emit_pointer(&lzss_ptrs[ptr_count++], pos, (unsigned short)match_len, (unsigned short)dist);
        

        fn_olptr((unsigned short)match_len,(unsigned short)dist);

        for(i=0;i<match_len;i++){
            insert_hash((const unsigned char*)input, pos+i);
            s = (unsigned char*)&symbol;
            s[0] = input[pos+i];
            s[1] = 0;
            fn_literalcount(symbol);
        }

        pos += match_len - 1;
    }
    else{
      insert_hash((const unsigned char*) input, pos);
      s = (unsigned char*)&symbol;
      s[0] = input[pos];
      s[1] = 0;
      fn_literalcount(symbol);
      /*printf("%c",input[pos]);*/
    }
    if(ptr_count==ptr_n){
      break;
    }
  }
  *data_cnt = pos;
  return ptr_count; 
}