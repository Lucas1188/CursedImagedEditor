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
void _cursed_log_info(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

void _cursed_log_verbose(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}

void _cursed_log_e_impl(const char *fmt, ...) {
    char _log_buf[512];
    va_list args;

    va_start(args, fmt);
    /* ANSI C89 standard way to format variadic arguments into a buffer */
    vsprintf(_log_buf, fmt, args);
    va_end(args);

    if (cursed_log_callback) {
        cursed_log_callback(_log_buf);
    } else {
        fprintf(stderr, "%s", _log_buf);
    }
}

/**
 * Manual implementation of strncpy logic.
 * Copies up to 'n' characters from 'src' to 'dest'.
 * If 'src' is less than 'n' characters long, the remainder 
 * of 'dest' is filled with '\0' bytes.
 */
void cursed_strncpy(char *dest, const char *src, size_t n) {
    size_t i;

    /* Loop 1: Copy characters from src to dest */
    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }

    /* Loop 2: ANSI C Standard requires padding the rest with nulls */
    for (; i < n; i++) {
        dest[i] = '\0';
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
    cursed_strncpy(dest, scratch, dest_sz - 1);
    dest[dest_sz - 1] = '\0'; /* Guarantee null termination */
}