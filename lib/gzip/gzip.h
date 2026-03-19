#ifndef GZIP_H
#define GZIP_H
#include <stdio.h>
#include "../bithelper/bithelpers.h"

#define ID1 0x1f
#define ID2 0x8b
#define CM_DEFLATE 8

typedef enum EFLG{
    FTEXT = 1,
    FHCRC = 2,
    FEXTRA = 4,
    FNAME = 8,
    FCOMMENT = 16
}EFLG;
typedef enum EOS{
    OS_FAT = 0,
    OS_AMIGA = 1,
    OS_VMS = 2,
    OS_UNIX = 3,
    OS_VM_CMS = 4,
    OS_ATARI = 5,
    OS_OS2 = 6,
    OS_MACOS = 7,
    OS_Z_SYSTEM = 8,
    OS_CPM = 9,
    OS_TOPS20 = 10,
    OS_NTFS = 11,
    OS_QDOS = 12,
    OS_RISCOS = 13,
    UNKNOWN = 255
}EOS;

typedef enum EXFLG{
    EXFLG_FAST = 2,
    EXFLG_SLOW = 4
}EXFLG;


typedef struct gzip_header{
  unsigned char id1;
  unsigned char id2;
  unsigned char cm;
  unsigned char flg;
  unsigned int mtime;
  unsigned char xfl;
  unsigned char os;
}gzip_header;

typedef struct gzip_footer{
  unsigned int crc32;
  unsigned int isize;
}gzip_footer;

typedef struct gzip_file{
  gzip_header header;
  bitarray* compressed_data;
  size_t compressed_size;
  gzip_footer footer;
}gzip_file;

int write_gzip_from_file(const char* filename, bitarray* bData);

#endif