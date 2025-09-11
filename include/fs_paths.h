/* === BEGIN FILE: include\fs_paths.h === */

#pragma once
#include <iterator>

namespace FsFinalPath {
    
    // GEÄNDERT: Pfade auf kanonische Kleinbuchstaben umgestellt, wie im Plan spezifiziert.
    constexpr const char* LOGS    = "/log";
    constexpr const char* DUMP    = "/dump";
    constexpr const char* CONFIG  = "/sys"; // CONFIG wird zu /sys
    constexpr const char* UPDATE  = "/upd"; // UPDATE wird zu /upd
    constexpr const char* SERVICE = "/serv";

    // NEU: Alte Pfade für die einmalige Migration
    namespace Legacy {
        constexpr const char* LOGS   = "/LOGS";
        constexpr const char* DUMP   = "/DUMP";
        constexpr const char* CONFIG = "/CFG";
    }

    // GEÄNDERT: Die alten kanonischen Pfade werden entfernt, da die Hauptpfade jetzt kanonisch sind.
    // constexpr const char* LOG_CANON = "/log";
    // constexpr const char* DUMP_CANON = "/dump";
    // constexpr const char* SYS_CANON  = "/sys";

    
    // GEÄNDERT: Pfade für Konfigurationsdateien unter /sys
    constexpr const char* CONFIG_FILE = "/sys/CFG.BIN";
    constexpr const char* ANCHOR_FILE = "/sys/ANCH.BIN";

    
    // GEÄNDERT: Log-Dateien sind jetzt unter /log
    constexpr const char* LOG_FILE_0 = "/log/SYS.000";
    constexpr const char* LOG_FILE_1 = "/log/SYS.001";
    constexpr const char* LOG_FILE_2 = "/log/SYS.002";
    constexpr const char* LOG_FILE_3 = "/log/SYS.003";
    constexpr const char* LOG_FILE_4 = "/log/SYS.004";

    constexpr const char* LOG_IDX_0 = "/log/SYS0.IDX";
    constexpr const char* LOG_IDX_1 = "/log/SYS1.IDX";

    inline constexpr const char* LOG_FILES[] = {
        LOG_FILE_0, LOG_FILE_1, LOG_FILE_2, LOG_FILE_3, LOG_FILE_4
    };
    inline constexpr const char* LOG_IDXS[]  = {
        LOG_IDX_0,  LOG_IDX_1
    };

    static_assert(std::size(LOG_FILES) > 0, "FsFinalPath::LOG_FILES must not be empty");
    static_assert(std::size(LOG_IDXS) == 2, "FsFinalPath::LOG_IDXS must have exactly 2 entries");
}


namespace FsSpecial {
    constexpr const char* DIRBAR_BASENAME = ".DIRBAR"; 
}

/* === END FILE: include\fs_paths.h === */