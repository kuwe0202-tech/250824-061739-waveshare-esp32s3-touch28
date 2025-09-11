#ifndef TOUCH_CST328_H
#define TOUCH_CST328_H

#include <Arduino.h>
#include <Wire.h>
#include "Konfiguration.h"

constexpr uint16_t ESP_LCD_TOUCH_CST328_READ_Number_REG   = 0xD005;
constexpr uint16_t ESP_LCD_TOUCH_CST328_READ_XY_REG       = 0xD000;
constexpr uint16_t HYN_REG_MUT_DEBUG_INFO_MODE            = 0xD101;
constexpr uint16_t HYN_REG_MUT_NORMAL_MODE                = 0xD109;
constexpr uint16_t HYN_REG_MUT_DEBUG_INFO_BOOT_TIME       = 0xD1FC;
constexpr uint16_t HYN_REG_MUT_DEBUG_INFO_TP_NTX          = 0xD1F4;

constexpr int CST328_LCD_TOUCH_MAX_POINTS = 5;
constexpr int MAX_TOUCH_POINTS = CST328_LCD_TOUCH_MAX_POINTS;

struct CST328_Touch {
  uint8_t points;
  struct {
    uint16_t x, y;
    uint16_t strength;
  } coords[CST328_LCD_TOUCH_MAX_POINTS];
};

extern CST328_Touch touch_data;
extern bool touchInitialized;

struct TouchPoint {
  uint16_t x = 0, y = 0;
  uint16_t strength = 0;
  bool active = false;
  bool was_active_last_frame = false;
  unsigned long touch_start = 0;
  unsigned long touch_end   = 0;
  uint16_t start_x = 0, start_y = 0;
};

enum class GestureType : uint8_t {
  None = 0, Tap, DoubleTap, LongPress, SwipeLeft, SwipeRight, SwipeUp, SwipeDown,
  PinchIn, PinchOut, RotateCW, RotateCCW, TwoFingerTap, ThreeFingerTap
};

struct GestureEvent {
  GestureType   type;
  uint16_t      x, y;
  float         value;
  uint8_t       finger_count;
  unsigned long timestamp;
};

extern TouchPoint   touch_points[MAX_TOUCH_POINTS];
extern uint8_t      active_touch_count;
extern uint8_t      last_active_count;
extern GestureEvent last_gesture;
extern const char*  current_state_text;

// API für die entkoppelte Wiederherstellung
bool touch_need_reinit();
void touch_clear_reinit();

bool Touch_I2C_Read(uint8_t addr, uint16_t reg, uint8_t *data, uint32_t length);
bool Touch_I2C_Write(uint8_t addr, uint16_t reg, const uint8_t *data, uint32_t length);

uint16_t CST328_Read_Config();
bool Touch_Read_Data();
void mapRawToDisplay(uint16_t rawX, uint16_t rawY, uint16_t *dispX, uint16_t *dispY);
void updateMultiTouch();
uint8_t Touch_Get_XY(uint16_t *x, uint16_t *y, uint16_t *strength, uint8_t *point_num, uint8_t max_point_num);
void touch_init();

#endif // TOUCH_CST328_H
