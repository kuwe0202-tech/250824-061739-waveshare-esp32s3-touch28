/* === BEGIN FILE: src/nvs_ab_fallback.cpp === */
#include <Arduino.h>
extern "C" {
  #include <nvs.h>
  #include <nvs_flash.h>
}

extern "C" bool __attribute__((weak))
nvs_ab_read_u64(const char* key, uint64_t* out) {
  if (!key) return false;

  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    nvs_flash_erase();
    err = nvs_flash_init();
  }
  if (err != ESP_OK) { if (out) *out = 0; return false; }

  nvs_handle_t h;
  // Namespace an nvs_cfg_ab.cpp angleichen
  err = nvs_open("sys", NVS_READONLY, &h);
  if (err != ESP_OK) { if (out) *out = 0; return false; }

  uint64_t v = 0;
  err = nvs_get_u64(h, key, &v);
  nvs_close(h);

  if (err == ESP_OK) { if (out) *out = v; return true; }
  if (out) *out = 0;
  return false;
}

extern "C" bool __attribute__((weak))
nvs_ab_write_u64(const char* key, uint64_t v) {
  if (!key) return false;

  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    nvs_flash_erase();
    err = nvs_flash_init();
  }
  if (err != ESP_OK) return false;

  nvs_handle_t h;
  // Namespace an nvs_cfg_ab.cpp angleichen
  err = nvs_open("sys", NVS_READWRITE, &h);
  if (err != ESP_OK) return false;

  err = nvs_set_u64(h, key, v);
  if (err == ESP_OK) err = nvs_commit(h);
  nvs_close(h);
  return (err == ESP_OK);
}
/* === END FILE: src/nvs_ab_fallback.cpp === */
