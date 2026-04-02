#ifndef CURSEDHELPERS_H
#define CURSEDHELPERS_H
#include <stdio.h>
extern void print_binary(int code,int len);

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
  #define LOG_E(fmt, ...) do { fprintf(stderr, fmt, ##__VA_ARGS__); } while(0)
#endif

#endif