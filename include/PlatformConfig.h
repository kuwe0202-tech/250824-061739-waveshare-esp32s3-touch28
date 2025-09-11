#pragma once
#ifndef PLATFORM_CONFIG_H
#define PLATFORM_CONFIG_H

#include <cstdint>

// Enums zur Definition der unterstützten Hardware-Typen
enum class DisplayDriver { ST7789, ILI9341, HDMI };
enum class TouchDriver   { CST328, FT5x06, NONE };
enum class SdMode        { SDMMC, SDSPI };
enum class BusWidth      { BW1 = 1, BW4 = 4 };

struct PlatformConfig {
  // Display-Konfiguration
  DisplayDriver display_driver;
  uint16_t      display_width;
  uint16_t      display_height;
  uint16_t      display_rotation;
  int           pin_disp_sclk;
  int           pin_disp_mosi;
  int           pin_disp_miso;
  int           pin_disp_cs;
  int           pin_disp_dc;
  int           pin_disp_rst;
  int           pin_disp_bl;
  int           spi_host_disp;
  uint32_t      spi_clk_disp_hz;
  uint32_t      spi_clk_disp_read_hz;

  // Touch-Konfiguration
  TouchDriver   touch_driver;
  bool          touch_swap_xy;
  bool          touch_invert_x;
  bool          touch_invert_y;
  uint16_t      touch_raw_x_min;
  uint16_t      touch_raw_x_max;
  uint16_t      touch_raw_y_min;
  uint16_t      touch_raw_y_max;
  int           i2c_port_touch;
  int           pin_touch_sda;
  int           pin_touch_scl;
  int           pin_touch_irq;
  int           pin_touch_rst;
  uint8_t       i2c_addr_touch;

  // SD-Karten-Konfiguration
  SdMode        sd_mode;
  BusWidth      sd_bus;
  uint32_t      sd_clk_hz;
  // SDMMC Pins
  int           pin_sd_clk;
  int           pin_sd_cmd;
  int           pin_sd_d0;
  int           pin_sd_d1;
  int           pin_sd_d2;
  int           pin_sd_d3;
  int           pin_sd_cd;
  // SDSPI Pins
  int           spi_host_sd;
  int           pin_sd_cs;
  int           pin_sd_mosi;
  int           pin_sd_miso;
  int           pin_sd_sclk;

  // RTC-Konfiguration
  int           i2c_port_rtc;
  int           pin_rtc_sda;
  int           pin_rtc_scl;
  uint8_t       i2c_addr_rtc;
  
  // System-Konfiguration
  bool          has_psram;
  uint32_t      wdt_ui_ms;
  uint32_t      wdt_io_ms;
};

// Globale Instanz der Plattformkonfiguration
extern PlatformConfig platform;

// Stub für die zukünftige Hardware-Erkennung
PlatformConfig probe_platform();

#endif // PLATFORM_CONFIG_H