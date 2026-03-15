#include <stdio.h>
#include <stdlib.h>
#include "sort.h"

typedef struct test{
  char c;
  long l;
}test;

int main(){
  test ts[5];
  test c;
  char* ptrs[5];
  int i;
  int offset = (char*)&(ts[0].l)-(char*)&(ts[0]);
  int offset2 = (char*)&(ts[0].c)-(char*)&(ts[0]);
  for(i=0;i<5;i++){
    ts[i].c = 'a'+i%3;
    ts[i].l = i%2;
    printf("%c:%ld ",ts[i].c,ts[i].l);
    ptrs[i]=(char*)&ts[i];
  }
  printf("Offset: %d\n",offset);
  
  insertion_sort_xxb(ptrs,offset,offset2,sizeof(long),sizeof(char),5,1);
  for(i=0;i<5;i++){
    c = *(test*)ptrs[i];
    printf("%c:%ld ",c.c,c.l);
  }
  printf("\n");
  return 0;
}
