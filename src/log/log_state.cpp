/* === BEGIN FILE: src\log\log_state.cpp === */

#include "log_internal.hpp"

RTC_NOINIT_ATTR PanicHdr g_panic_hdr;
RTC_NOINIT_ATTR uint8_t  g_panic_tail_buffer[CFG_PANIC_TAIL_MAX];

namespace log_internal {

static State g_log_state;

State& state() {
    return g_log_state;
}

} // namespace log_internal

/* === END FILE: src\log\log_state.cpp === */