/* === BEGIN FILE: include\ui_sd_manager.h === */
#ifndef UI_SD_MANAGER_H
#define UI_SD_MANAGER_H


#ifdef __cplusplus
extern "C" {
#endif

void ui_sd_manager_init();
void ui_sd_manager_enter_menu();
void ui_sd_manager_leave_menu();
void ui_sd_manager_notify_log_flush();


// NEU: Funktionen zur Steuerung eines modalen Hotplug/Progress-Dialogs
void ui_sd_manager_show_hotplug_dialog(const char* title);
void ui_sd_manager_update_hotplug_progress(int percent, const char* status_text);
void ui_sd_manager_hide_hotplug_dialog();


void ui_sd_manager_set_export_status_running();
void ui_sd_manager_set_export_status_success();
void ui_sd_manager_set_export_status_failed();

#ifdef __cplusplus
}
#endif

#endif
/* === END FILE: include\ui_sd_manager.h === */