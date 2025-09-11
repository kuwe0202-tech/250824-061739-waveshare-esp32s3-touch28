/* === BEGIN FILE: include\SD_Karten_Treiber.h === */
#ifndef SD_KARTEN_TREIBER_H
#define SD_KARTEN_TREIBER_H

#include <stdint.h>
#include <stdbool.h>
#include <FS.h> 

#define FS_REASON_SD_MOUNTED "sd_mounted"
#define FS_REASON_SD_UNMOUNT "sd_unmount"
#define FS_REASON_FS_ACTIVITY "fs_activity"

// NEU: Definition an zentraler Stelle
#define CD_DEBOUNCE_MS 10

#ifdef __cplusplus
extern "C" {
#endif


typedef enum {
    SD_UPDATE_REASON_BOOT,
    SD_UPDATE_REASON_MENU_TICK,
    SD_UPDATE_REASON_MENU_EXIT,
    SD_UPDATE_REASON_LOG_FLUSH,
    SD_UPDATE_REASON_MANUAL,
    SD_UPDATE_REASON_EXPORT_TXT_MANUAL, 
    SD_UPDATE_REASON_FINALIZE_DAY       
} sd_update_reason_t;



typedef struct {
    sd_update_reason_t reason;
    char payload[96]; 
} sd_job_t;


typedef struct {
    bool     mounted;
    uint64_t free_bytes;
    uint64_t total_bytes;
    uint64_t last_successful_probe_ms;
} sd_status_cache_t;


typedef void (*fs_flush_hook_t)(const char* reason);
void fs_flush_hook_register(fs_flush_hook_t fn);


void sd_worker_init(void);



// NEU: Erweiterte API für den Bootloader FSM
typedef enum {
    PHASE_MOUNT_BEGIN,
    PHASE_MOUNT_END_SUCCESS,
    PHASE_MOUNT_END_ERROR,
    PHASE_PROBE_BEGIN,
    PHASE_PROBE_END_SUCCESS,
    PHASE_PROBE_END_ERROR,
    PHASE_UNMOUNT_BEGIN,
    PHASE_UNMOUNT_END_SUCCESS,
} sd_observer_event_t;


typedef void (*sd_observer_callback_t)(sd_observer_event_t event);
typedef void (*sd_progress_callback_t)(uint32_t current_bytes, uint32_t total_bytes);


void sd_register_observer(sd_observer_callback_t cb);
void sd_register_progress_callback(sd_progress_callback_t cb);
bool sd_is_idle(void);
void sd_stats(uint32_t* retries, int32_t* last_err, uint32_t* last_change_ms);



bool sd_try_get_status_cache(sd_status_cache_t* out_cache);
bool sd_global_io_lock(uint32_t timeout_ms);
void sd_global_io_unlock(void);
void sd_telemetry_take_and_reset(uint32_t* out_ops, uint64_t* out_bytes);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

void sd_request_job(const sd_job_t& job);


static inline void sd_request_update(sd_update_reason_t reason){
    sd_job_t j; 
    j.reason = reason; 
    j.payload[0] = '\0';
    sd_request_job(j);
}

// HIER DIE FEHLENDE DEKLARATION HINZUFÜGEN
bool sd_preallocate_file(File& file, size_t size);
#endif


#endif
/* === END FILE: include\SD_Karten_Treiber.h === */