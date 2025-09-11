/* === BEGIN FILE: include\Konfiguration.h === */

#ifndef KONFIGURATION_H
#define KONFIGURATION_H

#include <stdint.h>
#include <Arduino.h>

#if defined(ARDUINO_ARCH_ESP32)
  #include <driver/ledc.h>
#endif

#include "PlatformConfig.h"
#include "fs_policy.h"

extern uint16_t Flash_Size;

void config_init();
void laufzeitlog_init();

// --- KORRIGIERT & ERWEITERT: Zentrale Definition der Prealloc-Größe und Parameter ---
#ifndef CFG_LOG_PREALLOC_MB
#define CFG_LOG_PREALLOC_MB           64u
#endif
#ifndef CFG_LOG_PREALLOC_BYTES
#define CFG_LOG_PREALLOC_BYTES        (CFG_LOG_PREALLOC_MB * 1024u * 1024u)
#endif
#ifndef CFG_LOG_PREALLOC_CHUNK_BYTES
#define CFG_LOG_PREALLOC_CHUNK_BYTES  (512u * 1024u) // 512 KiB pro Schreib-Chunk
#endif
#ifndef CFG_LOG_PREALLOC_SLICE_BUDGET_MS
#define CFG_LOG_PREALLOC_SLICE_BUDGET_MS 40 // Zeitbudget pro Chunk, um WDT zu füttern
#endif
// --- Ende der Korrektur ---

#ifndef CFG_LOG_RING_BYTES
#define CFG_LOG_RING_BYTES            (32u * 1024u)   
#endif
#ifndef CFG_LOG_FLUSH_MS_BASE
#define CFG_LOG_FLUSH_MS_BASE         5000u           
#endif
#ifndef CFG_LOG_FLUSH_MS_BURST
#define CFG_LOG_FLUSH_MS_BURST        500u            
#endif
#ifndef CFG_LOG_FLUSH_MAX_GAP_MS
#define CFG_LOG_FLUSH_MAX_GAP_MS      (60u * 1000u)   
#endif
#ifndef CFG_LOG_ROTATE_BYTES
#define CFG_LOG_ROTATE_BYTES          (256u * 1024u)  
#endif
#ifndef CFG_LOG_ROTATE_24H
#define CFG_LOG_ROTATE_24H            1               
#endif


#ifndef CFG_LOG_UART_FALLBACK
#define CFG_LOG_UART_FALLBACK         1               
#endif
#ifndef CFG_LOG_UART_MAX_LPS
#define CFG_LOG_UART_MAX_LPS          10u             
#endif
#ifndef CFG_LOG_UART_BOOT_BURST
#define CFG_LOG_UART_BOOT_BURST       100u            
#endif


#ifndef CFG_FS_SPEED_CAP_HZ
#define CFG_FS_SPEED_CAP_HZ           20000000u       
#endif
#ifndef CFG_FS_GUARD_ERR_WIN
#define CFG_FS_GUARD_ERR_WIN          30000u          
#endif
#ifndef CFG_FS_READONLY_THRESHOLD
#define CFG_FS_READONLY_THRESHOLD     3u              
#endif


#ifndef CFG_DUMP_ROTATE_MAX
#define CFG_DUMP_ROTATE_MAX           8u              
#endif
#ifndef CFG_PANIC_SAFE_MODE_THRESHOLD
#define CFG_PANIC_SAFE_MODE_THRESHOLD 2u              
#endif
#ifndef CFG_PANIC_TAIL_MAX
#define CFG_PANIC_TAIL_MAX            (4u * 1024u)    
#endif


#ifndef CFG_CLOCK_JUMP_THRESHOLD_S
#define CFG_CLOCK_JUMP_THRESHOLD_S    5               
#endif


#ifndef TELEMETRY_SD_STALE_MS
#define TELEMETRY_SD_STALE_MS         5000u           
#endif



#ifndef CFG_BOOT_FSM
#define CFG_BOOT_FSM 1
#endif
#ifndef CFG_BOOT_PROGRESS_SMOOTH_MIN_STEP_PCT
#define CFG_BOOT_PROGRESS_SMOOTH_MIN_STEP_PCT 1 
#endif
#ifndef CFG_BOOT_PROGRESS_SMOOTH_RATE_MAX_PCT_PER_S
#define CFG_BOOT_PROGRESS_SMOOTH_RATE_MAX_PCT_PER_S 7 
#endif
#ifndef CFG_FS_MIN_FREE_MB
#define CFG_FS_MIN_FREE_MB 128 
#endif



#define DISPLAY_WIDTH           (platform.display_width)
#define DISPLAY_HEIGHT          (platform.display_height)

namespace Config {

#if defined(ARDUINO_ARCH_ESP32)
constexpr ledc_mode_t    BL_PWM_MODE     = LEDC_LOW_SPEED_MODE;
constexpr ledc_timer_t   BL_PWM_TIMER    = LEDC_TIMER_0;
constexpr ledc_channel_t BL_PWM_CHANNEL  = LEDC_CHANNEL_0;
constexpr uint32_t       BL_PWM_FREQ_HZ  = 5000;
constexpr uint8_t        BL_PWM_RES_BITS = 8;
#endif
constexpr uint8_t  DISP_BACKLIGHT_MAX      = 255;
constexpr uint8_t  DISP_BACKLIGHT_INIT_PCT = 100;

constexpr uint16_t LVGL_TICK_MS           = 5;
constexpr bool     LVGL_FULL_REFRESH      = false;
constexpr uint16_t LVGL_TOUCH_GUARD_MS    = 60;
constexpr uint16_t LVGL_TOUCH_HOLDOVER_MS = 40;
constexpr uint8_t  LVGL_BUF_FRAC_NUM      = 1;
constexpr uint8_t  LVGL_BUF_FRAC_DEN      = 2;

constexpr bool     UI_DEBUG_OVERLAY      = true;
constexpr uint16_t UI_DEBUG_UPDATE_MS    = 500;   
constexpr uint32_t UI_DEBUG_COLOR_HEX    = 0x0000FF;
constexpr uint8_t  UI_DEBUG_TEXT_OPA     = 200;
constexpr int8_t   UI_DEBUG_OFFSET_X     = -4;
constexpr int8_t   UI_DEBUG_OFFSET_Y     = 18;
constexpr bool     UI_DEBUG_TRY_BIG_FONT = true;

constexpr int      RTC_YEAR_OFFSET    = 1970;
constexpr int      RTC_MIN_YEAR       = 2020;
constexpr int      RTC_UI_MIN_YEAR    = 2020;
constexpr int      RTC_UI_MAX_YEAR    = 2050;
constexpr uint16_t RTC_INIT_TICK_MS   = 10;
constexpr uint16_t RTC_ADVANCE_WAIT_MS= 1200;
constexpr uint16_t RTC_KICK_PAUSE_MS  = 5;
constexpr uint8_t  RTC_MAX_RETRIES    = 5;
constexpr uint16_t RTC_RETRY_GAP_MS   = 100;
constexpr bool     RTC_USE_CAP_SEL    = true;

constexpr uint16_t I2C_TIMEOUT_MS = 10;
constexpr uint16_t TOUCH_POLL_MS  = 10;   

#ifndef CONFIG_TEL_KEYS
#define CONFIG_TEL_KEYS
constexpr bool     TEL_ENABLE          = true;
constexpr uint16_t TEL_SAMPLE_MS       = 250;
constexpr uint32_t TEL_LOG_INTERVAL_MS = 60*60*1000UL;
constexpr uint32_t TEL_FIRST_DELAY_MS  = 3000;
constexpr uint16_t TEL_MIN_WARM_SAMPLES= 8;
#endif

} 

#endif

/* === END FILE: include\Konfiguration.h === */