/* === BEGIN FILE: src\log\log_recovery_export.cpp === */

#include "log_internal.hpp"
#include "Konfiguration.h"
#include "RTC_PCF85063.h"
#include "fs_policy.h"
#include "fs_paths.h"
#include "SD_Karten_Treiber.h"
#include "runtime_tasks.h"

#include <Arduino.h>
#include <string>
#include <vector>
#include <algorithm>
#include <time.h>

namespace log_internal {

// --- Interne Helfer ---

struct LogFileHeader {
    uint32_t magic; 
    uint8_t  version; 
    uint8_t  reserved[3];
    uint64_t anchor_boot_us; 
    int64_t  anchor_wall_us_offset; 
    uint32_t crc32;
};
static constexpr uint32_t LOG_FILE_HEADER_MAGIC = 0x474F4C48;

static constexpr uint32_t LOG_PAGE_MAGIC = 0x474F4C50;
static constexpr uint32_t LOG_PAGE_SIZE = 4096;

struct LogPageHdr {
    uint32_t magic;
    uint32_t page_seq;
    uint32_t payload_len;
    uint32_t payload_crc32;
} __attribute__((packed));

struct LogTail {
    uint32_t version;
    uint32_t tail_seq;
    uint32_t offset;
    uint32_t crc32;
} __attribute__((packed));

static bool read_and_verify_tail(const char* path, LogTail* out_tail) {
    if (!out_tail) return false;

    File f = fs_open_final(path, "r");
    if (!f || f.size() != sizeof(LogTail)) {
        if (f) f.close();
        return false;
    }

    if (f.read((uint8_t*)out_tail, sizeof(LogTail)) != sizeof(LogTail)) {
        f.close();
        return false;
    }
    f.close();

    uint32_t expected_crc = crc32_update(0, out_tail, sizeof(LogTail) - sizeof(uint32_t));
    return out_tail->crc32 == expected_crc;
}

static const char* level_to_str_export(log_level_t level) {
    switch (level) {
        case LOG_LVL_ERR: return "ERR";
        case LOG_LVL_WRN: return "WRN";
        case LOG_LVL_INF: return "INF";
        case LOG_LVL_DBG: return "DBG";
        default: return "???";
    }
}

static std::string json_escape(const char* str, size_t len) {
    std::string result;
    result.reserve(len + 10);
    result += '"';
    for (size_t i = 0; i < len; ++i) {
        char c = str[i];
        switch (c) {
            case '"':  result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\b': result += "\\b"; break;
            case '\f': result += "\\f"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:
                if (c >= 0 && c < 32) {
                    char buf[7];
                    sprintf(buf, "\\u%04x", (int)c);
                    result += buf;
                } else {
                    result += c;
                }
                break;
        }
    }
    result += '"';
    return result;
}

// --- Implementierung ---

void recover_on_boot() {
    auto& s = state();
    if (!s.sd_mounted.load() || s.log_day_is_active) return;

    LOGI_T("log", "Suche nach unvollendetem Log-Tag...");
    if (!sd_global_io_lock(3000)) { return; }

    // Die Adoptions-Logik ist jetzt in der setup()-Funktion,
    // diese Funktion dient nur noch dem Recovery eines abgestürzten Logs.

    File log_dir = fs_open_final("/log", "r");
    if (!log_dir) { sd_global_io_unlock(); return; }

    char tmp_path[96] = {0};
    uint32_t newest_date = 0;

    File entry = log_dir.openNextFile();
    while(entry) {
        const char* entry_name = entry.name();
        if (entry_name && strstr(entry_name, ".tmp")) {
            int year, month, day;
            if (sscanf(entry_name, "L%2d%2d%2d.tmp", &year, &month, &day) == 3) {
                uint32_t date_val = year * 10000 + month * 100 + day;
                if (date_val > newest_date) {
                    newest_date = date_val;
                    snprintf(tmp_path, sizeof(tmp_path), "/log/%s", entry_name);
                }
            }
        }
        entry.close();
        entry = log_dir.openNextFile();
    }
    log_dir.close();

    if (tmp_path[0] == '\0') { sd_global_io_unlock(); return; }

    LOGW_T("log", "Unvollendete Log-Datei gefunden: %s. Starte Wiederherstellung.", tmp_path);
    strncpy(s.active_log_basename, tmp_path, sizeof(s.active_log_basename) - 1);
    char* dot_tmp = strstr(s.active_log_basename, ".tmp");
    if (dot_tmp) *dot_tmp = '\0';

    LogTail tail_a, tail_b;
    bool a_valid = read_and_verify_tail(FsFinalPath::LOG_IDXS[0], &tail_a);
    bool b_valid = read_and_verify_tail(FsFinalPath::LOG_IDXS[1], &tail_b);

    LogTail* best_tail = nullptr;
    if (a_valid && b_valid) best_tail = (tail_a.tail_seq > tail_b.tail_seq) ? &tail_a : &tail_b;
    else if (a_valid) best_tail = &tail_a;
    else if (b_valid) best_tail = &tail_b;

    s.active_log_tmp_file = fs_open_final(tmp_path, "r+");

    if (best_tail) {
        uint32_t recovered_offset = best_tail->offset;
        uint32_t last_seq = (recovered_offset > 0) ? (recovered_offset / LOG_PAGE_SIZE) : 0;

        if (recovered_offset % LOG_PAGE_SIZE != 0) {
            recovered_offset -= (recovered_offset % LOG_PAGE_SIZE);
        }

        if(s.active_log_tmp_file) {
            while (recovered_offset > sizeof(LogFileHeader)) {
                uint32_t page_offset = recovered_offset - LOG_PAGE_SIZE;

                static uint8_t page_buf[LOG_PAGE_SIZE];
                s.active_log_tmp_file.seek(page_offset);
                if (s.active_log_tmp_file.read(page_buf, LOG_PAGE_SIZE) != LOG_PAGE_SIZE) {
                    recovered_offset = page_offset;
                    continue;
                }

                LogPageHdr* page_hdr = (LogPageHdr*)page_buf;
                if (page_hdr->payload_len == 0 || page_hdr->payload_len > LOG_PAGE_SIZE - sizeof(LogPageHdr)) {
                    recovered_offset = page_offset;
                    continue;
                }

                uint32_t expected_crc = crc32_update(0, page_buf + sizeof(LogPageHdr), page_hdr->payload_len);

                if (page_hdr->magic == LOG_PAGE_MAGIC && page_hdr->page_seq == last_seq && page_hdr->payload_crc32 == expected_crc) {
                    break;
                } else {
                    LOGW_T("log", "Letzte Page bei Offset %u ist korrupt (Magic/Seq/CRC). Setze Offset zurück.", (unsigned)recovered_offset);
                    recovered_offset = page_offset;
                    if (last_seq == 0) break;
                    last_seq--;
                    telemetry_event("recovery_page_break", NULL);
                }
            }

            s.current_log_offset = recovered_offset;
            s.current_tail_seq = best_tail->tail_seq;
            s.current_page_seq = (s.current_log_offset > sizeof(LogFileHeader)) ? (s.current_log_offset - sizeof(LogFileHeader)) / LOG_PAGE_SIZE : 0;
            s.log_day_is_active = true;
            s.log_header_anchored = (s.current_log_offset > sizeof(LogFileHeader));
            LOGI_T("log", "Wiederherstellung erfolgreich! Fahre bei Offset %u fort.", (unsigned)s.current_log_offset);
        }
    } else {
        File f_check = fs_open_final(tmp_path, "r");
        if (f_check) {
            f_check.close();
            if (s.active_log_tmp_file) {
                uint32_t scan_offset = sizeof(LogFileHeader);
                uint32_t last_good_offset = sizeof(LogFileHeader);
                uint32_t last_good_seq = 0;
                LogPageHdr page_hdr;
                static uint8_t page_buf[LOG_PAGE_SIZE];

                while(s.active_log_tmp_file.seek(scan_offset) && s.active_log_tmp_file.read(page_buf, LOG_PAGE_SIZE) == LOG_PAGE_SIZE) {
                    page_hdr = *(LogPageHdr*)page_buf;
                    if (page_hdr.magic != LOG_PAGE_MAGIC) break;
                    if (page_hdr.page_seq != last_good_seq + 1 && last_good_offset > sizeof(LogFileHeader)) {
                        break;
                    }
                    if (page_hdr.payload_len == 0 || page_hdr.payload_len > LOG_PAGE_SIZE - sizeof(LogPageHdr)) break;

                    uint32_t expected_crc = crc32_update(0, page_buf + sizeof(LogPageHdr), page_hdr.payload_len);
                    if (page_hdr.payload_crc32 == expected_crc) {
                        last_good_offset = scan_offset + LOG_PAGE_SIZE;
                        last_good_seq = page_hdr.page_seq;
                        scan_offset += LOG_PAGE_SIZE;
                    } else {
                        break;
                    }
                }
                s.current_log_offset = last_good_offset;
                s.current_page_seq = last_good_seq;
                s.current_tail_seq = 0;
                s.log_day_is_active = true;
                s.log_header_anchored = (s.current_log_offset > sizeof(LogFileHeader));
                if (!log_commit_tail()) {
                    telemetry_event("tail_commit_fail", "recover");
                }
                LOGI_T("log", "Scan beendet. Letzter guter Offset bei %u", (unsigned)s.current_log_offset);
            }
        } else {
            fs_policy_remove(tmp_path);
        }
    }

    if (s.active_log_tmp_file && !s.log_day_is_active) {
        s.active_log_tmp_file.close();
    }

    sd_global_io_unlock();
}

bool readback_tail(char* dst, size_t dst_len, size_t* out_len) {
    auto& s = state();
    if(out_len) *out_len = 0;
    if(!dst || dst_len == 0 || !s.sd_mounted.load() || !s.log_day_is_active) return false;

    if (!sd_global_io_lock(200)) return false;

    char tmp_path[96];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", s.active_log_basename);

    File f = fs_open_final(tmp_path, "r");
    if(!f) {
        sd_global_io_unlock();
        return false;
    }

    size_t off = (s.current_log_offset > LOG_PAGE_SIZE) ? (s.current_log_offset - LOG_PAGE_SIZE) : 0;
    if (off < sizeof(LogFileHeader)) off = sizeof(LogFileHeader);

    static uint8_t page_buffer[LOG_PAGE_SIZE];
    f.seek(off);
    size_t n_read = f.read(page_buffer, LOG_PAGE_SIZE);
    f.close();
    sd_global_io_unlock();

    if (n_read < sizeof(LogPageHdr)) return false;

    LogPageHdr* page_hdr = (LogPageHdr*)page_buffer;
    if (page_hdr->magic != LOG_PAGE_MAGIC || page_hdr->payload_len == 0 || page_hdr->payload_len > n_read - sizeof(LogPageHdr)) {
        return false;
    }
    uint32_t expected_crc = crc32_update(0, page_buffer + sizeof(LogPageHdr), page_hdr->payload_len);
    if (page_hdr->payload_crc32 != expected_crc) {
        return false;
    }

    uint8_t* payload_ptr = page_buffer + sizeof(LogPageHdr);
    uint32_t payload_remaining = page_hdr->payload_len;
    size_t dst_offset = 0;
    int msg_count = 0;

    char time_buf[32];
    char line_buf[512]; 
    static char reassembly_buf[CFG_LOG_RING_BYTES];

    while (payload_remaining >= sizeof(LogHdr)) {
        LogHdr* entry_hdr = (LogHdr*)payload_ptr;

        if (entry_hdr->tag_len > LOG_TAG_MAX_LEN || entry_hdr->msg_len > (sizeof(reassembly_buf)) ||
            (sizeof(LogHdr) + entry_hdr->tag_len > payload_remaining)) {
            break;
        }

        const char* tag = (const char*)(payload_ptr + sizeof(LogHdr));
        const char* msg_start = tag + entry_hdr->tag_len;
        size_t current_entry_offset = sizeof(LogHdr) + entry_hdr->tag_len;
        size_t msg_copied = 0;
        
        if(entry_hdr->flags & LOG_HDR_FLAG_FRAGMENTED) {
            size_t first_chunk_len = std::min((size_t)LOG_CHUNK_PAYLOAD_SIZE, (size_t)entry_hdr->msg_len);
            if(current_entry_offset + first_chunk_len > payload_remaining) break;
            memcpy(reassembly_buf, msg_start, first_chunk_len);
            msg_copied += first_chunk_len;
            current_entry_offset += first_chunk_len;

            while(msg_copied < entry_hdr->msg_len) {
                if (current_entry_offset + sizeof(LogContHdr) > payload_remaining) break;
                current_entry_offset += sizeof(LogContHdr);
                size_t next_chunk_len = std::min((size_t)LOG_CONT_CHUNK_PAYLOAD_SIZE, entry_hdr->msg_len - msg_copied);
                if (current_entry_offset + next_chunk_len > payload_remaining) break;
                memcpy(reassembly_buf + msg_copied, payload_ptr + current_entry_offset, next_chunk_len);
                msg_copied += next_chunk_len;
                current_entry_offset += next_chunk_len;
            }
        } else {
             if (current_entry_offset + entry_hdr->msg_len > payload_remaining) break;
             memcpy(reassembly_buf, msg_start, entry_hdr->msg_len);
             msg_copied = entry_hdr->msg_len;
             current_entry_offset += entry_hdr->msg_len;
        }

        if (msg_copied == entry_hdr->msg_len) {
            uint16_t actual_crc = crc16_update(0, tag, entry_hdr->tag_len);
            actual_crc = crc16_update(actual_crc, reassembly_buf, entry_hdr->msg_len);

            if (actual_crc == entry_hdr->crc16) {
                uint64_t export_us = (uint64_t)entry_hdr->ts_us + (uint64_t)s.wall_offset_us;
                time_t wall_ts_sec = (time_t)(export_us / 1000000ULL);
                struct tm timeinfo;
                gmtime_r(&wall_ts_sec, &timeinfo);
                size_t len = strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
                snprintf(time_buf + len, sizeof(time_buf) - len, ".%06llu", (unsigned long long)(export_us % 1000000ULL));

                int n_wanted = snprintf(line_buf, sizeof(line_buf), "%s|%u|%.*s|%.*s\n",
                    time_buf, (unsigned)entry_hdr->level, (int)entry_hdr->tag_len, tag, (int)entry_hdr->msg_len, reassembly_buf);

                size_t m_written = strnlen(line_buf, sizeof(line_buf));
                if (m_written > 0) {
                    size_t avail = (dst_len > 1 && dst_offset < dst_len -1) ? (dst_len - 1 - dst_offset) : 0;
                    size_t to_copy = std::min(m_written, avail);
                    if (to_copy > 0) {
                        memcpy(dst + dst_offset, line_buf, to_copy);
                        dst_offset += to_copy;
                        msg_count++;
                    }
                }
            }
        }
        
        payload_ptr += current_entry_offset;
        payload_remaining = (payload_ptr < page_buffer + n_read) ? (page_buffer + n_read - payload_ptr) : 0;
    }

    dst[dst_offset] = '\0';
    if(out_len) *out_len = dst_offset;

    char detail[48];
    snprintf(detail, sizeof(detail), "{msgs:%d, bytes:%u}", msg_count, (unsigned)dst_offset);
    telemetry_event("log_readback_parsed", detail);

    return dst_offset > 0;
}

bool run_export_for_file(const char* bin_log_path, bool export_txt, bool export_jsonl) {
    if (!bin_log_path || (!export_txt && !export_jsonl)) return false;

    struct {
        uint32_t parsed{0}, txt{0}, jsonl{0}, crc_fail{0};
    } counters;

    int y, m, d;
    if (sscanf(bin_log_path, "/log/L%2d%2d%2d.tmp", &y, &m, &d) != 3) {
        LOGE_T("log_export", "Konnte Datum aus '%s' nicht parsen.", bin_log_path);
        return false;
    }

    char txt_final_path[32], jsn_final_path[32];
    snprintf(txt_final_path, sizeof(txt_final_path), "/log/E%02d%02d%02d.TXT", y, m, d);
    snprintf(jsn_final_path, sizeof(jsn_final_path), "/log/J%02d%02d%02d.JSN", y, m, d);

    char txt_part_path[40], jsn_part_path[40];
    snprintf(txt_part_path, sizeof(txt_part_path), "/log/E%02d%02d%02d.TXT.part", y, m, d);
    snprintf(jsn_part_path, sizeof(jsn_part_path), "/log/J%02d%02d%02d.JSN.part", y, m, d);

    LfnTempScope scope;
    File bin_file = fs_open_final(bin_log_path, "r");
    if (!bin_file) {
        LOGE_T("log_export", "Konnte Quelldatei '%s' nicht öffnen.", bin_log_path);
        return false;
    }

    File txt_file, jsn_file;
    if (export_txt)   txt_file = fs_open_temp(scope, txt_part_path, "w");
    if (export_jsonl) jsn_file = fs_open_temp(scope, jsn_part_path, "w");
    if ((export_txt && !txt_file) || (export_jsonl && !jsn_file)) {
        LOGE_T("log_export", "Konnte temporäre Export-Datei nicht erstellen.");
        if(bin_file) bin_file.close();
        if(txt_file) { txt_file.close(); fs_policy_remove(txt_part_path); }
        if(jsn_file) { jsn_file.close(); fs_policy_remove(jsn_part_path); }
        return false;
    }

    LogFileHeader header = {0};
    bin_file.read((uint8_t*)&header, sizeof(header));
    uint32_t header_crc_check = crc32_update(0, &header, sizeof(header) - sizeof(uint32_t));
    if (header.magic != LOG_FILE_HEADER_MAGIC || header.version != 2 || header.crc32 != header_crc_check) {
        LOGW_T("log_export", "Log-Header ungültig oder alte Version. Nutze Fallback-Zeitstempel.");
        rtc_get_wall_boot_offset_us(&header.anchor_wall_us_offset);
        bin_file.seek(0);
    }
    
    int64_t  current_offset_us = header.anchor_wall_us_offset;
    uint64_t last_ts_boot_us   = 0;
    uint64_t last_export_us    = 0;
    bool is_first_entry      = true;

    static uint8_t page_buffer[LOG_PAGE_SIZE];
    static char reassembly_buffer[CFG_LOG_RING_BYTES];
    
    while (bin_file.available()) {
        tel_watchdog_mark_alive();
        vTaskDelay(1);

        size_t bytes_read = bin_file.read(page_buffer, LOG_PAGE_SIZE);
        if (bytes_read < sizeof(LogPageHdr)) continue;
        
        LogPageHdr* page_hdr = (LogPageHdr*)page_buffer;
        if (page_hdr->magic != LOG_PAGE_MAGIC || page_hdr->payload_len > (bytes_read - sizeof(LogPageHdr))) continue;
        if (crc32_update(0, page_buffer + sizeof(LogPageHdr), page_hdr->payload_len) != page_hdr->payload_crc32) continue;

        uint8_t* payload_ptr = page_buffer + sizeof(LogPageHdr);
        uint32_t payload_remaining = page_hdr->payload_len;

        while(payload_remaining >= sizeof(LogHdr)) {
            LogHdr* entry_hdr = (LogHdr*)payload_ptr;
            const char* tag = (const char*)(payload_ptr + sizeof(LogHdr));
            
            size_t msg_len = entry_hdr->msg_len;
            size_t tag_len = entry_hdr->tag_len;
            if (msg_len > sizeof(reassembly_buffer) || tag_len > LOG_TAG_MAX_LEN) {
                payload_remaining = 0; break;
            }

            size_t current_entry_offset = sizeof(LogHdr) + tag_len;
            if (current_entry_offset > payload_remaining) break;
            
            const char* msg_start_ptr = (const char*)(payload_ptr + current_entry_offset);
            size_t msg_copied = 0;

            if (entry_hdr->flags & LOG_HDR_FLAG_FRAGMENTED) {
                 size_t first_chunk_len = std::min((size_t)LOG_CHUNK_PAYLOAD_SIZE, msg_len);
                 if(current_entry_offset + first_chunk_len > payload_remaining) break;
                 memcpy(reassembly_buffer, msg_start_ptr, first_chunk_len);
                 msg_copied += first_chunk_len;
                 current_entry_offset += first_chunk_len;

                 while(msg_copied < msg_len) {
                     if (current_entry_offset + sizeof(LogContHdr) > payload_remaining) break;
                     current_entry_offset += sizeof(LogContHdr);
                     size_t next_chunk_len = std::min((size_t)LOG_CONT_CHUNK_PAYLOAD_SIZE, msg_len - msg_copied);
                     if (current_entry_offset + next_chunk_len > payload_remaining) break;
                     memcpy(reassembly_buffer + msg_copied, payload_ptr + current_entry_offset, next_chunk_len);
                     msg_copied += next_chunk_len;
                     current_entry_offset += next_chunk_len;
                 }
            } else {
                 if (current_entry_offset + msg_len > payload_remaining) break;
                 memcpy(reassembly_buffer, msg_start_ptr, msg_len);
                 msg_copied = msg_len;
                 current_entry_offset += msg_len;
            }

            if (msg_copied != msg_len) {
                payload_remaining = 0;
                continue;
            }
            
            uint16_t actual_crc = crc16_update(0, tag, tag_len);
            actual_crc = crc16_update(actual_crc, reassembly_buffer, msg_len);

            if (actual_crc == entry_hdr->crc16) {
                counters.parsed++;
                
                if (strncmp(tag, "BOOT", tag_len) == 0 && strncmp(reassembly_buffer, "EVENT_BOOT", 10) == 0) {
                     if (!is_first_entry) {
                         const uint64_t gap_us = 1;
                         uint64_t target_export_us = last_export_us + gap_us;
                         uint64_t would_be_export_us = entry_hdr->ts_us + current_offset_us;
                         int64_t correction = (int64_t)target_export_us - (int64_t)would_be_export_us;
                         current_offset_us += correction;
                     }
                } else if (strncmp(tag, "RTC", tag_len) == 0 && strncmp(reassembly_buffer, "CLOCK_JUMP", 10) == 0) {
                    long long delta_us = 0;
                    sscanf(reassembly_buffer, "CLOCK_JUMP{delta_us=%lld}", &delta_us);
                    current_offset_us += delta_us;
                }

                uint64_t export_us = entry_hdr->ts_us + current_offset_us;
                if (export_us < last_export_us && !is_first_entry) {
                    export_us = last_export_us + 1;
                }

                char time_buf[40];
                time_t wall_ts_sec = (time_t)(export_us / 1000000ULL);
                struct tm timeinfo;
                gmtime_r(&wall_ts_sec, &timeinfo);
                size_t len = strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
                snprintf(time_buf + len, sizeof(time_buf) - len, ".%06llu", (unsigned long long)(export_us % 1000000ULL));

                if (export_txt && txt_file) {
                    txt_file.printf("%s | %llu | [%s] [%.*s] %.*s\n", time_buf, (unsigned long long)entry_hdr->ts_us,
                        level_to_str_export((log_level_t)entry_hdr->level), (int)tag_len, tag,
                        (int)msg_len, reassembly_buffer);
                    counters.txt++;
                }
                if (export_jsonl && jsn_file) {
                    jsn_file.printf("{\"time_iso\":\"%sZ\",\"time_us\":%llu,\"level\":\"%s\",\"tag\":\"%.*s\",\"msg\":%s}\n",
                        time_buf, (unsigned long long)entry_hdr->ts_us,
                        level_to_str_export((log_level_t)entry_hdr->level), (int)tag_len, tag,
                        json_escape(reassembly_buffer, msg_len).c_str());
                    counters.jsonl++;
                }
                
                last_ts_boot_us = entry_hdr->ts_us;
                last_export_us = export_us;
                is_first_entry = false;
            } else {
                counters.crc_fail++;
            }
            
            payload_ptr += current_entry_offset;
            if(current_entry_offset > payload_remaining) payload_remaining = 0; else payload_remaining -= current_entry_offset;
        }
    }

    bin_file.close();
    if (txt_file) { fs_fsync(txt_file); txt_file.close(); }
    if (jsn_file) { fs_fsync(jsn_file); jsn_file.close(); }

    bool final_success = false;
    if (counters.parsed > 0 && counters.crc_fail == 0) {
        bool rename_ok = true;
        if (export_txt) {
            if (!fs_policy_rename(scope, txt_part_path, txt_final_path)) {
                rename_ok = false;
                fs_policy_remove(txt_part_path);
            }
        }
        if (export_jsonl) {
            if (!fs_policy_rename(scope, jsn_part_path, jsn_final_path)) {
                rename_ok = false;
                fs_policy_remove(jsn_part_path);
            }
        }
        final_success = rename_ok;
    } else {
        LOGW_T("log_export", "Export für '%s' fehlgeschlagen. Parsed:%u, CRC-Fails:%u. Räume .part-Dateien auf.",
            bin_log_path, counters.parsed, counters.crc_fail);
        if (export_txt) fs_policy_remove(txt_part_path);
        if (export_jsonl) fs_policy_remove(jsn_part_path);
    }
    
    return final_success;
}

// In Datei: src/log/log_recovery_export.cpp

void boot_adopt_preallocated_file(void) {
    if (!rtc_is_valid()) return;

    auto& s = state();
    datetime_t now; RTC_PCF85063_Read_Time(&now);
    char base[32]; snprintf(base, sizeof(base), "/log/L%02d%02d%02d", now.year % 100, now.month, now.day);
    char tmp_path[48]; snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", base);

    if (!sd_global_io_lock(2000)) {
        LOGE_T("BOOT", "Konnte Lock für Log-Adoption nicht erhalten.");
        return;
    }

    // KORREKTUR: Öffne die Datei direkt in der globalen Zustandsvariable.
    // Dadurch wird sie nicht am Ende der Funktion durch einen Destruktor geschlossen.
    s.active_log_tmp_file = fs_open_final(tmp_path, "r+");

    if (!s.active_log_tmp_file || s.active_log_tmp_file.isDirectory() || s.active_log_tmp_file.size() < CFG_LOG_PREALLOC_BYTES || (s.active_log_tmp_file.size() % 4096 != 0)) {
        if (s.active_log_tmp_file) {
            s.active_log_tmp_file.close(); // Manuell schließen, da es jetzt global ist
        }
        sd_global_io_unlock();
        return;
    }

    int64_t anchor_offset_us = 0;
    rtc_get_wall_boot_offset_us(&anchor_offset_us);

    LogFileHeader hdr{};
    hdr.magic = 0x474F4C48;
    hdr.version = 2;
    hdr.anchor_boot_us = 0; // Wird bei der ersten Log-Nachricht gesetzt
    hdr.anchor_wall_us_offset = anchor_offset_us;
    hdr.crc32 = 0;
    hdr.crc32 = crc32_update(0, &hdr, sizeof(hdr) - sizeof(uint32_t));

    s.active_log_tmp_file.seek(0);
    s.active_log_tmp_file.write((uint8_t*)&hdr, sizeof(hdr));
    fs_fsync(s.active_log_tmp_file);

    // Der Rest der Initialisierung
    s.current_log_offset = sizeof(LogFileHeader);
    s.active_log_tmp_file.seek(s.current_log_offset);
    s.current_page_seq = 0;
    s.current_tail_seq = 0;
    s.log_day_is_active = true;
    s.log_header_anchored = true;
    s.header_needs_reanchor = false;
    s.wall_offset_us = anchor_offset_us;
    
    // Wichtig: Auch die alten Indexdateien löschen
    fs_policy_remove(FsFinalPath::LOG_IDXS[0]);
    fs_policy_remove(FsFinalPath::LOG_IDXS[1]);
    log_commit_tail();

    sd_global_io_unlock();
    LOGI_T("BOOT", "Log-Datei vom Bootloader erfolgreich übernommen.");
}

} // namespace log_internal

/* === END FILE: src\log\log_recovery_export.cpp === */