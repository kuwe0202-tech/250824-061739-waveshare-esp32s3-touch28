/* === BEGIN FILE: src\Log.cpp === */

#include "Log.h"
#include "log/log_internal.hpp"
#include "SD_Karten_Treiber.h"

// --- Öffentliche API-Fassade ---

void log_init(void) {
    log_internal::init();
    fs_flush_hook_register(log_internal::log_fs_hook_cb);
}

void log_set_rtc_provider(RtcProvider provider) {
    log_internal::set_rtc_provider(provider);
}

void log_write(uint8_t level, const char* tag, const char* msg) {
    log_internal::write(level, tag, msg);
}

void log_flush(const char* reason) {
    log_internal::flush(reason);
}

void log_force_flush(void) {
    log_internal::force_flush();
}

void log_rotate_maybe(void) {
    log_internal::rotate_maybe();
}

bool log_readback_tail(char* dst, size_t dst_len, size_t* out_len) {
    return log_internal::readback_tail(dst, dst_len, out_len);
}

void log_sd_status_changed(bool mounted_ok) {
    log_internal::sd_status_changed(mounted_ok);
}

void log_notify_rtc_valid(void) {
    log_internal::log_on_rtc_valid();
}

void log_get_last_log_info(last_log_info_t* info) {
    log_internal::get_last_log_info(info);
}

bool log_run_export_for_file(const char* bin_log_path, bool export_txt, bool export_jsonl) {
    return log_internal::run_export_for_file(bin_log_path, export_txt, export_jsonl);
}

// --- NEU: Implementierung der Fassade für den Boot-Prozess ---
#if CFG_BOOT_FSM
bool log_boot_preallocate(void) {
    return log_internal::boot_preallocate();
}

void log_boot_adopt_preallocated_file(void) {
    log_internal::boot_adopt_preallocated_file();
}

void log_register_progress_callback(log_progress_callback_t cb) {
    // Diese Funktion muss noch intern implementiert werden, falls benötigt.
}
#endif

// --- Globale Funktionen, die von außerhalb aufgerufen werden (z.B. Task Ticker) ---

void run_flush_fsm(void) {
    log_internal::run_flush_fsm();
}

/* === END FILE: src\Log.cpp === */