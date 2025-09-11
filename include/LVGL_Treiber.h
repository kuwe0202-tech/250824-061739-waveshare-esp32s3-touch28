/* === BEGIN FILE: include\LVGL_Treiber.h === */
#ifndef LVGL_TREIBER_H
#define LVGL_TREIBER_H

#include "PlatformConfig.h"
#include "Konfiguration.h"
#include <lvgl.h>
#include <cstdint>

#define LVGL_UPDATE_TICK_MS Config::LVGL_TICK_MS


// NEU: Deklaration für eine UI-Hilfsfunktion
lv_obj_t* ui_create_modal_overlay(lv_obj_t* parent);


void Lvgl_print(const char * buf);
void Lvgl_Display_LCD(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p);
void Lvgl_Touchpad_Read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data);
void Lvgl_TickHandler(void *arg);
void Lvgl_Init(void);
void Lvgl_Loop(void);

uint16_t Lvgl_GetFPS();
uint8_t  Lvgl_GetLoopCpuPct();

struct LvglRenderStats {
  uint32_t samples;        
  uint32_t ms_avg;         
  uint32_t ms_max;         
  uint32_t px_avg;         
  uint16_t fill_pct_avg;   
  uint32_t full_flush_cnt; 
};

void Lvgl_GetRenderStats(LvglRenderStats* out);
void Lvgl_ResetRenderStats();

struct LvglMemStats {
  size_t used;
  size_t peak;
  size_t blocks;
  size_t max_alloc;
  uint8_t frag_est_pct; 
};


#if LV_MEM_CUSTOM
extern "C" size_t lvgl_mem_stat_used();
extern "C" size_t lvgl_mem_stat_peak();
extern "C" size_t lvgl_mem_stat_blocks();
extern "C" size_t lvgl_mem_stat_max_alloc();
#endif

inline bool Lvgl_MemGetStats(LvglMemStats* out) {
#if LV_MEM_CUSTOM
  if (!out) return false;
  out->used      = lvgl_mem_stat_used();
  out->peak      = lvgl_mem_stat_peak();
  out->blocks    = lvgl_mem_stat_blocks();
  out->max_alloc = lvgl_mem_stat_max_alloc();
  out->frag_est_pct = (out->used > 0 && out->max_alloc < out->used)
                      ? (uint8_t)((100u * (out->used - out->max_alloc)) / out->used)
                      : 0u;
  return true;
#else
  (void)out; return false;
#endif
}

#endif
/* === END FILE: include\LVGL_Treiber.h === */