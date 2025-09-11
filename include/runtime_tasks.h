/* === BEGIN FILE: include\runtime_tasks.h === */
#pragma once
#include <stdint.h>
#include <Arduino.h>
#include "lvgl.h"
#include "Konfiguration.h"


struct boot_header_t;

struct RuntimeTasksStatus {
  bool          sys_scheduled;
  bool          pm_scheduled;
  uint32_t      sys_ticks;
  uint32_t      pm_ticks;
  unsigned long started_ms;
};


// NEU: FSM Gates und zugehörige Funktionen, eingeschlossen in #if CFG_BOOT_FSM
#if CFG_BOOT_FSM
typedef enum {
    FSM_GATE_CORE,
    FSM_GATE_UI_STUB,
    FSM_GATE_RTC,
    FSM_GATE_TOUCH,
    FSM_GATE_SD,
    FSM_GATE_LOG,
    FSM_GATE_SELFTEST
} fsm_gate_t;

void fsm_init_gates();
void fsm_set_gate_flag(fsm_gate_t gate, bool value);
bool fsm_get_gate_flag(fsm_gate_t gate);
bool fsm_all_gates_ok();
void fsm_set_uart_only_mode(const char* reason);
bool fsm_is_uart_only_mode();
#endif


void runtime_tasks_init();
bool runtime_tasks_ok();
RuntimeTasksStatus runtime_tasks_status();

void start_system_status_task(uint32_t period_ms);
void stop_system_status_task();
void start_power_manager_task(uint32_t period_ms);
void stop_power_manager_task();
void pm_mark_user_activity();

void watchdog_init(void);
void watchdog_feed(void);
void watchdog_safe_delay(uint32_t ms);

extern volatile unsigned long touch_deadzone_until;
extern volatile unsigned long touch_block_until;


void start_touch_poll_task(uint32_t period_ms);
void stop_touch_poll_task();


void tel_render_on_flush(uint32_t ms, uint32_t px, bool full_flush);
void tel_touch_on_error();              
void tel_i2c_on_retry();                
void tel_touch_on_valid_sample();        
void tel_rtc_mark_synced();             
void tel_watchdog_set_timeout_ms(uint32_t timeout_ms);
void tel_watchdog_mark_alive();         


void telemetry_get_render(uint32_t* ms_avg,
                          uint32_t* ms_max,
                          uint32_t* px_avg,
                          uint8_t*  fill_pct,
                          uint32_t* full_flush_cnt);
void telemetry_get_lvgl_heap(uint32_t* lv_used_k,
                             uint8_t* lv_frag_pct);  
void telemetry_get_sys_heap(uint32_t* free_k,
                            uint32_t* min_free_k,
                            uint32_t* largest_k);     
void telemetry_get_scheduler(uint16_t* tick_budget_ms,
                             uint16_t* loop_ms_last,
                             uint32_t* overruns_cnt,
                             uint32_t* active_tasks,
                             uint32_t* free_slots,
                             uint32_t* total_drops);
void telemetry_get_touch_i2c(uint32_t* touch_err,
                             uint32_t* i2c_retry,
                             uint32_t* touch_stale_ms);
void telemetry_get_rtc(bool* rtc_ok,
                       uint32_t* rtc_age_s);




bool     telemetry_sd_ok();


uint32_t telemetry_fs_free_mb();


uint32_t telemetry_fs_total_mb();


bool     telemetry_sd_is_stale();


void telemetry_sd_io_stats_reset(uint32_t* out_ops, uint64_t* out_bytes);



int32_t  telemetry_wdt_margin_ms();     
uint8_t  telemetry_backlight_pct();     
uint8_t  telemetry_sleep_flag();        
int16_t  telemetry_rssi_dbm();          
bool     telemetry_mqtt_ok();           
uint16_t telemetry_ping_ms();         

#ifdef __cplusplus
extern "C" {
#endif
void telemetry_boot_fields(const struct boot_header_t* h);
void telemetry_event(const char* name, const char* detail);
#ifdef __cplusplus
}
#endif
/* === END FILE: include\runtime_tasks.h === */