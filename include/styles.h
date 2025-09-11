#pragma once
#include <lvgl.h>

namespace UI {
  // Farb-/Font-Tokens
  extern const uint32_t COL_BG, COL_FG, COL_ACCENT;
  extern const lv_font_t* FONT_SM; // 16
  extern const lv_font_t* FONT_MD; // 18
  extern const lv_font_t* FONT_LG; // 34

  // Einmalig beim UI-Start
  void init_styles();

  // Helfer (sparren viel Boilerplate)
  void label_fg(lv_obj_t* o, const lv_font_t* f=nullptr);
  void label_accent(lv_obj_t* o, const lv_font_t* f=nullptr);
  void bar_accent(lv_obj_t* o);
}