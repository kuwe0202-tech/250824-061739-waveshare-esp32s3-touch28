#include "Display_ST7789_Treiber.h"
#include "PlatformConfig.h"
#include <Arduino.h>
#include "Log.h"

#if defined(ARDUINO_ARCH_ESP32)
  #include <driver/ledc.h>
  #include <driver/spi_common.h>
#else
  typedef int spi_host_device_t;
  enum { SPI1_HOST = 0, SPI2_HOST = 1, SPI3_HOST = 2 };
  #ifndef SPI_DMA_CH_AUTO
    #define SPI_DMA_CH_AUTO 0
  #endif
#endif


LGFX::LGFX(void) {
  { 
    auto cfg = _bus_instance.config();
    cfg.spi_host    = (spi_host_device_t)platform.spi_host_disp;
    cfg.spi_mode    = 0;
    cfg.freq_write  = platform.spi_clk_disp_hz;
    cfg.freq_read   = platform.spi_clk_disp_read_hz;
    cfg.spi_3wire   = (platform.pin_disp_miso < 0);
    cfg.use_lock    = true;
    cfg.dma_channel = SPI_DMA_CH_AUTO;
    cfg.pin_sclk    = platform.pin_disp_sclk;
    cfg.pin_mosi    = platform.pin_disp_mosi;
    cfg.pin_miso    = platform.pin_disp_miso;
    cfg.pin_dc      = platform.pin_disp_dc;
    _bus_instance.config(cfg);
    _panel_instance.setBus(&_bus_instance);
  }
  { 
    auto cfg = _panel_instance.config();
    cfg.pin_cs           = platform.pin_disp_cs;
    cfg.pin_rst          = platform.pin_disp_rst;
    cfg.pin_busy         = -1;
    
    cfg.memory_width     = 240;
    cfg.memory_height    = 320;
    cfg.panel_width      = 240;
    cfg.panel_height     = 320;

    cfg.offset_x         = 0;
    cfg.offset_y         = 0;
    cfg.offset_rotation  = 0;

    cfg.dummy_read_pixel = 8;
    cfg.dummy_read_bits  = 1;

    cfg.readable         = (platform.pin_disp_miso >= 0);
    cfg.invert           = true;
    cfg.rgb_order        = true;
    cfg.dlen_16bit       = false;
    cfg.bus_shared       = true;
    _panel_instance.config(cfg);
  }
  setPanel(&_panel_instance);
}

LGFX tft;

static bool s_tft_ready = false;
static uint8_t s_bl_percent = Config::DISP_BACKLIGHT_INIT_PCT; 

#if defined(ARDUINO_ARCH_ESP32)
static inline void bl_write_raw(uint32_t raw) {
  const uint32_t max_raw = (1u << Config::BL_PWM_RES_BITS) - 1u;
  if (raw > max_raw) raw = max_raw;
  ledc_set_duty(Config::BL_PWM_MODE, Config::BL_PWM_CHANNEL, raw);
  ledc_update_duty(Config::BL_PWM_MODE, Config::BL_PWM_CHANNEL);
}
#else
static inline void bl_write_raw(uint32_t raw) {
  pinMode(platform.pin_disp_bl, OUTPUT);
  uint8_t v = (raw > 255u) ? 255u : (uint8_t)raw;
  analogWrite(platform.pin_disp_bl, v);
}
#endif

void tft_init() {
  if (s_tft_ready) return;
  LOG_INIT_BEGIN("TFT");
  const bool ok = tft.init();
  if (ok) {
    tft.setRotation(platform.display_rotation);
    tft.fillScreen(TFT_BLACK);
    LOG_INIT_READY("TFT");
    s_tft_ready = true;
  } else {
    LOG_INIT_FAIL("TFT", "init() failed");
    s_tft_ready = false;
  }
}

bool tft_is_ready() { return s_tft_ready; }

#if defined(ARDUINO_ARCH_ESP32)
void backlight_init() {
  LOG_INIT_BEGIN("Backlight");
  pinMode(platform.pin_disp_bl, OUTPUT);

  ledc_timer_config_t tcfg{};
  tcfg.speed_mode      = Config::BL_PWM_MODE;
  tcfg.timer_num       = Config::BL_PWM_TIMER;
  tcfg.duty_resolution = (ledc_timer_bit_t)Config::BL_PWM_RES_BITS;
  tcfg.freq_hz         = Config::BL_PWM_FREQ_HZ;
  tcfg.clk_cfg         = LEDC_AUTO_CLK;
  ledc_timer_config(&tcfg);

  ledc_channel_config_t ccfg{};
  ccfg.gpio_num   = platform.pin_disp_bl;
  ccfg.speed_mode = Config::BL_PWM_MODE;
  ccfg.channel    = Config::BL_PWM_CHANNEL;
  ccfg.intr_type  = LEDC_INTR_DISABLE;
  ccfg.timer_sel  = Config::BL_PWM_TIMER;
  ccfg.duty       = 0;
  ccfg.hpoint     = 0;
  ledc_channel_config(&ccfg);

  set_backlight_percent(s_bl_percent);
  LOG_INIT_READY("Backlight");
}
#else  
void backlight_init() {
  LOG_INIT_BEGIN("Backlight");
  set_backlight_percent(s_bl_percent);
  LOG_INIT_READY("Backlight");
}
#endif

void set_backlight_percent(uint8_t pct) {
  if (pct > 100) pct = 100;
  s_bl_percent = pct;
#if defined(ARDUINO_ARCH_ESP32)
  const uint32_t maxd = (uint32_t)Config::DISP_BACKLIGHT_MAX;
  const uint32_t duty = (pct * maxd + 50) / 100;
  bl_write_raw(duty);
#else
  const uint32_t duty = (pct * (uint32_t)Config::DISP_BACKLIGHT_MAX + 50) / 100;
  bl_write_raw(duty);
#endif
}

static constexpr uint8_t CMD_SLPIN   = 0x10;
static constexpr uint8_t CMD_SLPOUT  = 0x11;
static constexpr uint8_t CMD_DISPOFF = 0x28;
static constexpr uint8_t CMD_DISPON  = 0x29;

void display_sleep() {
  tft.startWrite(); tft.writeCommand(CMD_DISPOFF); tft.endWrite();
  delay(5);
  tft.startWrite(); tft.writeCommand(CMD_SLPIN);   tft.endWrite();
  delay(120);
  set_backlight_percent(0);
}

void display_wake() {
  tft.startWrite(); tft.writeCommand(CMD_SLPOUT);  tft.endWrite();
  delay(120);
  tft.startWrite(); tft.writeCommand(CMD_DISPON);  tft.endWrite();
  set_backlight_percent(s_bl_percent);  
}