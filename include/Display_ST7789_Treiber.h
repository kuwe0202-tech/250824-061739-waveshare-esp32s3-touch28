#pragma once
#include <LovyanGFX.hpp>
#include "Konfiguration.h"

class LGFX : public lgfx::LGFX_Device {
public:
  LGFX();

private:
  lgfx::Bus_SPI       _bus_instance;
  lgfx::Panel_ST7789  _panel_instance;
};

extern LGFX tft;
bool tft_is_ready();
void tft_init();
void backlight_init();
void set_backlight_percent(uint8_t pct);
void display_sleep();
void display_wake();