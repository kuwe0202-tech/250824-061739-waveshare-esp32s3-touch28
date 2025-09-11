/* === BEGIN FILE: src\ui_sd_manager.cpp === */
#include "ui_sd_manager.h"
#include "screens.h"
#include "Log.h"
#include "runtime_tasks.h" 
#include "Konfiguration.h"
#include "styles.h"
#include "LVGL_Treiber.h" 
#include <lvgl.h>
#include <inttypes.h>
#include <cstdio>
#include <string.h> 
#include <atomic>


enum class ExportState : uint8_t {
    IDLE = 0,
    RUNNING,
    SUCCESS,
    FAILED
};
static std::atomic<ExportState> s_export_state{ExportState::IDLE};
static uint32_t s_export_status_display_until = 0;

static lv_timer_t* s_menu_timer = NULL;
static lv_timer_t* s_desktop_timer = NULL;


// NEU: UI-Objekte und Logik für den Hotplug/Progress-Dialog
static lv_obj_t* s_hotplug_overlay = NULL;
static lv_obj_t* s_hotplug_bar = NULL;
static lv_obj_t* s_hotplug_label_status = NULL;
static int s_last_hotplug_percent = -1;

void ui_sd_manager_show_hotplug_dialog(const char* title) {
    if (!s_hotplug_overlay) {
        s_hotplug_overlay = ui_create_modal_overlay(lv_layer_top());

        lv_obj_t* cont = lv_obj_create(s_hotplug_overlay);
        lv_obj_set_size(cont, 300, 120);
        lv_obj_center(cont);
        lv_obj_set_style_bg_color(cont, lv_color_hex(0x333333), 0);
        lv_obj_set_style_border_color(cont, lv_color_hex(UI::COL_ACCENT), 0);
        lv_obj_set_style_border_width(cont, 2, 0);

        lv_obj_t* title_label = lv_label_create(cont);
        UI::label_accent(title_label, UI::FONT_MD);
        lv_label_set_text(title_label, title);
        lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 10);
        
        s_hotplug_bar = lv_bar_create(cont);
        lv_obj_set_size(s_hotplug_bar, 260, 15);
        lv_obj_align(s_hotplug_bar, LV_ALIGN_CENTER, 0, 5);
        lv_bar_set_range(s_hotplug_bar, 0, 100);
        UI::bar_accent(s_hotplug_bar);

        s_hotplug_label_status = lv_label_create(cont);
        UI::label_fg(s_hotplug_label_status, UI::FONT_SM);
        
        lv_obj_align_to(s_hotplug_label_status, s_hotplug_bar, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
    }
    
    s_last_hotplug_percent = -1;
    lv_bar_set_value(s_hotplug_bar, 0, LV_ANIM_OFF);
    lv_label_set_text(s_hotplug_label_status, "Initialisiere...");
    lv_obj_clear_flag(s_hotplug_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_timer_handler(); 
}

void ui_sd_manager_update_hotplug_progress(int percent, const char* status_text) {
    if (!s_hotplug_overlay || lv_obj_has_flag(s_hotplug_overlay, LV_OBJ_FLAG_HIDDEN)) {
        return; 
    }

    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    
    
    if (percent > s_last_hotplug_percent) {
        if (percent < s_last_hotplug_percent + CFG_BOOT_PROGRESS_SMOOTH_MIN_STEP_PCT) {
            percent = s_last_hotplug_percent + CFG_BOOT_PROGRESS_SMOOTH_MIN_STEP_PCT;
        }
        if (percent > 100) percent = 100;
    }
    s_last_hotplug_percent = percent;
    
    if (s_hotplug_bar) {
        lv_bar_set_value(s_hotplug_bar, percent, LV_ANIM_ON);
    }
    if (s_hotplug_label_status) {
        lv_label_set_text(s_hotplug_label_status, status_text);
    }
}

void ui_sd_manager_hide_hotplug_dialog() {
    if (s_hotplug_overlay) {
        lv_obj_add_flag(s_hotplug_overlay, LV_OBJ_FLAG_HIDDEN);
    }
}


static void update_desktop_indicator();
static void update_menu_labels();


extern "C" void ui_sd_manager_set_export_status_running() {
    s_export_state.store(ExportState::RUNNING, std::memory_order_relaxed);
}
extern "C" void ui_sd_manager_set_export_status_success() {
    s_export_state.store(ExportState::SUCCESS, std::memory_order_relaxed);
    s_export_status_display_until = millis() + 3000; 
}
extern "C" void ui_sd_manager_set_export_status_failed() {
    s_export_state.store(ExportState::FAILED, std::memory_order_relaxed);
    s_export_status_display_until = millis() + 3000; 
}



static void format_timespan(char* buf, size_t len, uint32_t total_seconds) {
    if (total_seconds < 60) {
        snprintf(buf, len, "vor %lu s", (unsigned long)total_seconds);
    } else if (total_seconds < 3600) {
        uint32_t minutes = total_seconds / 60;
        uint32_t seconds = total_seconds % 60;
        snprintf(buf, len, "vor %lu min %lu s", (unsigned long)minutes, (unsigned long)seconds);
    } else if (total_seconds < 86400) {
        uint32_t hours = total_seconds / 3600;
        uint32_t minutes = (total_seconds % 3600) / 60;
        snprintf(buf, len, "vor %lu h %lu min", (unsigned long)hours, (unsigned long)minutes);
    } else {
        snprintf(buf, len, "vor > 24 h");
    }
}


static const char* level_to_str_ui(log_level_t level) {
    switch (level) {
        case LOG_LVL_ERR: return "[ERR]";
        case LOG_LVL_WRN: return "[WRN]";
        case LOG_LVL_INF: return "[INF]";
        case LOG_LVL_DBG: return "[DBG]";
        default: return "[?]";
    }
}

static void menu_timer_cb(lv_timer_t* timer) {
    (void)timer;
    update_menu_labels();
}

static void desktop_timer_cb(lv_timer_t* timer) {
    (void)timer;
    update_desktop_indicator();
}

void ui_sd_manager_init() {
    s_desktop_timer = lv_timer_create(desktop_timer_cb, 1000, NULL);
}

void ui_sd_manager_enter_menu() {
    if (s_menu_timer) lv_timer_del(s_menu_timer);
    s_menu_timer = lv_timer_create(menu_timer_cb, 500, NULL); 
    s_export_state.store(ExportState::IDLE, std::memory_order_relaxed); 
    menu_timer_cb(s_menu_timer); 
}

void ui_sd_manager_leave_menu() {
    if (s_menu_timer) {
        lv_timer_del(s_menu_timer);
        s_menu_timer = NULL;
    }
}

void ui_sd_manager_notify_log_flush() {
    
}

static void update_desktop_indicator() {
    if (!objects.text_sd_aktiv) return;
    
    bool sd_is_ok = telemetry_sd_ok();
    lv_color_t col = sd_is_ok ? lv_color_hex(0x00FF54) : lv_color_hex(0xFF0000); 
    lv_obj_set_style_text_color(objects.text_sd_aktiv, col, 0);
    lv_label_set_text(objects.text_sd_aktiv, "SD");
}

static void update_menu_labels() {
    char buf[128];
    bool sd_is_ok = telemetry_sd_ok();
    
    if (objects.text_sd_karte_erkannt_nicht_erkannt) {
        const char* status_text;
        if (sd_is_ok) {
            status_text = "SD Karte aktiv";
        } else if (telemetry_sd_is_stale()) {
            status_text = "Status wird gelesen...";
        } else {
            status_text = "SD Karte nicht erkannt";
        }
        lv_label_set_text(objects.text_sd_karte_erkannt_nicht_erkannt, status_text);
    }
    
    if (objects.text_freier_speicher) {
        uint32_t free_mb = telemetry_fs_free_mb();
        if (sd_is_ok) snprintf(buf, sizeof(buf), "Freier Speicher: %" PRIu32 " MB", free_mb);
        else strcpy(buf, "Freier Speicher: ---");
        lv_label_set_text(objects.text_freier_speicher, buf);
    }
    
    if (objects.text_belegter_speicher) {
        uint32_t total_mb = telemetry_fs_total_mb();
        uint32_t free_mb = telemetry_fs_free_mb();
        
        if (sd_is_ok && total_mb > 0) {
            uint32_t used_mb = total_mb > free_mb ? total_mb - free_mb : 0;
            snprintf(buf, sizeof(buf), "Belegter Speicher: %" PRIu32 " MB", used_mb);
        } else {
            strcpy(buf, "Belegter Speicher: ---");
        }
        lv_label_set_text(objects.text_belegter_speicher, buf);
    }

    if (objects.text_letzter_log) {
        last_log_info_t log_info;
        log_get_last_log_info(&log_info);

        if (log_info.valid && log_info.last_write_ms > 0) {
            uint32_t age_seconds = (millis() - log_info.last_write_ms) / 1000;
            char timespan_buf[32];
            format_timespan(timespan_buf, sizeof(timespan_buf), age_seconds);
            
            snprintf(buf, sizeof(buf), "Letzter Log: %s, %s %s", 
                     log_info.filename, 
                     level_to_str_ui(log_info.last_level),
                     timespan_buf);
        } else if (log_info.valid) {
            snprintf(buf, sizeof(buf), "Letzter Log: %s, noch kein Schreibvorgang", log_info.filename);
        }
        else {
            strcpy(buf, "Letzter Log: N/A");
        }
        lv_label_set_text(objects.text_letzter_log, buf);
    }

    
    if (objects.button_export_txt && objects.label_export_status) {
        ExportState current_state = s_export_state.load(std::memory_order_relaxed);

        switch(current_state) {
            case ExportState::IDLE:
                lv_obj_clear_state(objects.button_export_txt, LV_STATE_DISABLED);
                lv_obj_add_flag(objects.label_export_status, LV_OBJ_FLAG_HIDDEN);
                break;

            case ExportState::RUNNING:
                lv_obj_add_state(objects.button_export_txt, LV_STATE_DISABLED);
                lv_label_set_text(objects.label_export_status, "Exportiert...");
                lv_obj_clear_flag(objects.label_export_status, LV_OBJ_FLAG_HIDDEN);
                break;

            case ExportState::SUCCESS:
                lv_label_set_text(objects.label_export_status, "Erfolgreich!");
                lv_obj_clear_flag(objects.label_export_status, LV_OBJ_FLAG_HIDDEN);
                if ((long)(millis() - s_export_status_display_until) >= 0) {
                    s_export_state.store(ExportState::IDLE, std::memory_order_relaxed);
                }
                break;

            case ExportState::FAILED:
                lv_label_set_text(objects.label_export_status, "Fehler!");
                lv_obj_clear_flag(objects.label_export_status, LV_OBJ_FLAG_HIDDEN);
                 if ((long)(millis() - s_export_status_display_until) >= 0) {
                    s_export_state.store(ExportState::IDLE, std::memory_order_relaxed);
                }
                break;
        }
    }
}
/* === END FILE: src\ui_sd_manager.cpp === */