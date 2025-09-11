/* === BEGIN FILE: src\SD_Karten_Treiber.cpp === */

#include "SD_Karten_Treiber.h"
#include <Arduino.h>
#include <algorithm>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <atomic>
#include <SD_MMC.h>
#include "PlatformConfig.h"
#include "Log.h"
#include "fs_policy.h"
#include "fs_paths.h"
#include "runtime_tasks.h" 
#include "ui_sd_manager.h" 
#include "runtime_globals.h" 


#define JOB_QUEUE_LENGTH 10
#define PROBE_BYTES_MIN_INTERVAL_MS 5000
#define MOUNT_RETRY_BACKOFF_BASE_MS 2000
#define MOUNT_RETRY_BACKOFF_MAX_MS 30000
#define DEDUPE_WINDOW_MS 500
#define WORKER_STACK_SIZE 8192


typedef struct {
    std::atomic<bool> mounted;
    uint64_t free_bytes;
    uint64_t total_bytes;
    uint32_t last_successful_probe_ms;
    std::atomic<uint32_t> generation;
} sd_internal_status_t;
static sd_internal_status_t s_status{false, 0, 0, 0, std::atomic<uint32_t>{0}};


static SemaphoreHandle_t s_global_io_mutex = NULL;
static QueueHandle_t s_job_queue = NULL;
static TaskHandle_t s_worker_handle = NULL;
static std::atomic<uint32_t> s_last_request_ms{0};
static uint32_t s_next_mount_attempt_ms = 0;
static std::atomic<uint32_t> s_queue_send_errors = {0};
static std::atomic<uint32_t> s_cb_queue_drops = {0};
static std::atomic<uint32_t> s_io_ops = {0};
static std::atomic<uint64_t> s_io_bytes = {0};
static std::atomic<uint32_t> s_last_fs_hook_ms{0};


static std::atomic<fs_flush_hook_t> s_flush_hook{nullptr};

enum FsReason : uint8_t { FS_ACTIVITY, FS_SD_MOUNTED, FS_SD_UNMOUNT };
enum CallbackKind : uint8_t { CB_FS_HOOK, CB_LOG_STATE };
struct CallbackMsg {
    CallbackKind kind;
    union {
        FsReason reason;
        bool mounted_ok;
    };
};

static QueueHandle_t s_cb_queue = NULL;
static TaskHandle_t s_cb_task_handle = NULL;

static void worker_perform_unmount(const char* reason); 

static inline const char* fs_reason_str(FsReason r){
  static constexpr const char* k[] = {
    FS_REASON_FS_ACTIVITY,
    FS_REASON_SD_MOUNTED,
    FS_REASON_SD_UNMOUNT
  };

  static_assert(FS_SD_UNMOUNT + 1 == sizeof(k)/sizeof(k[0]), "FsReason map out of sync");
  return (unsigned)r < (sizeof(k)/sizeof(k[0])) ? k[(unsigned)r] : "";
}

static void callback_task(void*){
    for(;;){
        CallbackMsg msg;
        if (xQueueReceive(s_cb_queue, &msg, portMAX_DELAY) == pdPASS){
            if (msg.kind == CB_FS_HOOK) {
                auto fn = s_flush_hook.load(std::memory_order_acquire);
                if (fn) fn(fs_reason_str(msg.reason));
            } else {
                log_sd_status_changed(msg.mounted_ok);
            }
            taskYIELD();
        }
    }
}

extern "C" void fs_flush_hook_register(fs_flush_hook_t fn) {
    s_flush_hook.store(fn, std::memory_order_release);
}


static void log_queue_drop(const char* queue_name) {
    static std::atomic<uint32_t> drops{0};
    uint32_t n = drops.fetch_add(1, std::memory_order_relaxed) + 1;
    if (n % 100 == 1) {
        LOGW_T("sd_worker", "Callback-Queue '%s' voll (drops=%u)", queue_name, n);
    }
}

static inline void schedule_fs_event(FsReason r) {
    if (!s_cb_queue) return;
    CallbackMsg m; m.kind = CB_FS_HOOK; m.reason = r;

    if (xQueueSend(s_cb_queue, &m, 0) != pdPASS) {
        log_queue_drop("fs_event");
    }
}


static inline void schedule_log_state(bool mounted_ok) {
    if (!s_cb_queue) return;
    CallbackMsg m; m.kind = CB_LOG_STATE; m.mounted_ok = mounted_ok;

    while (xQueueSend(s_cb_queue, &m, 0) != pdPASS) {
        CallbackMsg dump;
        if (xQueueReceive(s_cb_queue, &dump, 0) != pdPASS) {
            if (xQueueSend(s_cb_queue, &m, pdMS_TO_TICKS(50)) == pdPASS) break;
            log_queue_drop("log_state_critical");
            break;
        }
    }
}


static void sd_worker_task(void* pvParameters);
static bool worker_perform_mount();
static void worker_perform_probe_bytes();


void sd_worker_init(void) {
    LOG_INIT_BEGIN("SD-Worker");


    if (platform.pin_sd_cd >= 0) pinMode(platform.pin_sd_cd, INPUT_PULLUP);

    s_global_io_mutex = xSemaphoreCreateMutex();
    s_job_queue = xQueueCreate(JOB_QUEUE_LENGTH, sizeof(sd_job_t));
    s_cb_queue = xQueueCreate(16, sizeof(CallbackMsg));

    if (!s_global_io_mutex || !s_job_queue || !s_cb_queue) {
        LOG_INIT_FAIL("SD-Worker", "Mutex/Queue creation failed");
        return;
    }

    BaseType_t ok1 = xTaskCreatePinnedToCore(sd_worker_task, "sd_worker", WORKER_STACK_SIZE, NULL, 4, &s_worker_handle, 0);
    BaseType_t ok2 = xTaskCreatePinnedToCore(callback_task, "sd_callbacks", 8192, NULL, 3, &s_cb_task_handle, tskNO_AFFINITY);

    if (ok1 != pdPASS || ok2 != pdPASS) {
        LOG_INIT_FAIL("SD-Worker", "Task creation failed (sd:%d cb:%d)", (int)ok1, (int)ok2);
        return;
    }

    if (ok2 == pdPASS) {
        LOGI_T("rtos", "cb_high_water=%u", (unsigned)uxTaskGetStackHighWaterMark(s_cb_task_handle));
    }

    LOG_INIT_READY("SD-Worker");
    sd_request_update(SD_UPDATE_REASON_BOOT);
}

void sd_request_job(const sd_job_t& job) {
    if (s_job_queue) {
        uint32_t now = millis();
        uint32_t prev = s_last_request_ms.load(std::memory_order_relaxed);
        if ((int32_t)(now - prev) < (int32_t)DEDUPE_WINDOW_MS &&
            job.reason != SD_UPDATE_REASON_BOOT && job.reason != SD_UPDATE_REASON_FINALIZE_DAY) return;
        s_last_request_ms.store(now, std::memory_order_relaxed);

        if (xQueueSend(s_job_queue, &job, 0) != pdPASS) {
            uint32_t error_count = s_queue_send_errors.fetch_add(1, std::memory_order_relaxed) + 1;
            LOGW("SD-Worker job queue full! Total drops: %u", error_count);
        }
    }
}

bool sd_try_get_status_cache(sd_status_cache_t* out_cache) {
    if (!out_cache) return false;
    uint32_t gen1, gen2;
    do {
        gen1 = s_status.generation.load(std::memory_order_acquire);
        if (gen1 & 1) { taskYIELD(); continue; }
        out_cache->mounted = s_status.mounted.load(std::memory_order_acquire);
        out_cache->free_bytes = s_status.free_bytes;
        out_cache->total_bytes = s_status.total_bytes;
        out_cache->last_successful_probe_ms = s_status.last_successful_probe_ms;
        gen2 = s_status.generation.load(std::memory_order_acquire);
    } while (gen1 != gen2);
    return true;
}

bool sd_global_io_lock(uint32_t timeout_ms) {
    if (!s_global_io_mutex) return false;
    return xSemaphoreTake(s_global_io_mutex, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}
void sd_global_io_unlock(void) {
    if (s_global_io_mutex) xSemaphoreGive(s_global_io_mutex);
}

extern "C" void sd_telemetry_take_and_reset(uint32_t* out_ops, uint64_t* out_bytes) {
    if (out_ops)  *out_ops  = s_io_ops.exchange(0, std::memory_order_relaxed);
    if (out_bytes) *out_bytes = s_io_bytes.exchange(0, std::memory_order_relaxed);
}

// KORRIGIERTE VERSION DER FUNKTION
void sd_telemetry_count_write(size_t nbytes) {
    s_io_ops.fetch_add(1, std::memory_order_relaxed);
    s_io_bytes.fetch_add(nbytes, std::memory_order_relaxed);

    // NEU: Sicherer Zugriff mit .load()
    if (g_boot_phase_active.load() || !s_status.mounted.load(std::memory_order_relaxed)) {
        return;
    
    }

    uint32_t now = millis();
    uint32_t last = s_last_fs_hook_ms.load(std::memory_order_relaxed);
    if ((int32_t)(now - last) >= 50) {
        s_last_fs_hook_ms.store(now, std::memory_order_relaxed);
        schedule_fs_event(FS_ACTIVITY);
    }
}

bool sd_preallocate_file(File& file, size_t size) {
    if (!file) return false;
    static uint8_t zero_buf[4096] = {0};

    size_t written = 0;
    while (written < size) {
        tel_watchdog_mark_alive(); 

        if (!s_status.mounted.load(std::memory_order_relaxed)) {
            LOGW_T("prealloc", "Abbruch: SD nicht (mehr) gemountet.");
            return false;
        }

        if (!sd_global_io_lock(1000)) {
            vTaskDelay(pdMS_TO_TICKS(5)); 
            continue;
        }

        size_t bytes_to_write = std::min((size_t)sizeof(zero_buf), size - written);
        size_t bytes_written = file.write(zero_buf, bytes_to_write);
        
        sd_global_io_unlock(); 

        if (bytes_written != bytes_to_write) {
            LOGW_T("prealloc", "Schreibfehler! Erw.: %u, Geschr.: %u -> Unmount", (unsigned)bytes_to_write, (unsigned)bytes_written);
            worker_perform_unmount("prealloc_write_err");
            return false;
        }

        sd_telemetry_count_write(bytes_written);
        written += bytes_written;
        
        if (written > 0 && written % (256 * 1024) == 0) {
            if (sd_global_io_lock(1000)) {
                fs_fsync(file);
                schedule_fs_event(FS_ACTIVITY);
                sd_global_io_unlock();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1)); 
    }

    if (sd_global_io_lock(1000)) {
        fs_fsync(file);
        file.seek(0);
        sd_global_io_unlock();
    }
    
    return true;
}


static void sd_worker_task(void* pvParameters) {
    (void)pvParameters;

    static uint32_t mount_backoff_ms = MOUNT_RETRY_BACKOFF_BASE_MS;

    for (;;) {
        sd_job_t job;
        if (xQueueReceive(s_job_queue, &job, pdMS_TO_TICKS(500)) != pdPASS) {
            job.reason = SD_UPDATE_REASON_MANUAL;
            job.payload[0] = '\0';
        }

        switch (job.reason) {
            case SD_UPDATE_REASON_EXPORT_TXT_MANUAL: {
                ui_sd_manager_set_export_status_running(); 
                LOGI_T("sd_worker", "Manueller TXT-Export-Job gestartet.");
                
                bool export_ok = log_run_export_for_file(job.payload, true, false);
                
                if (export_ok) {
                    ui_sd_manager_set_export_status_success();
                } else {
                    ui_sd_manager_set_export_status_failed();
                }
                LOGI_T("sd_worker", "Manueller TXT-Export für '%s' beendet mit Status: %s.", job.payload, export_ok ? "ERFOLG" : "FEHLER");
                continue; 
            }

            case SD_UPDATE_REASON_FINALIZE_DAY: {
                LOGI_T("sd_worker", "Tagesabschluss-Job für '%s' gestartet.", job.payload);
                if (job.payload[0] != '\0') {
                    bool ok = log_run_export_for_file(job.payload, true, true);
                    if (ok) {
                        LOGI_T("sd_worker", "Export von '%s' erfolgreich, lösche Quelldatei.", job.payload);
                        
                        if (sd_global_io_lock(1000)) {
                            fs_policy_remove(job.payload); 
                            sd_global_io_unlock();
                        } else {
                            LOGE_T("sd_worker", "Konnte Lock zum Löschen von '%s' nicht erhalten.", job.payload);
                        }
                    } else {
                        LOGE_T("sd_worker", "Export von '%s' fehlgeschlagen. Quelldatei wird beibehalten.", job.payload);
                    }
                } else {
                    LOGE_T("sd_worker", "FINALIZE_DAY-Job ohne Dateipfad erhalten!");
                }
                continue; 
            }
            
            default:
                break;
        }

        bool card_present = (platform.pin_sd_cd < 0);
        if (platform.pin_sd_cd >= 0) {
            bool p1 = (digitalRead(platform.pin_sd_cd) == LOW);
            vTaskDelay(pdMS_TO_TICKS(CD_DEBOUNCE_MS));
            bool p2 = (digitalRead(platform.pin_sd_cd) == LOW);
            card_present = p1 && p2;
        }

        if (!card_present) {
            if (s_status.mounted.load(std::memory_order_acquire)) worker_perform_unmount("card_removed");
            vTaskDelay(pdMS_TO_TICKS(2));
            continue;
        }

        if (!s_status.mounted.load(std::memory_order_acquire)) {
            if ((int32_t)(millis() - s_next_mount_attempt_ms) >= 0) {
                if (worker_perform_mount()) {
                    mount_backoff_ms = MOUNT_RETRY_BACKOFF_BASE_MS;
                    worker_perform_probe_bytes();
                } else {
                    s_next_mount_attempt_ms = millis() + mount_backoff_ms;
                    mount_backoff_ms = std::min(mount_backoff_ms * 2, (uint32_t)MOUNT_RETRY_BACKOFF_MAX_MS);
                    uint32_t jitter = (xTaskGetTickCount() & 0xFF) % 250;
                    s_next_mount_attempt_ms += jitter;
                    LOGW_T("sd_worker", "mount retry in %u ms", (unsigned)(mount_backoff_ms + jitter));
                }
            }
        } else {
            bool needs_probe = (job.reason == SD_UPDATE_REASON_MANUAL) ||
                               (job.reason == SD_UPDATE_REASON_LOG_FLUSH) ||
                               ((int32_t)(millis() - s_status.last_successful_probe_ms) > (int32_t)PROBE_BYTES_MIN_INTERVAL_MS);

            if (needs_probe) {
                worker_perform_probe_bytes();
            }
        }
    }
}


static bool worker_perform_mount() {
    if (!sd_global_io_lock(300)) return false;

    SD_MMC.setPins(
        platform.pin_sd_clk, platform.pin_sd_cmd, platform.pin_sd_d0,
        platform.sd_bus == BusWidth::BW4 ? platform.pin_sd_d1 : -1,
        platform.sd_bus == BusWidth::BW4 ? platform.pin_sd_d2 : -1,
        platform.sd_bus == BusWidth::BW4 ? platform.pin_sd_d3 : -1
    );

    bool ok = SD_MMC.begin("/sdcard", platform.sd_bus == BusWidth::BW1, false);

    if (!ok && platform.sd_bus == BusWidth::BW4) {
        LOGW_T("sd_worker", "4-bit mode failed, falling back to 1-bit mode.");
        SD_MMC.end();
        SD_MMC.setPins(platform.pin_sd_clk, platform.pin_sd_cmd, platform.pin_sd_d0, -1, -1, -1);
        ok = SD_MMC.begin("/sdcard", true, false);
    }

    if (ok) {
        fs_policy_init(SD_MMC);

        s_status.generation.fetch_add(1, std::memory_order_acq_rel);
        s_status.mounted.store(true, std::memory_order_relaxed);
        s_status.generation.fetch_add(1, std::memory_order_release);
    }
    sd_global_io_unlock();

    if (ok) {
        schedule_log_state(true);
        schedule_fs_event(FS_SD_MOUNTED);
    }
    return ok;
}


static void worker_perform_probe_bytes() {
    if (!s_status.mounted.load(std::memory_order_acquire)) return;
    if (!sd_global_io_lock(150)) return;

    uint64_t total = SD_MMC.totalBytes();
    if (total == 0) {
        sd_global_io_unlock();
        worker_perform_unmount("probe_failed");
        return;
    }
    uint64_t used = SD_MMC.usedBytes();

    s_status.generation.fetch_add(1, std::memory_order_acq_rel);
    s_status.total_bytes = total;
    s_status.free_bytes = total > used ? total - used : 0;
    s_status.last_successful_probe_ms = millis();
    s_status.generation.fetch_add(1, std::memory_order_release);

    sd_global_io_unlock();
}


static void worker_perform_unmount(const char* reason) {
    if (!s_status.mounted.load(std::memory_order_acquire)) return;

    s_status.generation.fetch_add(1, std::memory_order_acq_rel);
    s_status.mounted.store(false, std::memory_order_relaxed);
    s_status.total_bytes = 0;
    s_status.free_bytes = 0;
    s_status.last_successful_probe_ms = 0;
    s_status.generation.fetch_add(1, std::memory_order_release);

    schedule_log_state(false);
    schedule_fs_event(FS_SD_UNMOUNT);

    uint32_t deadline = millis() + 150;
    bool locked = false;
    while ((int32_t)(millis() - deadline) < 0) {
        if (sd_global_io_lock(0)) { locked = true; break; }
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    if (locked) {
        SD_MMC.end();
        sd_global_io_unlock();
    } else {
        LOGW("SD: Unmount deferred (busy). Reason: %s", reason);
    }

    LOGW("SD: Unmounted. Reason: %s", reason);
    s_next_mount_attempt_ms = millis() + MOUNT_RETRY_BACKOFF_BASE_MS;
}

/* === END FILE: src\SD_Karten_Treiber.cpp === */