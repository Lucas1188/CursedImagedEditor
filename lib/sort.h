#include <string.h>

static void radix_sort_xb(char **c_array, size_t offset_comparitor, size_t bits, size_t n, unsigned char desc)
{
  size_t i,j,t0,t1;
  unsigned long mask,value;
  char **q0,**q1;
  int bit;
  if (n <= 1) return;
  q0 = malloc(n * sizeof(char*));
  q1 = malloc(n * sizeof(char*));
  if (!q0 || !q1) return;
  mask = 1;
  for (i = 0; i < bits; i++) {
    t0 = 0, t1 = 0;
    for (j = 0; j < n; j++) {
      value = 0;
      memcpy(&value, c_array[j] + offset_comparitor, (bits + 7) / 8);
      bit = (value & mask) ? 1 : 0;
      if (desc) bit = !bit;
      if (bit == 0){
        q0[t0++] = c_array[j];
      }
      else{
        q1[t1++] = c_array[j];
      }
    }
    memcpy(c_array, q0, t0 * sizeof(char*));
    memcpy(c_array + t0, q1, t1 * sizeof(char*));
    mask <<= 1;
  }
  free(q0);
  free(q1);
}

static void insertion_sort_xxB(char **c_array, size_t key0_offset, size_t key1_offset, size_t k0sz, size_t k1sz, size_t arr_sz, unsigned char desc){
  int i, j,cmp;
  long key0, key1, keyA, keyB;
  char *kptr;
  int o0 = sizeof(long) - k0sz;
  int o1 = sizeof(long) - k1sz;

  for (i = 1; i < arr_sz; ++i) {
    kptr = c_array[i];
    if (kptr == NULL) {
        continue;
    }
    keyA = keyB = 0;
    memcpy(((char*)&keyA) + o0, kptr + key0_offset, k0sz);
    memcpy(((char*)&keyB) + o1, kptr + key1_offset, k1sz);
    j = i - 1;
    while (j >= 0) {
      if(c_array[j]==NULL){
        c_array[j + 1] = c_array[j];
        j--;
        continue;
      }
      key0 = key1 = 0;
      memcpy(((char*)&key0) + o0, c_array[j] + key0_offset, k0sz);
      memcpy(((char*)&key1) + o1, c_array[j] + key1_offset, k1sz);
      cmp = key0 == keyA? key1>keyB:key0>keyA;
      cmp = desc?!cmp:cmp;
      if (!cmp){
          break;
      }
      c_array[j + 1] = c_array[j];
      j--;
    }
    c_array[j + 1] = kptr;
  }
}
