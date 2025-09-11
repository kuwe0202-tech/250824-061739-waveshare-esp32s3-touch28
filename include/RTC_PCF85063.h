/* === BEGIN FILE: include\RTC_PCF85063.h === */

#ifndef RTC_PCF85063_H
#define RTC_PCF85063_H

#include <Arduino.h>
#include <Wire.h>
#include "Konfiguration.h"
#include <lvgl.h>

constexpr uint8_t RTC_CTRL_1_ADDR     = 0x00;
constexpr uint8_t RTC_CTRL_2_ADDR     = 0x01;
constexpr uint8_t RTC_SECOND_ADDR     = 0x04;
constexpr uint8_t RTC_DAY_ADDR        = 0x07;
constexpr uint8_t RTC_MONTH_ADDR      = 0x09;
constexpr uint8_t RTC_YEAR_ADDR       = 0x0A;
constexpr uint8_t RTC_SECOND_ALARM    = 0x0B;

constexpr uint8_t RTC_CTRL_1_STOP     = 0x20;
constexpr uint8_t RTC_CTRL_1_SR       = 0x10;
constexpr uint8_t RTC_CTRL_1_CAP_SEL  = 0x01;
constexpr uint8_t RTC_CTRL_2_AIE      = 0x80;
constexpr uint8_t RTC_CTRL_2_AF       = 0x40;
constexpr uint8_t RTC_ALARM           = 0x80;

constexpr uint8_t RTC_CTRL_1_DEFAULT  = 0x00;
constexpr uint8_t RTC_CTRL_2_DEFAULT  = 0x00;

typedef struct {
  uint16_t year;
  uint8_t  month;
  uint8_t  day;
  uint8_t  dotw;
  uint8_t  hour;
  uint8_t  minute;
  uint8_t  second;
} datetime_t;

typedef void (*rtc_update_hook_t)(const datetime_t* now);
// --- NEU: Callback für den Moment, in dem die RTC gültig wird ---
typedef void (*rtc_valid_callback_t)(void);

extern datetime_t rtc_datetime;

static inline bool rtc_has_valid_time() { return rtc_datetime.year >= 1970; }


#ifdef __cplusplus
extern "C" {
#endif

void     rtc_notify_time_set(uint64_t new_wallclock_us);
bool     rtc_is_valid(void);

bool rtc_make_iso_timestamp(char* buf, size_t buflen);


bool rtc_get_wall_boot_offset_us(int64_t* offset);

#ifdef __cplusplus
}
#endif

void rtc_pcf85063_init();
void RTC_PCF85063_Loop();

// --- NEU: Hilfsfunktion und Callback-Setter ---
uint64_t rtc_get_unix_us_now(void);
void rtc_set_valid_callback(rtc_valid_callback_t cb);

void rtc_sync_system_time_from_hw(void);

void RTC_PCF85063_Set_Time(const datetime_t &time);
void RTC_PCF85063_Set_Date(const datetime_t &date);
void RTC_PCF85063_Set_All(const datetime_t &time);

void RTC_PCF85063_Read_Time(datetime_t *time);

void rtc_set_update_hook(rtc_update_hook_t cb);

void RTC_PCF85063_Enable_Alarm();
uint8_t RTC_PCF85063_Get_Alarm_Flag();
void RTC_PCF85063_Set_Alarm(const datetime_t &time);
void RTC_PCF85063_Read_Alarm(datetime_t *time);

void rtc_uhr_label_setzen(lv_obj_t *label);
void rtc_uhr_aktualisieren();
void RTC_PCF85063_SetupUhr (lv_obj_t *uhr_label);

void rtc_datum_label_setzen(lv_obj_t *label);
void rtc_datum_aktualisieren();
void RTC_PCF85063_SetupDatum(lv_obj_t *datum_label);

void rtc_wochentag_label_setzen(lv_obj_t *label);
void rtc_wochentag_aktualisieren();
void RTC_PCF85063_SetupWochentag(lv_obj_t *wochentag_label);

void rtc_apply_from_ui_rollers();

#endif

/* === END FILE: include\RTC_PCF85063.h === */