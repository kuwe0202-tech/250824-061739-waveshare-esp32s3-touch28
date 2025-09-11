/* === BEGIN FILE: include/fs_policy.h (FINALE WP-B VERSION) === */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <mutex>
#include <vector>
#include <string>

#include <FS.h>

enum class FsPolicyError : uint8_t {
    OK = 0,
    ERR_PATH_NOT_8DOT3_FINAL,
    ERR_RENAME_CROSS_DIR,
    ERR_FATFS_API_ERROR,
    ERR_COMMIT_VERIFY_FAIL
};

struct FsPolicyTelemetry {
    uint32_t policy_violations_total = 0;
    FsPolicyError last_error = FsPolicyError::OK;
    char     last_error_path[260] = {0};

    uint32_t lfn_temp_in_use = 0;
    uint32_t lfn_temp_leaks_total = 0;

    uint32_t commit_rename_used = 0;
    uint32_t commit_copy_used   = 0;
};

class LfnTempScope {
public:
    LfnTempScope();
    ~LfnTempScope();

    void register_path(const char* p);
    void unregister_path(const char* p);

private:
    std::mutex mtx;
    std::vector<std::string> paths;
};

void fs_policy_init(FS& fs);

File fs_open_final(const char* path, const char* mode);
File fs_open_temp(LfnTempScope& scope, const char* path, const char* mode);

bool fs_policy_mkdir(const char* dir_path);
bool fs_policy_remove(const char* path);

bool fs_policy_rename(LfnTempScope& scope, const char* from_temp_path, const char* to_final_path);
// NEU: Deklaration für die Migrationsfunktion
bool fs_policy_rename_dir(const char* from_path, const char* to_path);

FsPolicyError fs_policy_commit_copy(LfnTempScope& scope, const char* src_temp_path, const char* dst_final_path);

void fs_fsync(File& file);

bool fs_policy_is_final_path(const char* path);
bool fs_same_dir(const char* path_a, const char* path_b);

void fs_policy_get_telemetry(FsPolicyTelemetry& stats);

/* === END FILE: include/fs_policy.h === */