#ifndef CURSEDHELPERS_H
#define CURSEDHELPERS_H

#include <stdio.h>
extern void print_binary(int code,int len);
extern void (*cursed_log_callback)(const char* msg);

#ifdef DEBUG
  #define LOG_I(fmt, ...) do { printf(fmt, ##__VA_ARGS__); } while(0)
#else
  #define LOG_I(fmt, ...)
#endif
#if defined(VERBOSE)
  #define LOG_V(fmt, ...) do { printf(fmt, ##__VA_ARGS__); } while(0)
#else
  #define LOG_V(fmt, ...)
#endif

#ifdef VERBOSE
  #define PRINT_BINARY(value, bitcount) do { print_binary(value, bitcount); printf(" ");} while(0)
#else
  #define PRINT_BINARY(value, bitcount)
#endif

#ifndef SILENT
  extern void (*cursed_log_callback)(const char* msg);

#define LOG_E(...) do { \
    char _log_buf[256]; \
    snprintf(_log_buf, sizeof(_log_buf), __VA_ARGS__); \
    if (cursed_log_callback) { \
        cursed_log_callback(_log_buf); \
    } else { \
        fprintf(stderr, "%s", _log_buf); \
    } \
} while(0)
#endif

#endif