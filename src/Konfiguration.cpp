#include "Konfiguration.h"
#include "Log.h"
#if defined(ARDUINO_ARCH_ESP32)
  #include <Arduino.h>
#endif
#include <Wire.h>
#include "Task_Ticker.h"
#include "LVGL_Treiber.h"
#include "RTC_PCF85063.h"
#include "SD_Karten_Treiber.h"
#include <esp_heap_caps.h>
#include <lvgl.h>
#include "runtime_tasks.h"
#include "fs_policy.h"
#include <SD_MMC.h>

uint16_t Flash_Size = 0;

static bool config_inited = false;
static bool i2c_inited    = false;

static bool i2c_ping(TwoWire& bus, uint8_t addr) {
  bus.beginTransmission(addr);
  return (bus.endTransmission(true) == 0);
}

static void tel_start_once();
void laufzeitlog_init();

void config_init() {
  if (!config_inited) {
    LOG_INIT_BEGIN("Config");
  #if defined(ARDUINO_ARCH_ESP32)
    const uint32_t bytes = ESP.getFlashChipSize();
    Flash_Size = static_cast<uint16_t>(bytes / (1024u * 1024u));
    LOG_INIT_READY("Config");
  #else
    Flash_Size = 0;
    LOG_INIT_FAIL("Config", "unsupported platform");
  #endif
    config_inited = true;
  }

  if (!i2c_inited) {
    LOG_INIT_BEGIN("I2C");
    TwoWire& rtc_bus = (platform.i2c_port_rtc == 0) ? Wire : Wire1;
    const bool ok_rtc = rtc_bus.begin(platform.pin_rtc_sda, platform.pin_rtc_scl, 400000);
    TwoWire& touch_bus = (platform.i2c_port_touch == 0) ? Wire : Wire1;
    const bool ok_touch = touch_bus.begin(platform.pin_touch_sda, platform.pin_touch_scl, 400000);
    if (!ok_rtc || !ok_touch) {
      
      LOGE("I2C begin failed: rtc_bus=%s, touch_bus=%s", ok_rtc ? "OK" : "FAIL", ok_touch ? "OK" : "FAIL");
      LOG_INIT_FAIL("I2C", "begin() failed");
      return;
    }
    rtc_bus.setClock(400000);
    touch_bus.setClock(400000);
    rtc_bus.setTimeOut(Config::I2C_TIMEOUT_MS);
    touch_bus.setTimeOut(Config::I2C_TIMEOUT_MS);
    if (platform.touch_driver != TouchDriver::NONE) {
        if (!i2c_ping(touch_bus, platform.i2c_addr_touch)) {
            
            LOGW("I2C: Touch @0x%02X nicht erreichbar", platform.i2c_addr_touch);
        }
    }
    LOG_INIT_READY("I2C");
    i2c_inited = true;
  }
}

static bool     tel_started = false;
static uint32_t tel_samples = 0;
static uint32_t tel_cpu_sum = 0;   static uint8_t  tel_cpu_max = 0;
static uint32_t tel_fps_sum = 0;   static uint16_t tel_fps_max = 0;
static uint32_t tel_ram_free_sum_k = 0;
static uint32_t tel_ram_free_min_k = 0xFFFFFFFF;
static uint32_t tel_total_ram_k    = 0;
static uint32_t tel_sys_largest_sum_k = 0;
static uint32_t tel_lv_used_sum_k   = 0;
static uint32_t tel_lv_total_k      = 0;
static uint32_t tel_lv_frag_sum_pct = 0;
static uint64_t tel_rtc_age_sum_s   = 0;
static uint32_t tel_rtc_age_max_s   = 0;
static uint32_t tel_rtc_age_samples = 0;
static inline uint32_t to_k(size_t bytes){ return (uint32_t)(bytes / 1024u); }

static void tel_format_ts(char* out, size_t n) {
  if (rtc_datetime.year >= 1970) {
    snprintf(out, n, "%02u.%02u.%04u %02u:%02u",
             (unsigned)rtc_datetime.day, (unsigned)rtc_datetime.month,
             (unsigned)rtc_datetime.year, (unsigned)rtc_datetime.hour, (unsigned)rtc_datetime.minute);
  } else {
    snprintf(out, n, "--.--.---- --:--");
  }
}

static void tel_reset_hour() {
  tel_samples = 0;
  tel_cpu_sum = 0;  tel_cpu_max = 0;
  tel_fps_sum = 0;  tel_fps_max = 0;
  tel_ram_free_sum_k = 0; tel_ram_free_min_k = 0xFFFFFFFF;
  tel_sys_largest_sum_k = 0;
  tel_lv_used_sum_k = 0; tel_lv_total_k = 0; tel_lv_frag_sum_pct = 0;
  tel_rtc_age_sum_s = 0;
  tel_rtc_age_max_s = 0;
  tel_rtc_age_samples = 0;
}

static void tel_sample_task() {
  if (!Config::TEL_ENABLE) return;
  const uint8_t  cpu = Lvgl_GetLoopCpuPct();
  const uint16_t fps = Lvgl_GetFPS();
  const size_t free8   = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  const size_t freeps  = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  const size_t larg_int = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  const size_t larg_ps  = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
  const size_t larg_any = (larg_int > larg_ps) ? larg_int : larg_ps;
  const uint32_t free_k    = to_k(free8 + freeps);
  const uint32_t largest_k = to_k(larg_any);
  uint32_t lv_total_k = 0, lv_free_k = 0, lv_used_k = 0, lv_frag = 0;
#if LV_MEM_CUSTOM
  LvglMemStats st{};
  if (Lvgl_MemGetStats(&st)) {
    lv_used_k = to_k(st.used); lv_total_k = 0; lv_frag = st.frag_est_pct;
  }
#else
  lv_mem_monitor_t mon{}; lv_mem_monitor(&mon);
  lv_total_k = to_k(mon.total_size);
  lv_free_k  = to_k(mon.free_size);
  lv_used_k  = (lv_total_k > lv_free_k) ? (lv_total_k - lv_free_k) : 0;
  lv_frag    = mon.frag_pct;
#endif
  tel_samples++;
  tel_cpu_sum += cpu; if (cpu > tel_cpu_max) tel_cpu_max = cpu;
  tel_fps_sum += fps; if (fps > tel_fps_max) tel_fps_max = fps;
  tel_ram_free_sum_k += free_k;
  if (free_k < tel_ram_free_min_k) tel_ram_free_min_k = free_k;
  tel_sys_largest_sum_k += largest_k;
  tel_lv_used_sum_k   += lv_used_k;
  tel_lv_frag_sum_pct += lv_frag;
  if (tel_lv_total_k == 0 && lv_total_k != 0) tel_lv_total_k = lv_total_k;
  bool rtc_ok_s = false; uint32_t rtc_age_s = 0xFFFFFFFFu;
  telemetry_get_rtc(&rtc_ok_s, &rtc_age_s);
  if (rtc_age_s != 0xFFFFFFFFu) {
    tel_rtc_age_sum_s += rtc_age_s;
    if (rtc_age_s > tel_rtc_age_max_s) tel_rtc_age_max_s = rtc_age_s;
    tel_rtc_age_samples++;
  }
}

static void tel_log_hourly_task() {
  if (!Config::TEL_ENABLE || tel_samples < Config::TEL_MIN_WARM_SAMPLES) return;
  auto avg32 = [](uint64_t s, uint64_t n){ return (uint32_t)(n ? (s / n) : 0); };
  const uint32_t cpu_avg = avg32(tel_cpu_sum, tel_samples);
  const uint32_t fps_avg = avg32(tel_fps_sum, tel_samples);
  const uint32_t free_avg_k = avg32(tel_ram_free_sum_k, tel_samples);
  const uint32_t largest_avg_k = avg32(tel_sys_largest_sum_k, tel_samples);
  const uint32_t used_avg_k = (tel_total_ram_k > free_avg_k) ? (tel_total_ram_k - free_avg_k) : 0;
  const uint32_t lv_used_avg_k = avg32(tel_lv_used_sum_k, tel_samples);
  const uint32_t lv_frag_avg = avg32(tel_lv_frag_sum_pct, tel_samples);
  const uint32_t rtc_age_avg_s = (tel_rtc_age_samples ? (uint32_t)(tel_rtc_age_sum_s / tel_rtc_age_samples) : 0);
  char ts[24]; tel_format_ts(ts, sizeof(ts));
  
  LOGI_T("LAUFZEITLOG", "%s CPU Ø: %u%%  MAX: %u%%", ts, (unsigned)cpu_avg, (unsigned)tel_cpu_max);
  LOGI_T("LAUFZEITLOG", "%s FPS Ø: %u     MAX: %u", ts, (unsigned)fps_avg, (unsigned)tel_fps_max);
  LOGI_T("LAUFZEITLOG", "%s RAM Ø belegt: %uK/%uK", ts, (unsigned)used_avg_k, (unsigned)tel_total_ram_k);
#if LV_MEM_CUSTOM
  LvglMemStats st_now{}; Lvgl_MemGetStats(&st_now);
  if (lv_used_avg_k) LOGI_T("LAUFZEITLOG", "%s LVGL Heap Ø belegt (custom): %uK  Peak: %uK  Blöcke: %u  Frag~%u%%", ts, (unsigned)lv_used_avg_k, (unsigned)to_k(st_now.peak), (unsigned)st_now.blocks, (unsigned)st_now.frag_est_pct);
  else LOGI_T("LAUFZEITLOG", "%s LVGL Heap (custom): n/a", ts);
#else
  if (tel_lv_total_k) LOGI_T("LAUFZEITLOG", "%s LVGL Heap Ø belegt: %uK/%uK  FragØ: %u%%", ts, (unsigned)lv_used_avg_k, (unsigned)tel_lv_total_k, (unsigned)lv_frag_avg);
  else LOGI_T("LAUFZEITLOG", "%s LVGL Heap: n/a", ts);
#endif
  LOGI_T("LAUFZEITLOG", "%s SYS Heap frei Ø/min: %uK/%uK  LargestØ: %uK", ts, (unsigned)free_avg_k, (unsigned)((tel_ram_free_min_k==0xFFFFFFFF)?0:tel_ram_free_min_k), (unsigned)largest_avg_k);
  
  bool sd_ok = telemetry_sd_ok();
  uint32_t free_mb = telemetry_fs_free_mb();
  LOGI_T("LAUFZEITLOG", "%s SD ok: %s, frei: %u MB", ts, sd_ok ? "ja" : "nein", (unsigned)free_mb);

  bool rtc_ok = false; uint32_t rtc_age_latest_s = 0xFFFFFFFFu;
  telemetry_get_rtc(&rtc_ok, &rtc_age_latest_s);
  if (rtc_age_latest_s == 0xFFFFFFFFu) LOGI_T("LAUFZEITLOG", "%s RTC ok: %s  age: n/a  ageØ:n/a  ageMAX:%lus", ts, rtc_ok ? "ja" : "nein", (unsigned long)tel_rtc_age_max_s);
  else LOGI_T("LAUFZEITLOG", "%s RTC ok: %s  age:%lus  ageØ:%lus  ageMAX:%lus", ts, rtc_ok ? "ja" : "nein", (unsigned long)rtc_age_latest_s, (unsigned long)rtc_age_avg_s, (unsigned long)tel_rtc_age_max_s);
  {
    LvglRenderStats rs{}; Lvgl_GetRenderStats(&rs);
    LOGI_T("LAUFZEITLOG", "%s RENDER msØ:%u  msMAX:%u  pxØ:%u  fillØ:%u%%  Full:%u", ts, (unsigned)rs.ms_avg, (unsigned)rs.ms_max, (unsigned)rs.px_avg, (unsigned)rs.fill_pct_avg, (unsigned)rs.full_flush_cnt);
    Lvgl_ResetRenderStats();
    
    uint16_t tick_budget_ms, loop_ms;
    uint32_t overruns, active, free_s, drops;
    telemetry_get_scheduler(&tick_budget_ms, &loop_ms, &overruns, &active, &free_s, &drops);
    LOGI_T("LAUFZEITLOG", "%s SCHED Budget:%ums LoopLast:%ums Overruns:%lu Drops:%lu Active:%lu Free:%lu", 
      ts, (unsigned)tick_budget_ms, (unsigned)loop_ms, (unsigned long)overruns,
      (unsigned long)drops, (unsigned long)active, (unsigned long)free_s);
      
    const int32_t wdt_margin_ms = telemetry_wdt_margin_ms();
    LOGI_T("LAUFZEITLOG", "%s WDT Margin:%ldms", ts, (long)wdt_margin_ms);
    
    uint32_t t_err=0, i2c_retry=0, touch_stale_ms=0xFFFFFFFFu;
    telemetry_get_touch_i2c(&t_err, &i2c_retry, &touch_stale_ms);
    if (touch_stale_ms == 0xFFFFFFFFu) LOGI_T("LAUFZEITLOG", "%s TOUCH Err:%lu  I2C-Retry:%lu  Stale:n/a", ts, (unsigned long)t_err, (unsigned long)i2c_retry);
    else LOGI_T("LAUFZEITLOG", "%s TOUCH Err:%lu  I2C-Retry:%lu  Stale:%lums", ts, (unsigned long)t_err, (unsigned long)i2c_retry, (unsigned long)touch_stale_ms);
  }
  {
    FsPolicyTelemetry stats;
    // KORRIGIERTER AUFRUF: Übergabe per Referenz, ohne '&'
    fs_policy_get_telemetry(stats);
    LOGI_T("LAUFZEITLOG", "%s FS-POLICY Violations:%lu Leaks:%lu LFN-In-Use:%lu", ts, (unsigned long)stats.policy_violations_total, (unsigned long)stats.lfn_temp_leaks_total, (unsigned long)stats.lfn_temp_in_use);
    LOGI_T("LAUFZEITLOG", "%s FS-POLICY Commits Copy:%lu Rename:%lu LastErr:%d Path:'%s'", ts, (unsigned long)stats.commit_copy_used, (unsigned long)stats.commit_rename_used, (int)stats.last_error, stats.last_error_path);
  }
  {
    uint32_t sd_ops = 0; uint64_t sd_bytes = 0;
    telemetry_sd_io_stats_reset(&sd_ops, &sd_bytes);
    const uint32_t iv_ms = Config::TEL_LOG_INTERVAL_MS ? Config::TEL_LOG_INTERVAL_MS : 3600000UL;
    const uint64_t ops_per_h   = (uint64_t)sd_ops   * 3600000ULL / iv_ms;
    const uint64_t bytes_per_h = (uint64_t)sd_bytes * 3600000ULL / iv_ms;
    LOGI_T("LAUFZEITLOG", "%s SD IO ops:%lu  bytes:%llu  ops/h:%llu  bytes/h:%llu", ts, (unsigned long)sd_ops, (unsigned long long)sd_bytes, (unsigned long long)ops_per_h, (unsigned long long)bytes_per_h);
  }
  
  
  log_rotate_maybe();
  tel_reset_hour();
}

static void tel_start_once() {
  if (tel_started || !Config::TEL_ENABLE) return;
  tel_started = true;
  const size_t tot8  = heap_caps_get_total_size(MALLOC_CAP_8BIT);
  const size_t totps = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
  tel_total_ram_k = to_k(tot8 + totps);
  if (tel_total_ram_k == 0) {
    tel_total_ram_k = to_k(heap_caps_get_free_size(MALLOC_CAP_8BIT) + heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
  }
  tel_reset_hour();
  addTaskImmediateNamed("tel.sample", Config::TEL_SAMPLE_MS, tel_sample_task, true);
  addTaskPhasedNamed("tel.hourly", Config::TEL_FIRST_DELAY_MS, Config::TEL_LOG_INTERVAL_MS, tel_log_hourly_task, true);
}

void laufzeitlog_init() {
  tel_start_once();
  setTickBudgetMs(4); 
}