#include <stddef.h>

typedef int (*cmp_fn)(const void *key, const void *elem);
/*
   return:
   -1 if key < elem
    0 if equal
    1 if key > elem
*/

long binarysearch(const void *key, const void *base, size_t count, size_t elem_size, cmp_fn cmp){
    long left,mid,right;
    left = 0;
    mid = 0;
    right = (long)count - 1;

    while (left <= right) {
        long mid = left + (right - left) / 2;
        const void *elem = (const char *)base + mid * elem_size;
        int r = cmp(key, elem);

        if (r == 0)
            return mid;
        else if (r < 0)
            right = mid - 1;
        else
            left = mid + 1;
    }

    return -1;
}
