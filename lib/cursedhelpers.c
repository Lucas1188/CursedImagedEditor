#include "cursedhelpers.h"
#include <stdio.h>
void print_binary(int code,int len){
  for (len = len - 1; len >= 0; len--) {
    if (code & (1 << len)){
      LOG_I("1");
    }
    else{
      LOG_I("0");
    }
  }
}