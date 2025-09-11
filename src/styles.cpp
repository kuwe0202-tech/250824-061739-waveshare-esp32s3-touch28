#include "styles.h"

namespace UI {
  const uint32_t COL_BG = 0x000000;
  const uint32_t COL_FG = 0xFFFFFF;
  const uint32_t COL_ACCENT = 0x00FF54;

  const lv_font_t* FONT_SM = &lv_font_montserrat_16;
  const lv_font_t* FONT_MD = &lv_font_montserrat_18;
  const lv_font_t* FONT_LG = &lv_font_montserrat_34;

  void init_styles() {
    // Platz für Theme/Skins später
  }

  void label_fg(lv_obj_t* o, const lv_font_t* f) {
    if (!o) return;
    lv_obj_set_style_text_color(o, lv_color_hex(COL_FG), 0);
    if (f) lv_obj_set_style_text_font(o, f, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
  }

  void label_accent(lv_obj_t* o, const lv_font_t* f) {
    if (!o) return;
    lv_obj_set_style_text_color(o, lv_color_hex(COL_ACCENT), 0);
    if (f) lv_obj_set_style_text_font(o, f, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
  }

  void bar_accent(lv_obj_t* o) {
    if (!o) return;
    lv_obj_set_style_bg_color(o, lv_color_hex(COL_ACCENT), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(o, lv_color_hex(COL_ACCENT), LV_PART_INDICATOR | LV_STATE_DEFAULT);
  }
}