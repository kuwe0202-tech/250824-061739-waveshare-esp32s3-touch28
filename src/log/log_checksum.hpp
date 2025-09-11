/* === BEGIN FILE: src\log\log_checksum.hpp === */

#ifndef LOG_CHECKSUM_HPP
#define LOG_CHECKSUM_HPP

// ACHTUNG: Dieser Header ist privat für das Log-Modul.
// Stellt gemeinsame Checksummen-Funktionen bereit.

#include <stdint.h>
#include <cstddef>

static inline uint16_t crc16_update(uint16_t crc, const void* d, size_t len){
  const uint8_t* p=(const uint8_t*)d; while(len--){ crc^=(uint16_t)(*p++)<<8;
    for(int i=0;i<8;i++) crc=(crc&0x8000)?(uint16_t)((crc<<1)^0x1021):(uint16_t)(crc<<1);
  } return crc;
}

static inline uint32_t crc32_update(uint32_t crc, const void* d, size_t len){
  const uint8_t* p=(const uint8_t*)d; crc=~crc; while(len--){ crc^=*p++;
    for(int i=0;i<8;i++) crc=(crc>>1) ^ (0xEDB88320u & (-(int32_t)(crc & 1)));
  } return ~crc;
}

#endif // LOG_CHECKSUM_HPP

/* === END FILE: src\log\log_checksum.hpp === */