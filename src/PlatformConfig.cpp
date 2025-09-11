#include "PlatformConfig.h"
#if defined(ARDUINO_ARCH_ESP32)
  #include <driver/spi_common.h>
#else
  // Fallback-Definitionen für Nicht-ESP32-Plattformen
  typedef int spi_host_device_t;
  enum { SPI1_HOST = 0, SPI2_HOST = 1, SPI3_HOST = 2 };
#endif

// Definition der globalen Konfigurationsinstanz mit positionsbasierter Initialisierung (C++17-kompatibel)
PlatformConfig platform = {
    // Display
    DisplayDriver::ST7789, 320, 240, 1,
    40, 45, -1, 42, 41, 39, 5,
    SPI3_HOST, 80000000, 16000000,

    // Touch
    TouchDriver::CST328, true, false, true,
    0, 240, 0, 320,
    1, 1, 3, 4, 2, 0x1A,

// SD (1-Bit)
SdMode::SDMMC, BusWidth::BW1, 20000000,
14, 17, 16, -1, -1, 21, -1,
-1, -1, -1, -1, -1,

    // RTC
    0, 11, 10, 0x51,

    // System
    true, 2500, 5000
};

PlatformConfig probe_platform() {
    return platform;
}