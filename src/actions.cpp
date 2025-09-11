/* === BEGIN FILE: src\actions.cpp === */
#include "actions.h"
#include <stdio.h>

#include "RTC_PCF85063.h"
#include "runtime_tasks.h"
#include "SD_Karten_Treiber.h"
#include "ui_sd_manager.h"
#include "Log.h"
#include "screens.h"
#include <string.h>


extern objects_t objects;

void action_rtc_setze_datum_und_uhrzeit_aus_eez_roller(lv_event_t * e) {
    rtc_apply_from_ui_rollers();
    (void)e;
}


void action_export_current_log_to_txt(lv_event_t * e) {
    if (!e || lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    // NEU: Debouncing, um mehrfache Klicks zu verhindern
    static uint32_t last_export_request_ms = 0;
    if (millis() - last_export_request_ms < 5000) { 
        LOGI_T("actions", "Export-Anfrage ignoriert (Debouncing).");
        if (objects.label_export_status) {
            lv_label_set_text(objects.label_export_status, "Bitte warten...");
            lv_obj_clear_flag(objects.label_export_status, LV_OBJ_FLAG_HIDDEN);
        }
        return;
    }
    last_export_request_ms = millis();

    
    if (objects.button_export_txt) {
        lv_obj_add_state(objects.button_export_txt, LV_STATE_DISABLED);
    }
    if (objects.label_export_status) {
        lv_label_set_text(objects.label_export_status, "Export startet...");
        lv_obj_clear_flag(objects.label_export_status, LV_OBJ_FLAG_HIDDEN);
    }
    
    
    last_log_info_t info;
    log_get_last_log_info(&info);

    if (!info.valid || info.filename[0] == '\0' || strstr(info.filename, ".tmp") == NULL) {
        LOGW_T("actions", "Keine gültige, aktive Log-Datei für den Export gefunden.");
        if (objects.label_export_status) {
            lv_label_set_text(objects.label_export_status, "Kein Log aktiv");
        }
        
        return;
    }
    
    
    sd_job_t job;
    job.reason = SD_UPDATE_REASON_EXPORT_TXT_MANUAL;
    
    
    
    strncpy(job.payload, info.filename, sizeof(job.payload) - 1);
    job.payload[sizeof(job.payload)-1] = '\0';

    
    LOGI_T("actions", "Stößt manuellen Export für '%s' an.", job.payload);
    sd_request_job(job);
}
/* === END FILE: src\actions.cpp === */