#ifndef CURSEDHELPERS_H
#define CURSEDHELPERS_H

#include <stdio.h>
extern void print_binary(int code,int len);
extern void (*cursed_log_callback)(const char* msg);
void _cursed_log_info(const char *fmt, ...);
void _cursed_log_verbose(const char *fmt, ...);
void _cursed_log_e_impl(const char *fmt, ...);

#ifdef DEBUG
  /* The macro 'args' will be the entire (fmt, ...) tuple */
  #define LOG_I _cursed_log_info
#else
  #define LOG_I if(1); else _cursed_log_info
#endif

#if defined(VERBOSE)
  #define LOG_V _cursed_log_verbose
#else
  #define LOG_V if(1); else _cursed_log_verbose
#endif

#ifdef VERBOSE
  #define PRINT_BINARY(value, bitcount) do { print_binary(value, bitcount); printf(" ");} while(0)
#else
  #define PRINT_BINARY(value, bitcount)
#endif

void cursed_snprintf_fallback(char *dest, size_t dest_sz, const char *fmt, ...);
void cursed_strncpy(char *dest, const char *src, size_t n);

extern void (*cursed_log_callback)(const char* msg);

#ifndef SILENT  
  #define LOG_E _cursed_log_e_impl
#endif

#endif