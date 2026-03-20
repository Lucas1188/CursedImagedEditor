#include "crc32.h"

uint32_t crc32(unsigned int crc, const uint8_t *buf, size_t len) {

  uint32_t i, k;
  uint32_t _crc = 0xFFFFFFFF & crc;

  for ( i = 0; i < len; i++ )
  {
    _crc ^= ( buf[ i ] );
    for ( k = 8; k; k-- )
    {
      _crc = _crc & 1 ? ( _crc >> 1 ) ^ 0xEDB88320 : _crc >> 1;
    }
  }
  return _crc;
}