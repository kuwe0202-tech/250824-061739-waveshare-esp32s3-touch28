#include "fs_policy.h"
#include "fs_paths.h"
#include "Log.h"

#include <set>
#include <string>
#include <vector>
#include <mutex>
#include <algorithm>
#include <cstring>
#include <cctype>
#include <cassert>

#include "ff.h"

#ifndef FF_MAX_LFN
#define FF_MAX_LFN 255
#warning "FF_MAX_LFN not defined, using fallback value 255. Please include ffconf.h in your build path."
#endif




static FS* s_fs = nullptr;
static FsPolicyTelemetry s_telemetry = {0};




LfnTempScope::LfnTempScope() {}
LfnTempScope::~LfnTempScope() {
    std::lock_guard<std::mutex> lock(mtx);
    const uint32_t leaks = (uint32_t)paths.size();
    if (leaks > 0) {
        s_telemetry.lfn_temp_leaks_total += leaks;
        if (s_telemetry.lfn_temp_in_use >= leaks) {
            s_telemetry.lfn_temp_in_use -= leaks;
        } else {
            s_telemetry.lfn_temp_in_use = 0;
        }
        paths.clear();
    }
}

void LfnTempScope::register_path(const char* p) {
    if (!p || !*p) return;
    std::lock_guard<std::mutex> lock(mtx);
    paths.emplace_back(p);
    s_telemetry.lfn_temp_in_use += 1;
}

void LfnTempScope::unregister_path(const char* p) {
    if (!p) return;
    std::lock_guard<std::mutex> lock(mtx);
    auto it = std::find(paths.begin(), paths.end(), std::string(p));
    if (it != paths.end()) {
        paths.erase(it);
        if (s_telemetry.lfn_temp_in_use > 0) s_telemetry.lfn_temp_in_use -= 1;
    }
}




static void record_policy_violation(FsPolicyError err, const char* path) {
    s_telemetry.policy_violations_total++;
    s_telemetry.last_error = err;
    if (!path) {
        s_telemetry.last_error_path[0] = '\0';
        return;
    }
    strncpy(s_telemetry.last_error_path, path, sizeof(s_telemetry.last_error_path) - 1);
    s_telemetry.last_error_path[sizeof(s_telemetry.last_error_path) - 1] = '\0';
}

void fs_fsync(File& file) {
    if (file) file.flush();
}

bool fs_same_dir(const char* path_a, const char* path_b) {
    if (!path_a || !path_b) return false;
    const char* last_slash_a = strrchr(path_a, '/');
    const char* last_slash_b = strrchr(path_b, '/');
    size_t dir_len_a = (last_slash_a > path_a) ? (size_t)(last_slash_a - path_a) : 0;
    size_t dir_len_b = (last_slash_b > path_b) ? (size_t)(last_slash_b - path_b) : 0;
    if (dir_len_a != dir_len_b) return false;
    if (dir_len_a == 0) return true;
    return memcmp(path_a, path_b, dir_len_a) == 0;
}

bool fs_policy_is_final_path(const char* path) {
    static const std::set<std::string> DOS_RESERVED_NAMES = {
        "CON","PRN","AUX","NUL","COM1","COM2","COM3","COM4","COM5","COM6","COM7","COM8","COM9",
        "LPT1","LPT2","LPT3","LPT4","LPT5","LPT6","LPT7","LPT8","LPT9"
    };
    if (!path || path[0] != '/' || strcmp(path, "/") == 0) return false;

    // *** START DER ÄNDERUNG ***
    // Füge eine Ausnahme für die spezielle .DIRBAR-Datei hinzu
    const char* basename_ptr = strrchr(path, '/');
    if (basename_ptr && strcmp(basename_ptr + 1, FsSpecial::DIRBAR_BASENAME) == 0) {
        return true;
    }
    // *** ENDE DER ÄNDERUNG ***

    for (const char* p = path; *p; ++p) {
        unsigned char uc = (unsigned char)*p;
        if (!(std::isalpha(uc) || std::isdigit(uc) || uc == '_' || uc == '/' || uc == '.')) return false;
    }

    char path_buf[FF_MAX_LFN + 1];
    strncpy(path_buf, path, sizeof(path_buf));
    path_buf[sizeof(path_buf) - 1] = '\0';

    char* saveptr = nullptr;
    char* root = strtok_r(path_buf + 1, "/", &saveptr);
    if (!root) return false;

    char root_path[32];
    snprintf(root_path, sizeof(root_path), "/%s", root);
    for (char* p = root_path; *p; ++p) *p = (char)std::tolower((unsigned char)*p);

    const bool root_ok =
        (strcmp(root_path, FsFinalPath::LOGS)    == 0) ||
        (strcmp(root_path, FsFinalPath::DUMP)    == 0) ||
        (strcmp(root_path, FsFinalPath::CONFIG)  == 0) ||
        (strcmp(root_path, FsFinalPath::UPDATE)  == 0) ||
        (strcmp(root_path, FsFinalPath::SERVICE) == 0) ||
        (strcmp(root_path, FsFinalPath::Legacy::LOGS)   == 0) ||
        (strcmp(root_path, FsFinalPath::Legacy::DUMP)   == 0) ||
        (strcmp(root_path, FsFinalPath::Legacy::CONFIG) == 0);

    if (!root_ok) return false;

    char* filename = strtok_r(nullptr, "/", &saveptr);
    if (!filename) return true;
    if (strtok_r(nullptr, "/", &saveptr)) return false;

    char* dot = strchr(filename, '.');
    if (!dot || dot == filename || strchr(dot + 1, '.') || strlen(dot + 1) != 3) return false;

    *dot = '\0';
    char* basename = filename;
    size_t base_len = strlen(basename);
    if (base_len < 1 || base_len > 8) return false;

    for (size_t i = 0; i < base_len; ++i) {
        unsigned char uc = (unsigned char)basename[i];
        if (!(std::isalnum(uc) || basename[i] == '_')) return false;
    }
    for (size_t i = 0; i < 3; ++i) {
        unsigned char uc = (unsigned char)*(dot + 1 + i);
        if (!(std::isalnum(uc) || uc == '_')) return false;
    }

    char basename_up[9] = {0};
    for (size_t i = 0; i < base_len && i < sizeof(basename_up) - 1; ++i)
        basename_up[i] = (char)std::toupper((unsigned char)basename[i]);
    if (DOS_RESERVED_NAMES.count(basename_up)) return false;

    return true;
}




void fs_policy_init(FS& fs) {
    s_fs = &fs;
}

File fs_open_final(const char* path, const char* mode) {
    if (!fs_policy_is_final_path(path)) {
        record_policy_violation(FsPolicyError::ERR_PATH_NOT_8DOT3_FINAL, path);
        LOGE("Policy VIOLATION: Attempt to open invalid final path '%s'", path ? path : "(null)");
        return File();
    }
    return s_fs ? s_fs->open(path, mode) : File();
}

File fs_open_temp(LfnTempScope& scope, const char* path, const char* mode) {
    scope.register_path(path);
    return s_fs ? s_fs->open(path, mode) : File();
}

bool fs_policy_mkdir(const char* dir_path) {
    return s_fs ? s_fs->mkdir(dir_path) : false;
}

bool fs_policy_remove(const char* path) {
    return s_fs ? s_fs->remove(path) : false;
}

static void touch_dirbar_for_target(const char* final_path) {
    if (!s_fs || !final_path) return;
    const char* last_slash = strrchr(final_path, '/');
    if (!last_slash || last_slash == final_path) return;

    size_t dir_len = (size_t)(last_slash - final_path);
    char dirbar_path[FF_MAX_LFN + 1] = {0};
    if (dir_len + 1 + strlen(FsSpecial::DIRBAR_BASENAME) >= sizeof(dirbar_path)) return;

    memcpy(dirbar_path, final_path, dir_len);
    dirbar_path[dir_len] = '\0';
    strcat(dirbar_path, "/");
    strcat(dirbar_path, FsSpecial::DIRBAR_BASENAME);

    File f = s_fs->open(dirbar_path, s_fs->exists(dirbar_path) ? "a" : "w");
    if (f) { (void)f.write((const uint8_t*)"", 0); fs_fsync(f); f.close(); }
}

FsPolicyError fs_policy_commit_copy(LfnTempScope& scope, const char* src_temp_path, const char* dst_final_path) {
    if (fs_policy_is_final_path(src_temp_path) || !fs_policy_is_final_path(dst_final_path)) {
        record_policy_violation(FsPolicyError::ERR_PATH_NOT_8DOT3_FINAL, src_temp_path);
        return FsPolicyError::ERR_PATH_NOT_8DOT3_FINAL;
    }
    if (!fs_same_dir(src_temp_path, dst_final_path)) {
        record_policy_violation(FsPolicyError::ERR_RENAME_CROSS_DIR, src_temp_path);
        return FsPolicyError::ERR_RENAME_CROSS_DIR;
    }
    if (!s_fs) return FsPolicyError::ERR_FATFS_API_ERROR;

    File src = s_fs->open(src_temp_path, "r");
    if (!src) return FsPolicyError::ERR_FATFS_API_ERROR;
    File dst = fs_open_final(dst_final_path, "w");
    if (!dst) { src.close(); return FsPolicyError::ERR_FATFS_API_ERROR; }

    size_t src_size = src.size();
    uint8_t buf[4096];
    for (;;) {
        size_t n = src.read(buf, sizeof(buf));
        if (n == 0) break;
        if (dst.write(buf, n) != n) {
            src.close(); dst.close(); s_fs->remove(dst_final_path);
            return FsPolicyError::ERR_FATFS_API_ERROR;
        }
    }
    fs_fsync(dst);
    dst.close();

    File dst_verify = s_fs->open(dst_final_path, "r");
    bool verified = (dst_verify && dst_verify.size() == src_size);
    if (dst_verify) dst_verify.close();
    if (!verified) {
        src.close(); s_fs->remove(dst_final_path);
        return FsPolicyError::ERR_COMMIT_VERIFY_FAIL;
    }

    scope.unregister_path(src_temp_path);
    src.close();
    s_fs->remove(src_temp_path);
    touch_dirbar_for_target(dst_final_path);
    s_telemetry.commit_copy_used++;
    return FsPolicyError::OK;
}

bool fs_policy_rename(LfnTempScope& scope, const char* from_temp_path, const char* to_final_path) {
    if (fs_policy_is_final_path(from_temp_path) || !fs_policy_is_final_path(to_final_path)) {
        record_policy_violation(FsPolicyError::ERR_PATH_NOT_8DOT3_FINAL, from_temp_path);
        return false;
    }
    if (!fs_same_dir(from_temp_path, to_final_path)) {
        record_policy_violation(FsPolicyError::ERR_RENAME_CROSS_DIR, from_temp_path);
        return false;
    }
    if (!s_fs) return false;

    if (s_fs->rename(from_temp_path, to_final_path)) {
        scope.unregister_path(from_temp_path);
        touch_dirbar_for_target(to_final_path);
        s_telemetry.commit_rename_used++;
        return true;
    }
    return false;
}

bool fs_policy_rename_dir(const char* from_path, const char* to_path) {
    if (!s_fs) return false;
    if (s_fs->exists(from_path)) {
        LOGI("Migrating '%s' -> '%s'", from_path, to_path);
        return s_fs->rename(from_path, to_path);
    }
    return true;
}

void fs_policy_get_telemetry(FsPolicyTelemetry& stats) {
    stats = s_telemetry;
}