/* === BEGIN FILE: include\boot_header.h === */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define BOOT_HDR_USE_MBEDTLS 0

#define BOOT_HEADER_SCHEMA_VERSION 1U
#define BOOT_HEADER_SECURITY_VERSION 1U

#define BOOT_UUID_SIZE 16
#define FW_HASH_SIZE 32
#define RESET_REASON_TXT_SIZE 32

struct boot_header_t {
    uint8_t  boot_uuid[BOOT_UUID_SIZE];
    uint8_t  fw_hash[FW_HASH_SIZE];
    uint32_t schema_version;
    uint32_t security_version;
    char     reset_reason_txt[RESET_REASON_TXT_SIZE];
    uint64_t boot_seq;
} __attribute__((packed, aligned(8)));

#define EXPECTED_BOOT_HEADER_SIZE (BOOT_UUID_SIZE + FW_HASH_SIZE + sizeof(uint32_t)*2 + RESET_REASON_TXT_SIZE + sizeof(uint64_t))
static_assert(sizeof(boot_header_t) == EXPECTED_BOOT_HEADER_SIZE, "Boot header ABI mismatch!");
static_assert(alignof(boot_header_t) == 8, "Boot header must be 8-byte aligned.");





#define PANIC_HDR_MAGIC 0x50444D50u 
#define PANIC_HDR_VER 1             
#define PANIC_FW_HASH_SHORT_SIZE 9  


enum {
    PANIC_FL_UART_DUMPED    = 1u << 0, 
    PANIC_FL_RETRY_PENDING  = 1u << 1  
};

struct PanicHdr {
  uint32_t magic;       
  uint16_t ver;         
  uint16_t flags;       
  uint32_t len;         
  uint32_t crc;         
  uint64_t ts_us;       
  uint32_t last_seq;    
  uint32_t reboot_cnt;  
  char     fw_hash[PANIC_FW_HASH_SHORT_SIZE]; 
  
} __attribute__((packed, aligned(4)));


static_assert(sizeof(PanicHdr) % 4 == 0, "PanicHdr must be 4-byte aligned in size");
static_assert(PANIC_FW_HASH_SHORT_SIZE >= 2, "PanicHdr::fw_hash must have space for at least one character and a null terminator");

#ifdef __cplusplus
extern "C" {
#endif
bool                 boot_header_init(void);
const boot_header_t* boot_header_get(void);
void                 boot_header_uart_dump(void);

uint64_t             ts_boot_us(void);

uint64_t             ts_boot_us_iram(void);


// NEU: Deklarationen aus nvs_cfg_ab.h hierher verschoben
bool nvs_ab_read_u64 (const char* key, uint64_t* out);
bool nvs_ab_write_u64(const char* key, uint64_t  value);


#ifdef __cplusplus
}
#endif
/* === END FILE: include\boot_header.h === */