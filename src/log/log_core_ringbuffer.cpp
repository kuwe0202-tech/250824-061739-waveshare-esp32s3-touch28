/* === BEGIN FILE: src\log\log_core_ringbuffer.cpp === */

#include "log_internal.hpp"
#include "Konfiguration.h"
#include "RTC_PCF85063.h"
#include "runtime_tasks.h"
#include "Task_Ticker.h"

#include <Arduino.h>
#include <string.h>
#include <algorithm>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#if defined(ARDUINO_USB_CDC_ON_BOOT)
  #if __has_include(<tusb.h>)
    #include <tusb.h>
  #endif
#endif

namespace log_internal {

enum { EV_BP_ON=1u<<0, EV_BP_OFF=1u<<1, EV_UART_ON=1u<<2, EV_UART_OFF=1u<<3 };

// --- Interne Helfer ---

static inline bool _serial_host_ready() {
#if defined(ARDUINO_USB_CDC_ON_BOOT)
  #if defined(TINYUSB_H_) || __has_include(<tusb.h>)
    return tud_cdc_connected();
  #else
    return (bool)Serial;
  #endif
#else
  return (bool)Serial;
#endif
}

size_t get_ring_used_unsafe(){
  auto& s = state();
  return (s.ring_head>=s.ring_tail)? (s.ring_head-s.ring_tail)
                                      : (CFG_LOG_RING_BYTES-s.ring_tail+s.ring_head);
}

static size_t get_ring_free_unsafe(){ return CFG_LOG_RING_BYTES - get_ring_used_unsafe() - 1; }

static void internal_ring_write_unsafe(const void* d, size_t len){
  auto& s = state();
  size_t h=s.ring_head; size_t n1=std::min(len, CFG_LOG_RING_BYTES-h);
  memcpy(&s.log_ring_buffer[h], d, n1);
  size_t n2=len-n1; if(n2) memcpy(&s.log_ring_buffer[0], (const uint8_t*)d+n1, n2);
  s.ring_head=(h+len)%CFG_LOG_RING_BYTES;
}

void ring_read_at(size_t* idx, void* d, size_t len){
  auto& s = state();
  size_t t=*idx; size_t n1=std::min(len, CFG_LOG_RING_BYTES-t);
  memcpy(d, &s.log_ring_buffer[t], n1);
  size_t n2=len-n1; if(n2) memcpy((uint8_t*)d+n1, &s.log_ring_buffer[0], n2);
  *idx=(t+len)%CFG_LOG_RING_BYTES;
}

// --- Implementierung ---

void init(void) {
  auto& s = state();
  s.ring_head = s.ring_tail = 0;
  s.log_seq_counter.store(0, std::memory_order_relaxed);
  s.backpressure_state.store(State::BackpressureState::OFF, std::memory_order_relaxed);
  s.uart_fallback_active.store(false, std::memory_order_relaxed);
  s.evt_bits.store(0, std::memory_order_relaxed);

  uint32_t now = millis();
  s.last_flush_ms = 0;
  s.sd_evt_ms.store(now, std::memory_order_relaxed);
  s.sd_abort.store(false, std::memory_order_relaxed);
  s.log_day_is_active = false;
  s.recover_pending.store(false, std::memory_order_relaxed);
  s.close_file_pending.store(false, std::memory_order_relaxed);

  addTaskPhasedNamed("log_flush_fsm", 1000, 250, run_flush_fsm, true);

  // --- KORRIGIERT: Zugriff auf globale Variable g_panic_hdr ---
  memset(&g_panic_hdr, 0, sizeof(g_panic_hdr));
  if (const boot_header_t* bh = boot_header_get()) {
    static const char kHexDigits[] = "0123456789abcdef";
    for (int i = 0; i < 4; ++i) {
      uint8_t b = bh->fw_hash[i];
      g_panic_hdr.fw_hash[i*2 + 0] = kHexDigits[(b >> 4) & 0x0F];
      g_panic_hdr.fw_hash[i*2 + 1] = kHexDigits[(b     ) & 0x0F];
    }
    g_panic_hdr.fw_hash[8] = '\0';
  }
}

void set_rtc_provider(RtcProvider provider) {
  state().rtc_provider = provider;
}

void write(uint8_t level, const char* tag, const char* msg) {
  auto& s = state();
  s.last_enqueued_level = (log_level_t)level;

  const bool serial_ready = _serial_host_ready();
  const bool in_isr = xPortInIsrContext();

  if (in_isr) { portENTER_CRITICAL_ISR(&s.ring_spinlock); }
  else        { portENTER_CRITICAL(&s.ring_spinlock); }

  const size_t used = get_ring_used_unsafe();
  const uint32_t pct = (used * 100) / CFG_LOG_RING_BYTES;

  auto st = s.backpressure_state.load(std::memory_order_relaxed);
  auto ns = st;
  if (st == State::BackpressureState::OFF      && pct > 70) ns = State::BackpressureState::LEVEL_1;
  else if (st == State::BackpressureState::LEVEL_1 && pct > 90) ns = State::BackpressureState::LEVEL_2;
  else if (st == State::BackpressureState::LEVEL_2 && pct < 85) ns = State::BackpressureState::LEVEL_1;
  else if (st == State::BackpressureState::LEVEL_1 && pct < 65) ns = State::BackpressureState::OFF;

  if (ns != st) {
    s.backpressure_state.store(ns, std::memory_order_relaxed);
    s.evt_bits.fetch_or((ns > st) ? EV_BP_ON : EV_BP_OFF, std::memory_order_relaxed);
  }
  if (ns >= State::BackpressureState::LEVEL_1 && level >= LOG_LVL_INF) { if(in_isr) portEXIT_CRITICAL_ISR(&s.ring_spinlock); else portEXIT_CRITICAL(&s.ring_spinlock); return; }
  if (ns >= State::BackpressureState::LEVEL_2 && level >= LOG_LVL_DBG) { if(in_isr) portEXIT_CRITICAL_ISR(&s.ring_spinlock); else portEXIT_CRITICAL(&s.ring_spinlock); return; }

  bool uart_on = s.uart_fallback_active.load(std::memory_order_relaxed);
  if (!uart_on && pct >= 90 && serial_ready) {
    s.uart_fallback_active.store(true, std::memory_order_relaxed);
    s.evt_bits.fetch_or(EV_UART_ON, std::memory_order_relaxed);
  } else if (uart_on && pct < 85) {
    s.uart_fallback_active.store(false, std::memory_order_relaxed);
    s.evt_bits.fetch_or(EV_UART_OFF, std::memory_order_relaxed);
  }

  size_t tag_len = strlen(tag);
  if (tag_len > LOG_TAG_MAX_LEN) tag_len = LOG_TAG_MAX_LEN;
  size_t msg_len = strlen(msg);

  const size_t first_cap = LOG_CHUNK_PAYLOAD_SIZE;
  const size_t cont_cap  = LOG_CONT_CHUNK_PAYLOAD_SIZE;
  size_t n_cont = 0;
  if (msg_len > first_cap) {
    n_cont = (msg_len - first_cap + cont_cap - 1) / cont_cap;
  }
  size_t needed_space = sizeof(LogHdr) + tag_len + msg_len + n_cont * sizeof(LogContHdr);

  if (needed_space > get_ring_free_unsafe()) {
    if(in_isr) portEXIT_CRITICAL_ISR(&s.ring_spinlock); else portEXIT_CRITICAL(&s.ring_spinlock);
    return;
  }

  uint32_t seq = s.log_seq_counter.fetch_add(1);
  const char* msg_ptr = msg;
  size_t remaining_msg_len = msg_len;

  {
    LogHdr hdr;
    hdr.ts_us = ts_boot_us();
    hdr.seq = seq;
    hdr.level = level;
    hdr.tag_len = tag_len;
    hdr.msg_len = msg_len;
    hdr.flags = (msg_len > LOG_CHUNK_PAYLOAD_SIZE) ? LOG_HDR_FLAG_FRAGMENTED : 0;
    hdr.crc16 = crc16_update(0, tag, tag_len);
    hdr.crc16 = crc16_update(hdr.crc16, msg, msg_len);
    internal_ring_write_unsafe(&hdr, sizeof(hdr));
    internal_ring_write_unsafe(tag, tag_len);
    size_t chunk_len = std::min(first_cap, remaining_msg_len);
    internal_ring_write_unsafe(msg_ptr, chunk_len);
    msg_ptr += chunk_len;
    remaining_msg_len -= chunk_len;
  }
  while (remaining_msg_len > 0) {
    LogContHdr cont_hdr{LOG_HDR_FLAG_CONTINUES};
    internal_ring_write_unsafe(&cont_hdr, sizeof(cont_hdr));
    size_t chunk_len = std::min(cont_cap, remaining_msg_len);
    internal_ring_write_unsafe(msg_ptr, chunk_len);
    msg_ptr += chunk_len;
    remaining_msg_len -= chunk_len;
  }

  if(in_isr) portEXIT_CRITICAL_ISR(&s.ring_spinlock); else portEXIT_CRITICAL(&s.ring_spinlock);

  s.flush_request_pending.store(true, std::memory_order_relaxed);

  if (!in_isr && s.uart_fallback_active.load() && _serial_host_ready()) {
    uint32_t now = millis();
    if (now - s.last_uart_refill_ms >= 1000) {
      s.uart_tokens = std::min<uint32_t>(s.uart_tokens + CFG_LOG_UART_MAX_LPS, CFG_LOG_UART_BOOT_BURST);
      s.last_uart_refill_ms = now;
    }
    if (s.uart_tokens) {
      s.uart_tokens--;
      Serial.printf("[%llu] [%u] [%.*s] %.*s\n",
        (unsigned long long)ts_boot_us(), (unsigned)level,
        (int)tag_len, tag, (int)std::min<size_t>(96, msg_len), msg);
    }
  }
}

void flush(const char* reason) {
  auto& s = state();
  if(reason && strcmp(reason,"panic")==0){
    portENTER_CRITICAL(&s.ring_spinlock);
    size_t used=get_ring_used_unsafe();
    size_t to_copy=std::min((size_t)CFG_PANIC_TAIL_MAX, used);
    size_t head=s.ring_head;
    size_t start=(head+CFG_LOG_RING_BYTES-to_copy)%CFG_LOG_RING_BYTES;
    // --- KORRIGIERT: Zugriff auf globale Variable g_panic_tail_buffer ---
    size_t cur=start; ring_read_at(&cur, g_panic_tail_buffer, to_copy);

    // --- KORRIGIERT: Zugriff auf globale Variable g_panic_hdr ---
    g_panic_hdr.ver      = PANIC_HDR_VER;
    g_panic_hdr.flags    = PANIC_FL_RETRY_PENDING;
    g_panic_hdr.ts_us    = ts_boot_us_iram();
    g_panic_hdr.last_seq = s.log_seq_counter.load(std::memory_order_relaxed);
    g_panic_hdr.len      = to_copy;
    g_panic_hdr.crc      = crc32_update(0, g_panic_tail_buffer, to_copy);

    #if defined(__XTENSA__)
      asm volatile("memw" ::: "memory");
    #else
      __sync_synchronize();
    #endif
    g_panic_hdr.magic = PANIC_HDR_MAGIC;

    portEXIT_CRITICAL(&s.ring_spinlock);
    return;
  }
  s.flush_request_pending.store(true, std::memory_order_relaxed);
}

void get_last_log_info(last_log_info_t* info) {
    if (!info) return;
    auto& s = state();

    std::lock_guard<std::mutex> lock(s.flush_mutex);
    if (s.sd_mounted.load(std::memory_order_relaxed)) {
        info->valid = true;
        info->last_write_ms = s.last_flush_ms;
        info->last_level = s.last_flushed_level;
        if (s.log_day_is_active) {
            snprintf(info->filename, sizeof(info->filename), "%s.tmp", s.active_log_basename);
        } else {
            strcpy(info->filename, "Kein aktiver Log");
        }
    } else {
        info->valid = false;
        info->last_write_ms = 0;
        info->last_level = LOG_LVL_DBG;
        strcpy(info->filename, "SD nicht bereit");
    }
}

} // namespace log_internal

/* === END FILE: src\log\log_core_ringbuffer.cpp === */