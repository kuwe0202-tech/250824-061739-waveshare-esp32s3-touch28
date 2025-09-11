/* === BEGIN FILE: include\Log.h === */

#ifndef LOG_H
#define LOG_H

#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <stddef.h>
#include "Konfiguration.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef enum {
    LOG_LVL_ERR = 1,
    LOG_LVL_WRN = 2,
    LOG_LVL_INF = 3,
    LOG_LVL_DBG = 4,
} log_level_t;


typedef struct {
    char        filename[96];    
    uint32_t    last_write_ms;   
    log_level_t last_level;      
    bool        valid;           
} last_log_info_t;


typedef bool (*RtcProvider)(char* buf, size_t buflen);
typedef void (*log_progress_callback_t)(uint32_t current, uint32_t total);


void    log_init(void);
void    log_set_rtc_provider(RtcProvider provider);
void    log_write(uint8_t level, const char* tag, const char* msg);
void    log_flush(const char* reason);
void    log_force_flush(void);              
void    log_rotate_maybe(void);             
bool    log_readback_tail(char* dst, size_t dst_len, size_t* out_len);
void    log_sd_status_changed(bool mounted_ok);

void    log_notify_rtc_valid(void);

bool    log_is_prealloc_complete(void);

bool log_prealloc_prepare(void);
bool log_primer(void);

// --- ANGEPASSTER BEREICH ---
// Die #if / #endif Direktiven wurden entfernt
bool log_boot_preallocate(void);
void log_boot_adopt_preallocated_file(void);
void log_register_progress_callback(log_progress_callback_t cb);
// --- ENDE ANGEPASSTER BEREICH ---


void    log_get_last_log_info(last_log_info_t* info);
bool    log_run_export_for_file(const char* bin_log_path, bool export_txt, bool export_jsonl);
void    log_pause_flush(void);
void    log_resume_flush(void);
void        nt_display_woke(void);
void log_hint_system_busy(uint32_t ms);


#ifndef LOG_FMT_TMP_BYTES
#define LOG_FMT_TMP_BYTES 256  
#endif

#if defined(__GNUC__) || defined(__clang__)
  #define LOG_PRINTF_LIKE(fmt_idx, va_idx) __attribute__((format(printf, fmt_idx, va_idx)))
#else
  #define LOG_PRINTF_LIKE(fmt_idx, va_idx)
#endif

static inline void log__writef(uint8_t lvl, const char* tag, const char* fmt, ...) LOG_PRINTF_LIKE(3,4);
static inline void log__writef(uint8_t lvl, const char* tag, const char* fmt, ...) {
    char buf[LOG_FMT_TMP_BYTES];
    va_list ap; va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n < 0) return;
    buf[sizeof(buf)-1] = '\0';
    log_write(lvl, tag, buf);
}


#define LOGE_T(TAG, FMT, ...) log__writef(LOG_LVL_ERR, (TAG), (FMT), ##__VA_ARGS__)
#define LOGW_T(TAG, FMT, ...) log__writef(LOG_LVL_WRN, (TAG), (FMT), ##__VA_ARGS__)
#define LOGI_T(TAG, FMT, ...) log__writef(LOG_LVL_INF, (TAG), (FMT), ##__VA_ARGS__)
#define LOGD_T(TAG, FMT, ...) log__writef(LOG_LVL_DBG, (TAG), (FMT), ##__VA_ARGS__)


#define LOGE(FMT, ...) LOGE_T(__func__, (FMT), ##__VA_ARGS__)
#define LOGW(FMT, ...) LOGW_T(__func__, (FMT), ##__VA_ARGS__)
#define LOGI(FMT, ...) LOGI_T(__func__, (FMT), ##__VA_ARGS__)
#define LOGD(FMT, ...) LOGD_T(__func__, (FMT), ##__VA_ARGS__)


#define LOGE_RAW(TAG, MSG) log_write(LOG_LVL_ERR, (TAG), (MSG))
#define LOGW_RAW(TAG, MSG) log_write(LOG_LVL_WRN, (TAG), (MSG))
#define LOGI_RAW(TAG, MSG) log_write(LOG_LVL_INF, (TAG), (MSG))
#define LOGD_RAW(TAG, MSG) log_write(LOG_LVL_DBG, (TAG), (MSG))


#define LOG_INIT_BEGIN(name) LOGI_T("init", "%s: init...", (name))
#define LOG_INIT_READY(name) LOGI_T("init", "%s: bereit.", (name))
#define LOG_INIT_FAIL(name, FMT, ...) LOGW_T("init", "%s: init fehlgeschlagen: " FMT, (name), ##__VA_ARGS__)


#define LOG_FLUSH(reason) do { log_flush((reason)); } while (0)

#ifdef __cplusplus
}
#endif
#endif