/* === BEGIN FILE: src\log\log_format.hpp === */

#ifndef LOG_FORMAT_HPP
#define LOG_FORMAT_HPP

// ACHTUNG: Dieser Header ist privat für das Log-Modul.
// Enthält gemeinsame Datenstrukturen für das binäre Log-Format.

#include <stdint.h>

#define LOG_TAG_MAX_LEN 31
#define LOG_CHUNK_PAYLOAD_SIZE      (256 - sizeof(LogHdr))
#define LOG_CONT_CHUNK_PAYLOAD_SIZE (256 - sizeof(LogContHdr))

struct LogHdr {
  uint64_t ts_us; uint32_t seq; uint8_t level; uint8_t tag_len;
  uint16_t msg_len; uint16_t crc16; uint16_t flags;
} __attribute__((packed));

struct LogContHdr { uint16_t flags; } __attribute__((packed));

enum { 
    LOG_HDR_FLAG_FRAGMENTED = 1u << 0, 
    LOG_HDR_FLAG_CONTINUES  = 1u << 1 
};

#endif // LOG_FORMAT_HPP

/* === END FILE: src\log\log_format.hpp === */