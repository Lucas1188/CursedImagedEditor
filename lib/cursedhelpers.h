#ifndef CURSEDHELPERS_H
#define CURSEDHELPERS_H
#include <stdio.h>

extern void print_binary(int code,int len);

#ifdef DEBUG
  #define LOG_I(fmt, ...) do { printf(fmt, ##__VA_ARGS__); } while(0)
#else
  #define LOG_I(fmt, ...) do {} while(0)
#endif
#if defined(VERBOSE) || defined(DEBUG)
  #define LOG_V(fmt, ...) do { printf(fmt, ##__VA_ARGS__); } while(0)
#else
  #define LOG_V(fmt, ...) do {} while(0)
#endif

#ifdef DEBUG
  #define PRINT_BINARY(value, bitcount) do { print_binary(value, bitcount); printf(" ");} while(0)
#else
  #define PRINT_BINARY(value, bitcount) do {} while(0)
#endif

#ifndef SILENT
  #define LOG_E(fmt, ...) do { fprintf(stderr, fmt, ##__VA_ARGS__); } while(0)
#endif

#endif