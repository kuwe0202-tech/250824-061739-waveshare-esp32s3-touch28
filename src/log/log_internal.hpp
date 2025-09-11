/* === BEGIN FILE: src\log\log_internal.hpp === */

#ifndef LOG_INTERNAL_HPP
#define LOG_INTERNAL_HPP

// ACHTUNG: Dieser Header ist privat für das Log-Modul.
// Nicht außerhalb von src/log/ inkludieren!

#include <stdint.h>
#include <cstddef>
#include <atomic>
#include <mutex>
#include <FS.h>

#include "Konfiguration.h"
#include <esp_attr.h>

#include "Log.h"
#include "boot_header.h"

#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>

#include "log_format.hpp"
#include "log_checksum.hpp"

extern RTC_NOINIT_ATTR PanicHdr g_panic_hdr;
extern RTC_NOINIT_ATTR uint8_t  g_panic_tail_buffer[CFG_PANIC_TAIL_MAX];

namespace log_internal {

struct State {
    portMUX_TYPE ring_spinlock = portMUX_INITIALIZER_UNLOCKED;
    uint8_t      log_ring_buffer[CFG_LOG_RING_BYTES];
    size_t       ring_head = 0;
    size_t       ring_tail = 0;
    std::atomic<uint32_t> log_seq_counter{0};

    enum class BackpressureState : uint8_t { OFF, LEVEL_1, LEVEL_2 };
    std::atomic<BackpressureState> backpressure_state{BackpressureState::OFF};
    std::atomic<bool> uart_fallback_active{false};
    uint32_t uart_tokens = CFG_LOG_UART_BOOT_BURST;
    uint32_t last_uart_refill_ms = 0;

    std::mutex flush_mutex;
    std::atomic<bool> sd_mounted{false};
    std::atomic<bool> flush_request_pending{false};
    uint32_t last_flush_ms = 0;
    log_level_t last_flushed_level = LOG_LVL_DBG;
    log_level_t last_enqueued_level = LOG_LVL_DBG;

    std::atomic<uint32_t> sd_evt_ms{0};
    std::atomic<bool>     sd_abort{false};
    
    std::atomic<bool> recover_pending{false};
    std::atomic<bool> close_file_pending{false};

    std::atomic<uint32_t> evt_bits{0};
    RtcProvider rtc_provider = nullptr;

    File     active_log_tmp_file;
    char     active_log_basename[96] = {0};
    uint32_t current_log_offset = 0;
    uint32_t current_page_seq = 0;
    uint32_t current_tail_seq = 0;
    bool     log_day_is_active = false;
    bool     log_header_anchored = false;

    bool     header_needs_reanchor = false;
    int64_t  wall_offset_us = 0;
};

State& state();

// --- Interne Funktionsdeklarationen ---

void init();
void set_rtc_provider(RtcProvider provider);
void write(uint8_t level, const char* tag, const char* msg);
void get_last_log_info(last_log_info_t* info);
void flush(const char* reason);
void ring_read_at(size_t* idx, void* d, size_t len);
size_t get_ring_used_unsafe();
void force_flush();
void rotate_maybe();
void run_flush_fsm();
void sd_status_changed(bool mounted_ok);
void log_fs_hook_cb(const char* r);
bool log_commit_tail();
void recover_on_boot();
bool run_export_for_file(const char* bin_log_path, bool export_txt, bool export_jsonl);
bool readback_tail(char* dst, size_t dst_len, size_t* out_len);

// Boot-Prozess Funktionen
bool boot_preallocate(void);
void boot_adopt_preallocated_file(void);

void log_on_rtc_valid();

} // namespace log_internal

#endif // LOG_INTERNAL_HPP

/* === END FILE: src\log\log_internal.hpp === */