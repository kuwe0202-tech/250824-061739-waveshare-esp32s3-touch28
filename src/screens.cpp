/* === BEGIN FILE: src\screens.cpp === */
#include <string.h>
#include "screens.h"
#include "fonts.h"
#include "actions.h"
#include "styles.h"
#include "ui.h"
#include "RTC_PCF85063.h"
#include "ui_sd_manager.h" 
#include "Konfiguration.h"

objects_t objects;


static lv_obj_t* ui_mk_label(lv_obj_t* parent, const char* text, const lv_font_t* font, bool is_accent) {
    lv_obj_t* lbl = lv_label_create(parent);
    if (text) {
        lv_label_set_text(lbl, text);
    }
    if (is_accent) {
        UI::label_accent(lbl, font);
    } else {
        UI::label_fg(lbl, font);
    }
    return lbl;
}

static lv_obj_t* ui_mk_btn_with_label(lv_obj_t* parent, const char* text, lv_coord_t w, lv_coord_t h, lv_event_cb_t cb) {
    lv_obj_t* btn = lv_btn_create(parent);
    lv_obj_set_size(btn, w, h);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_ALL, NULL);
    lv_obj_set_style_bg_color(btn, lv_color_hex(UI::COL_BG), 0);
    
    lv_obj_t* lbl = ui_mk_label(btn, text, UI::FONT_MD, true);
    lv_obj_center(lbl);
    
    return btn;
}

namespace UI {

struct ClockTargets {
  lv_obj_t* lbl_time{nullptr};
  lv_obj_t* lbl_date{nullptr};
  lv_obj_t* lbl_dotw{nullptr};
};
static ClockTargets  g_clk;
static datetime_t    g_last{};
static volatile bool g_dirty = false;
static const char* dotw_name(uint8_t d) {
  static const char* N[7] = {"So","Mo","Di","Mi","Do","Fr","Sa"};
  return (d < 7) ? N[d] : "?";
}
static void clock_apply_() {
  if (!g_dirty) return;
  g_dirty = false;
  const datetime_t now = g_last;
  if (!rtc_has_valid_time()) {
    if (g_clk.lbl_time)  lv_label_set_text_static(g_clk.lbl_time, "--:--:--");
    if (g_clk.lbl_date)  lv_label_set_text_static(g_clk.lbl_date, "--.--.----");
    if (g_clk.lbl_dotw)  lv_label_set_text_static(g_clk.lbl_dotw, "--");
    return;
  }
  if (g_clk.lbl_time) lv_label_set_text_fmt(g_clk.lbl_time, "%02u:%02u:%02u", now.hour, now.minute, now.second);
  if (g_clk.lbl_date) lv_label_set_text_fmt(g_clk.lbl_date, "%02u.%02u.%04u",  now.day,  now.month,  now.year);
  if (g_clk.lbl_dotw) lv_label_set_text_static(g_clk.lbl_dotw, dotw_name(now.dotw));
}
static void clock_timer_cb(lv_timer_t*) { clock_apply_(); }
static lv_timer_t* s_clock_tmr = nullptr;
static void rtc_hook_(const datetime_t* now) {
  g_last = *now;
  g_dirty = true;
}
void clock_bind(const ClockTargets& t) {
  g_clk = t;
  rtc_set_update_hook(&rtc_hook_);
  if (!s_clock_tmr) s_clock_tmr = lv_timer_create(clock_timer_cb, 100, nullptr);
  g_dirty = true;
  clock_apply_();
}
}


#if CFG_BOOT_FSM
// HIER IST DIE FEHLENDE IMPLEMENTIERUNG
static int s_last_boot_percent = -1;
void ui_boot_screen_update_progress(int percent, const char* status_text) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    
    if (percent > s_last_boot_percent) {
        if (percent < s_last_boot_percent + CFG_BOOT_PROGRESS_SMOOTH_MIN_STEP_PCT) {
            percent = s_last_boot_percent + CFG_BOOT_PROGRESS_SMOOTH_MIN_STEP_PCT;
        }
        if (percent > 100) percent = 100;
    }
    s_last_boot_percent = percent;

    if (objects.boot_bar) {
        lv_bar_set_value(objects.boot_bar, percent, LV_ANIM_ON);
    }
    if (objects.boot_label_percent) {
        lv_label_set_text_fmt(objects.boot_label_percent, "%d%%", percent);
    }
    if (objects.boot_label_status) {
        lv_label_set_text(objects.boot_label_status, status_text);
    }
}
#endif


static void event_handler_cb_page_startbildschirm_page_startbildschirm(lv_event_t *e) {
    lv_event_code_t event = lv_event_get_code(e);
    if (event == LV_EVENT_SCREEN_LOADED) {
        loadScreen(SCREEN_ID_PAGE_DESKTOP);
    }
}
static void event_handler_cb_page_desktop_page_desktop(lv_event_t *e) {
    lv_event_code_t event = lv_event_get_code(e);
    if (event == LV_EVENT_GESTURE) {
        loadScreen(SCREEN_ID_PAGE_HAUPTMEN_);
    }
}
static void event_handler_cb_page_hauptmen__subbutton_licht(lv_event_t *e) {
    lv_event_code_t event = lv_event_get_code(e);
    if (event == LV_EVENT_CLICKED) {
        loadScreen(SCREEN_ID_PAGE_SUBMENU_LICHT);
    }
}
static void event_handler_cb_page_hauptmen__subbutton_luft(lv_event_t *e) {
    lv_event_code_t event = lv_event_get_code(e);
    if (event == LV_EVENT_CLICKED) {
        loadScreen(SCREEN_ID_PAGE_SUBMENU_LUFT);
    }
}
static void event_handler_cb_page_hauptmen__subbutton_boden(lv_event_t *e) {
    lv_event_code_t event = lv_event_get_code(e);
    if (event == LV_EVENT_CLICKED) {
        loadScreen(SCREEN_ID_PAGE_SUBMENU_BODEN);
    }
}
static void event_handler_cb_page_hauptmen__subbutton_wlan(lv_event_t *e) {
    lv_event_code_t event = lv_event_get_code(e);
    if (event == LV_EVENT_CLICKED) {
        loadScreen(SCREEN_ID_PAGE_SUBMENU_WLAN);
    }
}
static void event_handler_cb_page_hauptmen__subbutton_einstellungen(lv_event_t *e) {
    lv_event_code_t event = lv_event_get_code(e);
    if (event == LV_EVENT_CLICKED) {
        loadScreen(SCREEN_ID_PAGE_EINSTELLUNGEN);
    }
}
static void event_handler_cb_page_hauptmen__subbutton_zurueck_hauptmenu(lv_event_t *e) {
    lv_event_code_t event = lv_event_get_code(e);
    if (event == LV_EVENT_CLICKED) {
        loadScreen(SCREEN_ID_PAGE_DESKTOP);
    }
}
static void event_handler_cb_page_submenu_licht_button_zurueck_lichtmenu(lv_event_t *e) {
    lv_event_code_t event = lv_event_get_code(e);
    if (event == LV_EVENT_CLICKED) {
        loadScreen(SCREEN_ID_PAGE_HAUPTMEN_);
    }
}
static void event_handler_cb_page_submenu_luft_button_zurueck_luftmenu(lv_event_t *e) {
    lv_event_code_t event = lv_event_get_code(e);
    if (event == LV_EVENT_CLICKED) {
        loadScreen(SCREEN_ID_PAGE_HAUPTMEN_);
    }
}
static void event_handler_cb_page_submenu_boden_button_zurueck_bodenmenu(lv_event_t *e) {
    lv_event_code_t event = lv_event_get_code(e);
    if (event == LV_EVENT_CLICKED) {
        loadScreen(SCREEN_ID_PAGE_HAUPTMEN_);
    }
}
static void event_handler_cb_page_submenu_co2_button_zurueck_co2menu(lv_event_t *e) {
    lv_event_code_t event = lv_event_get_code(e);
    if (event == LV_EVENT_CLICKED) {
        loadScreen(SCREEN_ID_PAGE_HAUPTMEN_);
    }
}
static void event_handler_cb_page_submenu_wlan_subbutton_zurueck_4(lv_event_t *e) {
    lv_event_code_t event = lv_event_get_code(e);
    if (event == LV_EVENT_CLICKED) {
        loadScreen(SCREEN_ID_PAGE_HAUPTMEN_);
    }
}
static void event_handler_cb_page_einstellungen_button_zurueck_einstellungenmenu(lv_event_t *e) {
    lv_event_code_t event = lv_event_get_code(e);
    if (event == LV_EVENT_CLICKED) {
        loadScreen(SCREEN_ID_PAGE_HAUPTMEN_);
    }
}
static void event_handler_cb_page_einstellungen_uhrzeit_und_datum_einstellen(lv_event_t *e) {
    lv_event_code_t event = lv_event_get_code(e);
    if (event == LV_EVENT_CLICKED) {
        loadScreen(SCREEN_ID_PAGE_DATUM_EINSTELLEN);
    }
}
static void event_handler_cb_page_einstellungen_button_sd_karte_anzeigen(lv_event_t *e) {
    lv_event_code_t event = lv_event_get_code(e);
    if (event == LV_EVENT_CLICKED) {
        loadScreen(SCREEN_ID_PAGE_SD_KARTE_ANZEIGEN);
    }
}
static void event_handler_cb_page_datum_einstellen_button_zurueck_subsubmenu_uhrzeit_und_datum_einstellen(lv_event_t *e) {
    lv_event_code_t event = lv_event_get_code(e);
    if (event == LV_EVENT_CLICKED) {
        loadScreen(SCREEN_ID_PAGE_EINSTELLUNGEN);
    }
}
static void event_handler_cb_page_datum_einstellen_button_speichern_subsubmenu_uhrzeit_und_datum_einstellen(lv_event_t *e) {
    lv_event_code_t event = lv_event_get_code(e);
    if (event == LV_EVENT_PRESSED) {
        action_rtc_setze_datum_und_uhrzeit_aus_eez_roller(e);
    }
    if (event == LV_EVENT_CLICKED) {
        loadScreen(SCREEN_ID_PAGE_UHRZEIT_EINSTELLEN);
    }
}
static void event_handler_cb_page_uhrzeit_einstellen_button_zurueck_uhrzeit_einstellen(lv_event_t *e) {
    lv_event_code_t event = lv_event_get_code(e);
    if (event == LV_EVENT_CLICKED) {
        loadScreen(SCREEN_ID_PAGE_DATUM_EINSTELLEN);
    }
}
static void event_handler_cb_page_uhrzeit_einstellen_button_speichern_subsubmenu_uhrzeit_und_datum_einstellen_1(lv_event_t *e) {
    lv_event_code_t event = lv_event_get_code(e);
    if (event == LV_EVENT_PRESSED) {
        action_rtc_setze_datum_und_uhrzeit_aus_eez_roller(e);
    }
    if (event == LV_EVENT_CLICKED) {
        loadScreen(SCREEN_ID_PAGE_EINSTELLUNGEN);
    }
}


static void event_handler_cb_page_sd_karte_anzeigen_page_sd_karte_anzeigen(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_SCREEN_LOADED) {
        ui_sd_manager_enter_menu();
    } else if (code == LV_EVENT_SCREEN_UNLOAD_START) {
        ui_sd_manager_leave_menu();
    }
}
static void event_handler_cb_page_sd_karte_anzeigen_button_zurueck_einstellungenmenu_1(lv_event_t *e) {
    lv_event_code_t event = lv_event_get_code(e);
    if (event == LV_EVENT_CLICKED) {
        loadScreen(SCREEN_ID_PAGE_EINSTELLUNGEN);
    }
}

static void event_handler_cb_export_button(lv_event_t *e) {
    lv_event_code_t event = lv_event_get_code(e);
    if (event == LV_EVENT_CLICKED) {
        action_export_current_log_to_txt(e);
    }
}


void create_screen_page_startbildschirm() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.page_startbildschirm = obj;
    lv_obj_set_size(obj, 320, 240);
#if !CFG_BOOT_FSM
    // Registriere den Event-Handler nur, wenn der Bootloader NICHT aktiv ist.
    lv_obj_add_event_cb(obj, event_handler_cb_page_startbildschirm_page_startbildschirm, LV_EVENT_ALL, NULL);
#endif
    lv_obj_set_style_bg_color(obj, lv_color_hex(UI::COL_BG), 0);
    
    // UI-Elemente für den Ladebildschirm. Im alten Modus werden sie kurz angezeigt.
    objects.boot_bar = lv_bar_create(obj);
    lv_obj_set_size(objects.boot_bar, 280, 20);
    lv_obj_center(objects.boot_bar);
    lv_bar_set_range(objects.boot_bar, 0, 100);
    lv_bar_set_value(objects.boot_bar, 0, LV_ANIM_OFF);
    UI::bar_accent(objects.boot_bar);

    objects.boot_label_status = ui_mk_label(obj, "Initialisiere...", UI::FONT_SM, false);
    lv_obj_align_to(objects.boot_label_status, objects.boot_bar, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
    
    objects.boot_label_percent = ui_mk_label(obj, "0%", UI::FONT_MD, true);
    lv_obj_align_to(objects.boot_label_percent, objects.boot_bar, LV_ALIGN_OUT_TOP_MID, 0, -10);

    // Verstecke den "Controller V.1" Text, da der Platz jetzt vom Ladebalken genutzt wird.
    // objects.text_startbildschirm = ui_mk_label(obj, "Controller V.1", UI::FONT_LG, true);
    // lv_obj_center(objects.text_startbildschirm);
}
void tick_screen_page_startbildschirm() { }

void create_screen_page_desktop() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.page_desktop = obj;
    lv_obj_set_size(obj, 320, 240);
    lv_obj_add_event_cb(obj, event_handler_cb_page_desktop_page_desktop, LV_EVENT_ALL, NULL);
    lv_obj_set_style_bg_color(obj, lv_color_hex(UI::COL_BG), 0);
    
    objects.text_uhrzeit = ui_mk_label(obj, "--:--:--", UI::FONT_SM, false);
    lv_obj_set_pos(objects.text_uhrzeit, -1, 0);
    objects.text_datum = ui_mk_label(obj, "--.--.----", UI::FONT_SM, false);
    lv_obj_set_pos(objects.text_datum, 125, 0);
    objects.text_wochentag = ui_mk_label(obj, "--", UI::FONT_SM, false);
    lv_obj_set_pos(objects.text_wochentag, 99, 0);
    UI::clock_bind({objects.text_uhrzeit, objects.text_datum, objects.text_wochentag});

    objects.text_relative_luftfeuchte_innen = ui_mk_label(obj, "55%", UI::FONT_LG, true);
    lv_obj_set_pos(objects.text_relative_luftfeuchte_innen, 201, 38);
    objects.text_status_wlan = ui_mk_label(obj, "Wlan", UI::FONT_SM, false);
    lv_obj_set_pos(objects.text_status_wlan, 276, 0);
    objects.text_realative_luftfeuchte_zuluft = ui_mk_label(obj, "50%", UI::FONT_MD, true);
    lv_obj_set_pos(objects.text_realative_luftfeuchte_zuluft, 218, 76);
    
    objects.text_temperatur_innen = ui_mk_label(obj, u8"26Â°C", UI::FONT_LG, true);
    lv_obj_set_pos(objects.text_temperatur_innen, 48, 38);
    objects.text_temperatur_zuluft = ui_mk_label(obj, u8"22Â°C", UI::FONT_MD, true);
    lv_obj_set_pos(objects.text_temperatur_zuluft, 67, 76);
    
    lv_obj_t *line = lv_line_create(obj);
    objects.trennlinie_unter_der_zeit = line;
    static lv_point_t line_points[] = { { 2, 19 }, { 318, 19 } };
    lv_line_set_points(line, line_points, 2);
    lv_obj_set_style_line_color(line, lv_color_hex(UI::COL_ACCENT), 0);
    lv_obj_set_style_line_width(line, 1, 0);
    
    objects.text_lichtstaerke_in_ppfd = ui_mk_label(obj, "1300 PPFD", UI::FONT_SM, true);
    lv_obj_set_pos(objects.text_lichtstaerke_in_ppfd, 192, 120);
    objects.text_luefterleistung_in_m3 = ui_mk_label(obj, "750m3", UI::FONT_SM, true);
    lv_obj_set_pos(objects.text_luefterleistung_in_m3, 207, 160);
    
    objects.anzeigebalken_licht_dimmung = lv_bar_create(obj);
    lv_obj_set_pos(objects.anzeigebalken_licht_dimmung, 36, 120);
    lv_obj_set_size(objects.anzeigebalken_licht_dimmung, 150, 20);
    lv_bar_set_value(objects.anzeigebalken_licht_dimmung, 100, LV_ANIM_OFF);
    UI::bar_accent(objects.anzeigebalken_licht_dimmung);
    
    objects.anzeigebalken_luefter_dimmung = lv_bar_create(obj);
    lv_obj_set_pos(objects.anzeigebalken_luefter_dimmung, 36, 159);
    lv_obj_set_size(objects.anzeigebalken_luefter_dimmung, 150, 20);
    lv_bar_set_value(objects.anzeigebalken_luefter_dimmung, 75, LV_ANIM_OFF);
    UI::bar_accent(objects.anzeigebalken_luefter_dimmung);
    
    objects.anzeigebalken_erdfeuchte = lv_bar_create(obj);
    lv_obj_set_pos(objects.anzeigebalken_erdfeuchte, 36, 198);
    lv_obj_set_size(objects.anzeigebalken_erdfeuchte, 150, 20);
    lv_bar_set_value(objects.anzeigebalken_erdfeuchte, 50, LV_ANIM_OFF);
    UI::bar_accent(objects.anzeigebalken_erdfeuchte);
    
    objects.text_erdefeucht_in_prozent = ui_mk_label(obj, "50%", UI::FONT_SM, true);
    lv_obj_set_pos(objects.text_erdefeucht_in_prozent, 218, 199);
    objects.text_sd_aktiv = ui_mk_label(obj, "SD", UI::FONT_SM, false);
    lv_obj_set_pos(objects.text_sd_aktiv, 235, -1);
}
void tick_screen_page_desktop() { }




void create_screen_page_sd_karte_anzeigen() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.page_sd_karte_anzeigen = obj;
    lv_obj_set_size(obj, 320, 240);
    
    lv_obj_add_event_cb(obj, event_handler_cb_page_sd_karte_anzeigen_page_sd_karte_anzeigen, LV_EVENT_ALL, NULL);
    lv_obj_set_style_bg_color(obj, lv_color_hex(UI::COL_BG), 0);

    objects.text_sd_karte_erkannt_nicht_erkannt = ui_mk_label(obj, "Lade Status...", UI::FONT_MD, true);
    lv_obj_set_pos(objects.text_sd_karte_erkannt_nicht_erkannt, 95, 25);
    objects.text_freier_speicher = ui_mk_label(obj, "Freier Speicher: ...", UI::FONT_SM, true);
    lv_obj_set_pos(objects.text_freier_speicher, 8, 55);
    objects.text_belegter_speicher = ui_mk_label(obj, "Belegter Speicher: ...", UI::FONT_SM, true);
    lv_obj_set_pos(objects.text_belegter_speicher, 8, 75);
    objects.text_letzter_log = ui_mk_label(obj, "Letzter Log: ...", UI::FONT_SM, true);
    lv_obj_set_pos(objects.text_letzter_log, 8, 95);

    objects.button_export_txt = ui_mk_btn_with_label(obj, "TXT exportieren", 150, 42, event_handler_cb_export_button);
    lv_obj_set_pos(objects.button_export_txt, 8, 130);
    
    objects.label_export_status = ui_mk_label(obj, "", UI::FONT_SM, false);
    lv_obj_align_to(objects.label_export_status, objects.button_export_txt, LV_ALIGN_OUT_RIGHT_MID, 10, 0);
    lv_obj_add_flag(objects.label_export_status, LV_OBJ_FLAG_HIDDEN);

    objects.button_zurueck_einstellungenmenu_1 = ui_mk_btn_with_label(obj, "Zurück", 120, 42, event_handler_cb_page_sd_karte_anzeigen_button_zurueck_einstellungenmenu_1);
    lv_obj_set_pos(objects.button_zurueck_einstellungenmenu_1, 95, 190);
}
void tick_screen_page_sd_karte_anzeigen() { }
void create_screen_page_hauptmen_() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.page_hauptmen_ = obj;
    lv_obj_set_size(obj, 320, 240);
    lv_obj_set_style_bg_color(obj, lv_color_hex(UI::COL_BG), 0);
    
    objects.subbutton_licht = ui_mk_btn_with_label(obj, "Licht", 120, 50, event_handler_cb_page_hauptmen__subbutton_licht);
    lv_obj_set_pos(objects.subbutton_licht, 20, 20);

    objects.subbutton_luft = ui_mk_btn_with_label(obj, "Luft", 120, 50, event_handler_cb_page_hauptmen__subbutton_luft);
    lv_obj_set_pos(objects.subbutton_luft, 180, 20);

    objects.subbutton_boden = ui_mk_btn_with_label(obj, "Boden", 120, 50, event_handler_cb_page_hauptmen__subbutton_boden);
    lv_obj_set_pos(objects.subbutton_boden, 20, 95);

    objects.subbutton_wlan = ui_mk_btn_with_label(obj, "Wlan", 120, 50, event_handler_cb_page_hauptmen__subbutton_wlan);
    lv_obj_set_pos(objects.subbutton_wlan, 180, 95);

    objects.subbutton_einstellungen = ui_mk_btn_with_label(obj, "Einstellungen", 120, 50, event_handler_cb_page_hauptmen__subbutton_einstellungen);
    lv_obj_set_pos(objects.subbutton_einstellungen, 180, 170);

    objects.subbutton_zurueck_hauptmenu = ui_mk_btn_with_label(obj, "Zurück", 120, 50, event_handler_cb_page_hauptmen__subbutton_zurueck_hauptmenu);
    lv_obj_set_pos(objects.subbutton_zurueck_hauptmenu, 20, 170);
}
void tick_screen_page_hauptmen_() { }

void create_screen_page_submenu_licht() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.page_submenu_licht = obj;
    lv_obj_set_size(obj, 320, 240);
    lv_obj_set_style_bg_color(obj, lv_color_hex(UI::COL_BG), 0);
    
    objects.text_page_submenu_licht = ui_mk_label(obj, "Hier kommt das Lichtmenü", UI::FONT_MD, true);
    lv_obj_center(objects.text_page_submenu_licht);

    objects.button_zurueck_lichtmenu = ui_mk_btn_with_label(obj, "Zurück", 120, 50, event_handler_cb_page_submenu_licht_button_zurueck_lichtmenu);
    lv_obj_set_pos(objects.button_zurueck_lichtmenu, 100, 171);
}
void tick_screen_page_submenu_licht() { }

void create_screen_page_submenu_luft() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.page_submenu_luft = obj;
    lv_obj_set_size(obj, 320, 240);
    lv_obj_set_style_bg_color(obj, lv_color_hex(UI::COL_BG), 0);

    objects.text_page_submenu_luft = ui_mk_label(obj, "Hier kommt das Luftmenü", UI::FONT_MD, true);
    lv_obj_center(objects.text_page_submenu_luft);

    objects.button_zurueck_luftmenu = ui_mk_btn_with_label(obj, "Zurück", 120, 50, event_handler_cb_page_submenu_luft_button_zurueck_luftmenu);
    lv_obj_set_pos(objects.button_zurueck_luftmenu, 100, 165);
}
void tick_screen_page_submenu_luft() { }

void create_screen_page_submenu_boden() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.page_submenu_boden = obj;
    lv_obj_set_size(obj, 320, 240);
    lv_obj_set_style_bg_color(obj, lv_color_hex(UI::COL_BG), 0);

    objects.text_page_submenu_boden = ui_mk_label(obj, "Hier kommt das Bodenmenü", UI::FONT_MD, true);
    lv_obj_center(objects.text_page_submenu_boden);

    objects.button_zurueck_bodenmenu = ui_mk_btn_with_label(obj, "Zurück", 120, 50, event_handler_cb_page_submenu_boden_button_zurueck_bodenmenu);
    lv_obj_set_pos(objects.button_zurueck_bodenmenu, 100, 170);
}
void tick_screen_page_submenu_boden() { }

void create_screen_page_submenu_co2() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.page_submenu_co2 = obj;
    lv_obj_set_size(obj, 320, 240);
    lv_obj_set_style_bg_color(obj, lv_color_hex(UI::COL_BG), 0);

    objects.text_page_submenu_co2 = ui_mk_label(obj, "Hier kommt das CO2-Menü", UI::FONT_MD, true);
    lv_obj_center(objects.text_page_submenu_co2);

    objects.button_zurueck_co2menu = ui_mk_btn_with_label(obj, "Zurück", 120, 50, event_handler_cb_page_submenu_co2_button_zurueck_co2menu);
    lv_obj_set_pos(objects.button_zurueck_co2menu, 100, 170);
}
void tick_screen_page_submenu_co2() { }

void create_screen_page_submenu_wlan() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.page_submenu_wlan = obj;
    lv_obj_set_size(obj, 320, 240);
    lv_obj_set_style_bg_color(obj, lv_color_hex(UI::COL_BG), 0);

    objects.text_page_submenu_wlan = ui_mk_label(obj, "Hier kommt das WLAN-Menü", UI::FONT_MD, true);
    lv_obj_center(objects.text_page_submenu_wlan);

    objects.subbutton_zurueck_4 = ui_mk_btn_with_label(obj, "Zurück", 120, 50, event_handler_cb_page_submenu_wlan_subbutton_zurueck_4);
    lv_obj_set_pos(objects.subbutton_zurueck_4, 100, 170);
}
void tick_screen_page_submenu_wlan() { }

void create_screen_page_einstellungen() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.page_einstellungen = obj;
    lv_obj_set_size(obj, 320, 240);
    lv_obj_set_style_bg_color(obj, lv_color_hex(UI::COL_BG), 0);

    objects.uhrzeit_und_datum_einstellen = ui_mk_btn_with_label(obj, "Datum und\n   Uhrzeit", 120, 55, event_handler_cb_page_einstellungen_uhrzeit_und_datum_einstellen);
    lv_obj_set_pos(objects.uhrzeit_und_datum_einstellen, 100, 16);

    objects.button_sd_karte_anzeigen = ui_mk_btn_with_label(obj, "SD Karte", 120, 55, event_handler_cb_page_einstellungen_button_sd_karte_anzeigen);
    lv_obj_set_pos(objects.button_sd_karte_anzeigen, 100, 93);

    objects.button_zurueck_einstellungenmenu = ui_mk_btn_with_label(obj, "Zurück", 120, 42, event_handler_cb_page_einstellungen_button_zurueck_einstellungenmenu);
    lv_obj_set_pos(objects.button_zurueck_einstellungenmenu, 100, 168);
}
void tick_screen_page_einstellungen() { }

void create_screen_page_datum_einstellen() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.page_datum_einstellen = obj;
    lv_obj_set_size(obj, 320, 240);
    lv_obj_set_style_bg_color(obj, lv_color_hex(UI::COL_BG), 0);

    objects.button_zurueck_subsubmenu_uhrzeit_und_datum_einstellen = ui_mk_btn_with_label(obj, "Zurück", 120, 50, event_handler_cb_page_datum_einstellen_button_zurueck_subsubmenu_uhrzeit_und_datum_einstellen);
    lv_obj_set_pos(objects.button_zurueck_subsubmenu_uhrzeit_und_datum_einstellen, 31, 172);

    objects.button_speichern_subsubmenu_uhrzeit_und_datum_einstellen = ui_mk_btn_with_label(obj, "Speichern", 120, 50, event_handler_cb_page_datum_einstellen_button_speichern_subsubmenu_uhrzeit_und_datum_einstellen);
    lv_obj_set_pos(objects.button_speichern_subsubmenu_uhrzeit_und_datum_einstellen, 177, 172);

    objects.text_roller_einstellungen_tag = ui_mk_label(obj, "d.d.", UI::FONT_MD, true);
    lv_obj_set_pos(objects.text_roller_einstellungen_tag, 44, 60);
    objects.text_roller_einstellungen_monat = ui_mk_label(obj, "m.m.", UI::FONT_MD, true);
    lv_obj_set_pos(objects.text_roller_einstellungen_monat, 131, 60);
    objects.text_roller_einstellungen_jahr = ui_mk_label(obj, "j.j.j.j.", UI::FONT_MD, true);
    lv_obj_set_pos(objects.text_roller_einstellungen_jahr, 228, 60);

    objects.roller_einstellungen_tag = lv_roller_create(obj);
    lv_obj_set_pos(objects.roller_einstellungen_tag, 44, 98);
    lv_obj_set_size(objects.roller_einstellungen_tag, 40, 40);
    lv_roller_set_options(objects.roller_einstellungen_tag, "01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23\n24\n25\n26\n27\n28\n29\n30\n31", LV_ROLLER_MODE_NORMAL);
    lv_obj_set_style_text_color(objects.roller_einstellungen_tag, lv_color_hex(UI::COL_ACCENT), LV_PART_SELECTED);
    lv_obj_set_style_bg_opa(objects.roller_einstellungen_tag, LV_OPA_TRANSP, 0);

    objects.roller_einstellungen_monat = lv_roller_create(obj);
    lv_obj_set_pos(objects.roller_einstellungen_monat, 131, 98);
    lv_obj_set_size(objects.roller_einstellungen_monat, 40, 40);
    lv_roller_set_options(objects.roller_einstellungen_monat, "01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12", LV_ROLLER_MODE_NORMAL);
    lv_obj_set_style_text_color(objects.roller_einstellungen_monat, lv_color_hex(UI::COL_ACCENT), LV_PART_SELECTED);
    lv_obj_set_style_bg_opa(objects.roller_einstellungen_monat, LV_OPA_TRANSP, 0);

    objects.roller_einstellungen_jahr = lv_roller_create(obj);
    lv_obj_set_pos(objects.roller_einstellungen_jahr, 206, 98);
    lv_obj_set_size(objects.roller_einstellungen_jahr, 80, 40);
    lv_roller_set_options(objects.roller_einstellungen_jahr, "2020\n2021\n2022\n2023\n2024\n2025\n2026\n2027\n2028\n2029\n2030\n2031\n2032\n2033\n2034\n2035\n2036\n2037\n2038\n2039\n2040\n2041\n2042\n2043\n2044\n2045\n2046\n2047\n2048\n2049\n2050", LV_ROLLER_MODE_NORMAL);
    lv_obj_set_style_text_color(objects.roller_einstellungen_jahr, lv_color_hex(UI::COL_ACCENT), LV_PART_SELECTED);
    lv_obj_set_style_bg_opa(objects.roller_einstellungen_jahr, LV_OPA_TRANSP, 0);
}
void tick_screen_page_datum_einstellen() { }

void create_screen_page_uhrzeit_einstellen() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.page_uhrzeit_einstellen = obj;
    lv_obj_set_size(obj, 320, 240);
    lv_obj_set_style_bg_color(obj, lv_color_hex(UI::COL_BG), 0);

    objects.button_zurueck_uhrzeit_einstellen = ui_mk_btn_with_label(obj, "Abbrechen", 120, 50, event_handler_cb_page_uhrzeit_einstellen_button_zurueck_uhrzeit_einstellen);
    lv_obj_set_pos(objects.button_zurueck_uhrzeit_einstellen, 7, 175);

    objects.button_speichern_subsubmenu_uhrzeit_und_datum_einstellen_1 = ui_mk_btn_with_label(obj, "Speichern", 120, 50, event_handler_cb_page_uhrzeit_einstellen_button_speichern_subsubmenu_uhrzeit_und_datum_einstellen_1);
    lv_obj_set_pos(objects.button_speichern_subsubmenu_uhrzeit_und_datum_einstellen_1, 185, 175);

    objects.text_roller_einstellungen_stunde = ui_mk_label(obj, "hh", UI::FONT_MD, true);
    lv_obj_set_pos(objects.text_roller_einstellungen_stunde, 87, 64);
    objects.text_roller_einstellungen_minuten = ui_mk_label(obj, "mm", UI::FONT_MD, true);
    lv_obj_set_pos(objects.text_roller_einstellungen_minuten, 206, 64);

    objects.roller_einstellungen_stunden = lv_roller_create(obj);
    lv_obj_set_pos(objects.roller_einstellungen_stunden, 59, 100);
    lv_obj_set_size(objects.roller_einstellungen_stunden, 80, 40);
    lv_roller_set_options(objects.roller_einstellungen_stunden, "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23", LV_ROLLER_MODE_NORMAL);
    lv_obj_set_style_text_color(objects.roller_einstellungen_stunden, lv_color_hex(UI::COL_ACCENT), LV_PART_SELECTED);
    lv_obj_set_style_bg_opa(objects.roller_einstellungen_stunden, LV_OPA_TRANSP, 0);

    objects.roller_einstellungen_minuten = lv_roller_create(obj);
    lv_obj_set_pos(objects.roller_einstellungen_minuten, 185, 100);
    lv_obj_set_size(objects.roller_einstellungen_minuten, 80, 40);
    lv_roller_set_options(objects.roller_einstellungen_minuten, "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23\n24\n25\n26\n27\n28\n29\n30\n31\n32\n33\n34\n35\n36\n37\n38\n39\n40\n41\n42\n43\n44\n45\n46\n47\n48\n49\n50\n51\n52\n53\n54\n55\n56\n57\n58\n59", LV_ROLLER_MODE_NORMAL);
    lv_obj_set_style_text_color(objects.roller_einstellungen_minuten, lv_color_hex(UI::COL_ACCENT), LV_PART_SELECTED);
    lv_obj_set_style_bg_opa(objects.roller_einstellungen_minuten, LV_OPA_TRANSP, 0);
}
void tick_screen_page_uhrzeit_einstellen() { }




typedef void (*tick_screen_func_t)();
tick_screen_func_t tick_screen_funcs[] = {
    tick_screen_page_startbildschirm,
    tick_screen_page_desktop,
    tick_screen_page_hauptmen_,
    tick_screen_page_submenu_licht,
    tick_screen_page_submenu_luft,
    tick_screen_page_submenu_boden,
    tick_screen_page_submenu_co2,
    tick_screen_page_submenu_wlan,
    tick_screen_page_einstellungen,
    tick_screen_page_datum_einstellen,
    tick_screen_page_uhrzeit_einstellen,
    tick_screen_page_sd_karte_anzeigen,
};

void tick_screen(int screen_index) {
    if (screen_index >= 0 && (unsigned)screen_index < sizeof(tick_screen_funcs) / sizeof(tick_screen_func_t)) {
        tick_screen_funcs[screen_index]();
    }
}

void create_screens() {
    create_screen_page_startbildschirm();
    create_screen_page_desktop();
    create_screen_page_hauptmen_();
    create_screen_page_submenu_licht();
    create_screen_page_submenu_luft();
    create_screen_page_submenu_boden();
    create_screen_page_submenu_co2();
    create_screen_page_submenu_wlan();
    create_screen_page_einstellungen();
    create_screen_page_datum_einstellen();
    create_screen_page_uhrzeit_einstellen();
    create_screen_page_sd_karte_anzeigen();
}