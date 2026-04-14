#include "cursedhelpers.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

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

/* A variadic wrapper that mimics snprintf using C89-standard vsprintf */
void cursed_snprintf_fallback(char *dest, size_t dest_sz, const char *fmt, ...) {
    va_list args;
    /* 1. The Scratchpad: Make this large enough for any reasonable log entry */
    char scratch[2048]; 

    if (dest_sz == 0) return;

    va_start(args, fmt);
    /* 2. Format into the scratchpad (Standard ANSI C) */
    vsprintf(scratch, fmt, args);
    va_end(args);

    /* 3. Manually enforce the boundary during the transfer to the log buffer */
    strncpy(dest, scratch, dest_sz - 1);
    dest[dest_sz - 1] = '\0'; /* Guarantee null termination */
}