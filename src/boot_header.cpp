/* === BEGIN FILE: src\boot_header.cpp === */
#include "boot_header.h"

#include <stdio.h>
#include <string.h>
#include <inttypes.h>

// NEU: Includes, die von der NVS A/B Logik benötigt werden
#include <nvs_flash.h>
#include <nvs.h>
#include <assert.h>


#if defined(ARDUINO_ARCH_ESP32) || defined(ESP_PLATFORM)
  #include <esp_system.h>
  #if __has_include(<esp_random.h>)
    #include <esp_random.h>
  #endif
  
  #if __has_include(<esp_attr.h>)
    #include <esp_attr.h>
  #endif
  
  #if __has_include(<esp_timer.h>)
    #include <esp_timer.h>
  #endif
#endif

#if BOOT_HDR_USE_MBEDTLS == 1
  #if defined(__has_include) && __has_include(<mbedtls/md.h>)
    #define BOOT_HDR_HAVE_MBEDTLS 1
    #include <mbedtls/md.h>
  #endif
#endif

#if defined(__has_include)
  #if __has_include("security/rng.h")
    #include "security/rng.h"
  #endif
  #if __has_include("core/reset_policy.h")
    #include "core/reset_policy.h"
  #endif
  // Die nvs_cfg_ab.h wird nicht mehr benötigt
#endif

extern "C" __attribute__((weak)) bool rng_check_health(void) { return true; }

#if defined(ARDUINO_ARCH_ESP32) || defined(ESP_PLATFORM)

static inline uint32_t IRAM_ATTR read_ccount_() {
  uint32_t c;
  asm volatile("rsr.ccount %0" : "=a"(c));
  return c;
}

static volatile uint32_t s_cc_last        = 0;
static volatile uint32_t s_cc_residual    = 0;  
static volatile uint32_t s_cycles_per_us  = 0;  
static volatile uint64_t s_cc_accum_us    = 0;


#ifndef F_CPU
#define F_CPU 240000000UL
#endif
#endif 

#if defined(ARDUINO_ARCH_ESP32) || defined(ESP_PLATFORM)
__attribute__((weak)) void rng_get_random_bytes(uint8_t* d, size_t n){
  for (size_t i = 0; i < n; i += 4) {
    uint32_t r = esp_random();
    size_t chunk = (n - i) < 4 ? (n - i) : 4;
    memcpy(d + i, &r, chunk);
  }
}
#else
  #include <Arduino.h>
__attribute__((weak)) void rng_get_random_bytes(uint8_t* d, size_t n){
  unsigned long seed = (unsigned long)micros();
#ifdef A0
  seed ^= (unsigned long)analogRead(A0);
#else
  seed ^= (unsigned long)analogRead(0);
#endif
  randomSeed(seed);
  for (size_t i=0; i<n; ++i) d[i] = (uint8_t)random(0, 256);
}
#endif

extern "C" const char* reset_policy_get_reason_str(int code) {
#if defined(ARDUINO_ARCH_ESP32) || defined(ESP_PLATFORM)
  switch ((esp_reset_reason_t)code) {
    case ESP_RST_POWERON:   return "POWERON";
    case ESP_RST_SW:        return "SW";
    case ESP_RST_PANIC:     return "PANIC";
    case ESP_RST_INT_WDT:   return "INT_WDT";
    case ESP_RST_TASK_WDT:  return "TASK_WDT";
    case ESP_RST_WDT:       return "WDT";
    case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
    case ESP_RST_BROWNOUT:  return "BROWNOUT";
    case ESP_RST_SDIO:      return "SDIO";
    default:                return "UNKNOWN";
  }
#else
  (void)code; return "UNKNOWN";
#endif
}

extern "C" __attribute__((weak)) void reset_policy_trigger_safe_mode(const char*) {  }

// ENTFERNT: Die alten, schwachen Implementierungen von nvs_ab_... werden gelöscht
// extern "C" __attribute__((weak)) bool nvs_ab_read_u64(const char*, uint64_t* out){ if(out) *out = 0; return false; }
// extern "C" __attribute__((weak)) bool nvs_ab_write_u64(const char*, uint64_t) { return true; }


// NEU: Vollständige Implementierung der NVS A/B Logik
namespace {
#pragma pack(push,1)
struct Rec {
  uint8_t  version;   
  uint8_t  rsv;       
  uint32_t crc32;     
  uint64_t value;     
  uint32_t counter;   
};
#pragma pack(pop)
static_assert(sizeof(Rec)==1+1+4+8+4, "Rec size mismatch");

static uint32_t crc32(const void* data, size_t n) {
  const uint8_t* p = static_cast<const uint8_t*>(data);
  uint32_t crc = ~0U;
  while (n--) { crc ^= *p++; for (int i=0;i<8;i++) crc = (crc>>1) ^ (-(crc&1)&0xEDB88320); }
  return ~crc;
}

static bool nvs_ready = false;
static void ensure_nvs() {
  if (nvs_ready) return;
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    nvs_flash_erase();
    nvs_flash_init();
  }
  nvs_ready = true;
}

static void keysAB(const char* key, char* a, char* b, size_t cap) {
  snprintf(a, cap, "%s_A", key);
  snprintf(b, cap, "%s_B", key);
}

static bool read_one(nvs_handle_t h, const char* k, Rec* r, bool* valid) {
  size_t len = sizeof(Rec);
  esp_err_t err = nvs_get_blob(h, k, r, &len);
  if (err != ESP_OK || len != sizeof(Rec)) { *valid=false; return true; }
  uint32_t got = r->crc32;
  Rec tmp = *r; tmp.crc32=0;
  *valid = (got == crc32(&tmp, sizeof(Rec)));
  return true;
}

static bool write_one(nvs_handle_t h, const char* k, const Rec& in) {
  Rec w = in; w.crc32=0; w.crc32 = crc32(&w, sizeof(Rec));
  esp_err_t err = nvs_set_blob(h, k, &w, sizeof(Rec));
  if (err != ESP_OK) return false;
  err = nvs_commit(h);
  return (err == ESP_OK);
}
} 

extern "C" bool nvs_ab_read_u64(const char* key, uint64_t* out) {
  if (!key || !out) return false;
  ensure_nvs();

  nvs_handle_t nh;
  if (nvs_open("sys", NVS_READONLY, &nh) != ESP_OK) return false;

  char ka[16], kb[16]; keysAB(key, ka, kb, sizeof(ka));

  Rec ra{}, rb{}; bool va=false, vb=false;
  read_one(nh, ka, &ra, &va);
  read_one(nh, kb, &rb, &vb);
  nvs_close(nh);

  if (!va && !vb) return false;
  const Rec* best = (!vb || (va && ra.counter >= rb.counter)) ? &ra : &rb;
  *out = best->value;
  return true;
}

extern "C" bool nvs_ab_write_u64(const char* key, uint64_t value) {
  if (!key) return false;
  ensure_nvs();

  nvs_handle_t nh;
  if (nvs_open("sys", NVS_READWRITE, &nh) != ESP_OK) return false;

  char ka[16], kb[16]; keysAB(key, ka, kb, sizeof(ka));

  Rec ra{}, rb{}; bool va=false, vb=false;
  read_one(nh, ka, &ra, &va);
  read_one(nh, kb, &rb, &vb);

  const bool have = (va || vb);
  const bool a_is_newest = (va && (!vb || ra.counter >= rb.counter));
  const char* target = a_is_newest ? kb : ka;
  const Rec*  cur    = a_is_newest ? &ra : (vb ? &rb : nullptr);

  Rec nw{};
  nw.version = 1;
  nw.rsv     = 0;
  nw.value   = value;
  nw.counter = have && cur ? (cur->counter + 1) : 1;

  bool ok = write_one(nh, target, nw);
  nvs_close(nh);
  return ok;
}


extern "C" void telemetry_boot_fields(const boot_header_t*);
extern "C" void telemetry_event(const char* name, const char* detail);

extern "C" __attribute__((weak)) const uint8_t g_firmware_build_hash[FW_HASH_SIZE] = {0};

#ifdef FW_HASH_HEX
static bool parse_fw_hash_hex(const char* hex, uint8_t out[FW_HASH_SIZE]) {
    auto hexval = [](char c)->int{
        if (c>='0'&&c<='9') return c-'0';
        if (c>='a'&&c<='f') return c-'a'+10;
        if (c>='A'&&c<='F') return c-'A'+10;
        return -1;
    };
    size_t L = 0; while (hex[L] != 0) ++L;
    if (L != FW_HASH_SIZE*2) return false;
    for (size_t i=0;i<FW_HASH_SIZE;i++){
        int hi = hexval(hex[2*i]);
        int lo = hexval(hex[2*i+1]);
        if (hi<0 || lo<0) return false;
        out[i] = (uint8_t)((hi<<4)|lo);
    }
    return true;
}
#endif

static boot_header_t g_boot_header;
static bool g_initialized = false;

static void print_hex(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; ++i) printf("%02x", data[i]);
}
static bool is_all_zero(const uint8_t* d, size_t n){
    for(size_t i=0;i<n;i++) if(d[i]!=0) return false;
    return true;
}

#if BOOT_HDR_USE_MBEDTLS == 1
static int hkdf_sha256(const uint8_t* salt, size_t salt_len,
                       const uint8_t* ikm,  size_t ikm_len,
                       const uint8_t* info, size_t info_len,
                       uint8_t* okm, size_t okm_len) {
#if BOOT_HDR_HAVE_MBEDTLS
    const mbedtls_md_info_t* md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (!md) return -1;
    uint8_t prk[32];
    mbedtls_md_hmac(md, salt, salt_len, ikm, ikm_len, prk);

    uint8_t T[32];
    size_t pos = 0;
    uint8_t counter = 1;
    size_t t_len = 0;

    while (pos < okm_len) {
        mbedtls_md_context_t ctx;
        mbedtls_md_init(&ctx);
        if (mbedtls_md_setup(&ctx, md, 1) != 0) { mbedtls_md_free(&ctx); return -1; }
        mbedtls_md_hmac_starts(&ctx, prk, sizeof(prk));
        if (t_len) mbedtls_md_hmac_update(&ctx, T, t_len);
        mbedtls_md_hmac_update(&ctx, info, info_len);
        mbedtls_md_hmac_update(&ctx, &counter, 1);
        mbedtls_md_hmac_finish(&ctx, T);
        mbedtls_md_free(&ctx);

        size_t cpy = (okm_len - pos < sizeof(T)) ? (okm_len - pos) : sizeof(T);
        memcpy(okm + pos, T, cpy);
        pos += cpy;
        t_len = sizeof(T);
        counter++;
    }
    memset(prk, 0, sizeof(prk));
    memset(T, 0, sizeof(T));
    return 0;
#else
    (void)salt; (void)salt_len; (void)ikm; (void)ikm_len; (void)info; (void)info_len; (void)okm; (void)okm_len;
    return -1;
#endif
}
#endif

bool boot_header_init(void) {
    if (g_initialized) return true;

    memset(&g_boot_header, 0, sizeof(g_boot_header));

    const bool trng_ok = rng_check_health();
    if (!trng_ok) {
        reset_policy_trigger_safe_mode("TRNG_HEALTH_FAIL");
        telemetry_event("trng_health_fail", "rng_unhealthy_before_uuid");
    }

#if BOOT_HDR_USE_MBEDTLS == 1
    uint8_t ikm[32], salt[16];
    rng_get_random_bytes(ikm, sizeof(ikm));
    rng_get_random_bytes(salt, sizeof(salt));
    static const uint8_t info[] = {'b','o','o','t','-','u','u','i','d','-','v','1'};

    if (hkdf_sha256(salt, sizeof(salt), ikm, sizeof(ikm), info, sizeof(info), g_boot_header.boot_uuid, BOOT_UUID_SIZE) != 0) {
        rng_get_random_bytes(g_boot_header.boot_uuid, BOOT_UUID_SIZE);
    }
#else
    rng_get_random_bytes(g_boot_header.boot_uuid, BOOT_UUID_SIZE);
#endif

    g_boot_header.boot_uuid[6] = (g_boot_header.boot_uuid[6] & 0x0F) | 0x40;
    g_boot_header.boot_uuid[8] = (g_boot_header.boot_uuid[8] & 0x3F) | 0x80;

    memcpy(g_boot_header.fw_hash, g_firmware_build_hash, FW_HASH_SIZE);
#ifdef FW_HASH_HEX
    if (is_all_zero(g_boot_header.fw_hash, FW_HASH_SIZE)) {
        uint8_t tmp[FW_HASH_SIZE];
        if (parse_fw_hash_hex(FW_HASH_HEX, tmp)) {
            memcpy(g_boot_header.fw_hash, tmp, FW_HASH_SIZE);
        }
    }
#endif

    g_boot_header.schema_version  = BOOT_HEADER_SCHEMA_VERSION;
    g_boot_header.security_version = BOOT_HEADER_SECURITY_VERSION;
    static_assert(BOOT_HEADER_SCHEMA_VERSION > 0, "Schema version must be positive.");
    static_assert(BOOT_HEADER_SECURITY_VERSION >= 1, "Security version seems too low.");

    #if defined(ARDUINO_ARCH_ESP32) || defined(ESP_PLATFORM)
      const char* reason_str = reset_policy_get_reason_str((int)esp_reset_reason());
    #else
      const char* reason_str = reset_policy_get_reason_str(0);
    #endif
    if (!reason_str) reason_str = "UNKNOWN";
    strncpy(g_boot_header.reset_reason_txt, reason_str, RESET_REASON_TXT_SIZE - 1);
    g_boot_header.reset_reason_txt[RESET_REASON_TXT_SIZE - 1] = '\0';

    uint64_t last_seen_seq = 0;
    (void)nvs_ab_read_u64("boot_seq", &last_seen_seq);

    if (last_seen_seq >= UINT64_MAX - 1ULL) {
        reset_policy_trigger_safe_mode("BOOT_SEQ_OVERFLOW");
        telemetry_event("boot_seq_overflow", "");
        g_boot_header.boot_seq = UINT64_MAX;
    } else if (last_seen_seq == 0) {
        g_boot_header.boot_seq = 1;
    } else {
        g_boot_header.boot_seq = last_seen_seq + 1;
    }

#if defined(ARDUINO_ARCH_ESP32) || defined(ESP_PLATFORM)
    
    if (s_cycles_per_us == 0) {
        s_cycles_per_us = (uint32_t)(F_CPU / 1000000UL);
        if (s_cycles_per_us == 0) s_cycles_per_us = 240; 
    }
    s_cc_last     = read_ccount_();
    s_cc_residual = 0;
    s_cc_accum_us = 0;
#endif

    g_initialized = true;
    boot_header_uart_dump();
    telemetry_boot_fields(&g_boot_header);

    return true;
}

const boot_header_t* boot_header_get(void) {
    return g_initialized ? &g_boot_header : NULL;
}

void boot_header_uart_dump(void) {
    if (!g_initialized) { printf("Boot-Header: NICHT INITIALISIERT\n"); return; }
    printf("\n--- Boot Header (Schema v%u) ---\n", g_boot_header.schema_version);
    printf("  Boot Seq: %" PRIu64 "\n", g_boot_header.boot_seq);
    printf("  Reset:    %s\n", g_boot_header.reset_reason_txt);
    printf("  UUID:     "); print_hex(g_boot_header.boot_uuid, BOOT_UUID_SIZE); printf("\n");
    printf("  FW Hash:  "); print_hex(g_boot_header.fw_hash, FW_HASH_SIZE); printf("\n");
    printf("  Security: %u\n", g_boot_header.security_version);
    printf("----------------------------------\n\n");
}

#ifdef BOOT_HEADER_SELFTEST
extern "C" void boot_header_selftest_sample(void){
    boot_header_t h = {};
    for (int i=0;i<BOOT_UUID_SIZE;i++) h.boot_uuid[i]=(uint8_t)i;
    for (int i=0;i<FW_HASH_SIZE;i++)  h.fw_hash[i]=(uint8_t)(255-i);
    h.schema_version=BOOT_HEADER_SCHEMA_VERSION;
    h.security_version=BOOT_HEADER_SECURITY_VERSION;
    strncpy(h.reset_reason_txt,"POWERON",sizeof(h.reset_reason_txt)-1);
    h.boot_seq=42;
    printf("\n--- Boot Header (Schema v%u) ---\n", h.schema_version);
    printf("  Boot Seq: %" PRIu64 "\n", h.boot_seq);
    printf("  Reset:    %s\n", h.reset_reason_txt);
    printf("  UUID:     "); print_hex(h.boot_uuid, BOOT_UUID_SIZE); printf("\n");
    printf("  FW Hash:  "); print_hex(h.fw_hash, FW_HASH_SIZE); printf("\n");
    printf("  Security: %u\n", h.security_version);
    printf("----------------------------------\n\n");
}
#endif





extern "C" uint64_t IRAM_ATTR ts_boot_us(void) {
#if defined(ARDUINO_ARCH_ESP32) || defined(ESP_PLATFORM)
  uint32_t now   = read_ccount_();
  uint32_t prev  = s_cc_last;
  s_cc_last      = now;
  uint32_t delta = now - prev; 

  uint32_t per = s_cycles_per_us ? s_cycles_per_us : 240;
  uint64_t cyc = (uint64_t)s_cc_residual + (uint64_t)delta;
  uint64_t inc_us = cyc / per;
  s_cc_residual   = (uint32_t)(cyc - inc_us * per);
  s_cc_accum_us  += inc_us;
  return s_cc_accum_us;
#else
  
  static uint32_t last = 0;
  static uint64_t acc  = 0;
  uint32_t now = (uint32_t)micros();
  uint32_t delta = now - last;
  acc += (uint64_t)delta;
  last = now;
  return acc;
#endif
}



extern "C" uint64_t IRAM_ATTR ts_boot_us_iram(void) {
  return ts_boot_us();
}
/* === END FILE: src\boot_header.cpp === */