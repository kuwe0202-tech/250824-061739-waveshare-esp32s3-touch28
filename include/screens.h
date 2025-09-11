/* === BEGIN FILE: include\screens.h === */
#ifndef EEZ_LVGL_UI_SCREENS_H
#define EEZ_LVGL_UI_SCREENS_H

#include <lvgl.h>
#include "Konfiguration.h" 

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _objects_t {
    lv_obj_t *page_startbildschirm;
    lv_obj_t *page_desktop;
    lv_obj_t *page_hauptmen_;
    lv_obj_t *page_submenu_licht;
    lv_obj_t *page_submenu_luft;
    lv_obj_t *page_submenu_boden;
    lv_obj_t *page_submenu_co2;
    lv_obj_t *page_submenu_wlan;
    lv_obj_t *page_einstellungen;
    lv_obj_t *page_datum_einstellen;
    lv_obj_t *page_uhrzeit_einstellen;
    lv_obj_t *page_sd_karte_anzeigen;
    lv_obj_t *subbutton_licht;
    lv_obj_t *subbutton_luft;
    lv_obj_t *subbutton_boden;
    lv_obj_t *subbutton_wlan;
    lv_obj_t *subbutton_einstellungen;
    lv_obj_t *subbutton_zurueck_hauptmenu;
    lv_obj_t *button_zurueck_lichtmenu;
    lv_obj_t *button_zurueck_luftmenu;
    lv_obj_t *button_zurueck_bodenmenu;
    lv_obj_t *button_zurueck_co2menu;
    lv_obj_t *subbutton_zurueck_4;
    lv_obj_t *button_zurueck_einstellungenmenu;
    lv_obj_t *uhrzeit_und_datum_einstellen;
    lv_obj_t *button_sd_karte_anzeigen;
    lv_obj_t *button_zurueck_subsubmenu_uhrzeit_und_datum_einstellen;
    lv_obj_t *button_speichern_subsubmenu_uhrzeit_und_datum_einstellen;
    lv_obj_t *button_zurueck_uhrzeit_einstellen;
    lv_obj_t *button_speichern_subsubmenu_uhrzeit_und_datum_einstellen_1;
    lv_obj_t *button_zurueck_einstellungenmenu_1;
    
    // UI-Elemente für den Startbildschirm aus deinem Code
    lv_obj_t *boot_bar;
    lv_obj_t *boot_label_percent;
    lv_obj_t *boot_label_status;
    lv_obj_t *text_uhrzeit;
    lv_obj_t *text_datum;
    lv_obj_t *text_relative_luftfeuchte_innen;
    lv_obj_t *text_status_wlan;
    lv_obj_t *text_realative_luftfeuchte_zuluft;
    lv_obj_t *text_temperatur_innen;
    lv_obj_t *text_temperatur_zuluft;
    lv_obj_t *trennlinie_unter_der_zeit;
    lv_obj_t *text_lichtstaerke_in_ppfd;
    lv_obj_t *text_luefterleistung_in_m3;
    lv_obj_t *anzeigebalken_licht_dimmung;
    lv_obj_t *anzeigebalken_luefter_dimmung;
    lv_obj_t *anzeigebalken_erdfeuchte;
    lv_obj_t *text_erdefeucht_in_prozent;
    lv_obj_t *text_wochentag;
    lv_obj_t *text_sd_aktiv;
    lv_obj_t *text_subbutton_licht;
    lv_obj_t *text_subbutton_luft;
    lv_obj_t *text_subbutton_boden;
    lv_obj_t *text_subbutton_wlan;
    lv_obj_t *text_subbutton_einstellungen;
    lv_obj_t *text_subbutton_zuruck;
    lv_obj_t *text_page_submenu_licht;
    lv_obj_t *text_button_zurueck_lichtmenu;
    lv_obj_t *text_page_submenu_luft;
    lv_obj_t *text_button_zurueck_lufttmenu;
    lv_obj_t *text_page_submenu_boden;
    lv_obj_t *text_button_zurueck_bodenmenu;
    lv_obj_t *text_page_submenu_co2;
    lv_obj_t *text_button_zurueck_co2menu;
    lv_obj_t *text_page_submenu_wlan;
    lv_obj_t *text_button_zurueck_wlanmenu;
    lv_obj_t *text_button_zurueck_einstellungenmenu;
    lv_obj_t *text_uhrzeit_und_datum_einstellen;
    lv_obj_t *text_sd_karte_anzeigen;
    lv_obj_t *text_button_zurueck_subsubmenu_uhrzeit_und_datum_einstellen;
    lv_obj_t *text_button_speichern_subsubmenu_uhrzeit_und_datum_einstellen;
    lv_obj_t *roller_einstellungen_tag;
    lv_obj_t *text_roller_einstellungen_tag;
    lv_obj_t *text_roller_einstellungen_monat;
    lv_obj_t *roller_einstellungen_monat;
    lv_obj_t *roller_einstellungen_jahr;
    lv_obj_t *text_roller_einstellungen_jahr;
    lv_obj_t *text_button_zurueck_subsubmenu_uhrzeit_und_datum_einstellen_1;
    lv_obj_t *roller_einstellungen_stunden;
    lv_obj_t *roller_einstellungen_minuten;
    lv_obj_t *text_roller_einstellungen_stunde;
    lv_obj_t *text_roller_einstellungen_minuten;
    lv_obj_t *text_button_speichern_subsubmenu_uhrzeit_und_datum_einstellen_1;
    lv_obj_t *text_sd_karte_erkannt_nicht_erkannt;
    lv_obj_t *text_freier_speicher;
    lv_obj_t *text_belegter_speicher;
    lv_obj_t *text_letzter_log;
    
    lv_obj_t *button_export_txt;
    lv_obj_t *label_export_status;
} objects_t;


extern objects_t objects;

enum ScreensEnum {
    SCREEN_ID_PAGE_STARTBILDSCHIRM = 1,
    SCREEN_ID_PAGE_DESKTOP = 2,
    SCREEN_ID_PAGE_HAUPTMEN_ = 3,
    SCREEN_ID_PAGE_SUBMENU_LICHT = 4,
    SCREEN_ID_PAGE_SUBMENU_LUFT = 5,
    SCREEN_ID_PAGE_SUBMENU_BODEN = 6,
    SCREEN_ID_PAGE_SUBMENU_CO2 = 7,
    SCREEN_ID_PAGE_SUBMENU_WLAN = 8,
    SCREEN_ID_PAGE_EINSTELLUNGEN = 9,
    SCREEN_ID_PAGE_DATUM_EINSTELLEN = 10,
    SCREEN_ID_PAGE_UHRZEIT_EINSTELLEN = 11,
    SCREEN_ID_PAGE_SD_KARTE_ANZEIGEN = 12,
};

void create_screens();
void tick_screen(int screen_index);


#if CFG_BOOT_FSM
void ui_boot_screen_update_progress(int percent, const char* status_text);
#endif


void create_screen_page_startbildschirm();
void tick_screen_page_startbildschirm();
void create_screen_page_desktop();
void tick_screen_page_desktop();
void create_screen_page_hauptmen_();
void tick_screen_page_hauptmen_();
void create_screen_page_submenu_licht();
void tick_screen_page_submenu_licht();
void create_screen_page_submenu_luft();
void tick_screen_page_submenu_luft();
void create_screen_page_submenu_boden();
void tick_screen_page_submenu_boden();
void create_screen_page_submenu_co2();
void tick_screen_page_submenu_co2();
void create_screen_page_submenu_wlan();
void tick_screen_page_submenu_wlan();
void create_screen_page_einstellungen();
void tick_screen_page_einstellungen();
void create_screen_page_datum_einstellen();
void tick_screen_page_datum_einstellen();
void create_screen_page_uhrzeit_einstellen();
void tick_screen_page_uhrzeit_einstellen();
void create_screen_page_sd_karte_anzeigen();
void tick_screen_page_sd_karte_anzeigen();

#ifdef __cplusplus
}
#endif

#endif
/* === END FILE: include\screens.h === */