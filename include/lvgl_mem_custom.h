#ifndef LVGL_MEM_CUSTOM_H
#define LVGL_MEM_CUSTOM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* kleine Hash-Tabelle zum Tracken */
#ifndef LVGL_ALLOC_TRACK_SLOTS
#  define LVGL_ALLOC_TRACK_SLOTS 8192
#endif

typedef struct { void* p; size_t sz; } lvgl_trk_ent_t;
static lvgl_trk_ent_t _trk[LVGL_ALLOC_TRACK_SLOTS];
static size_t _trk_used_bytes = 0;
static size_t _trk_peak_bytes = 0;
static size_t _trk_blocks     = 0;
static size_t _trk_max_block  = 0;

static inline uint32_t _hptr(const void* p){
  uintptr_t x = (uintptr_t)p; x >>= 3;
  return (uint32_t)(x & (LVGL_ALLOC_TRACK_SLOTS-1));
}
static inline int _trk_find_slot(const void* p){
  uint32_t i = _hptr(p);
  for (uint32_t k=0;k<LVGL_ALLOC_TRACK_SLOTS;k++){
    uint32_t j = (i+k) & (LVGL_ALLOC_TRACK_SLOTS-1);
    if (_trk[j].p == p)   return (int)j;
    if (_trk[j].p == NULL) return -1;
  }
  return -1;
}
static inline int _trk_find_free(void){
  for (uint32_t j=0;j<LVGL_ALLOC_TRACK_SLOTS;j++)
    if (_trk[j].p == NULL) return (int)j;
  return -1;
}
static inline void _trk_recompute_max(void){
  size_t m=0;
  for (uint32_t j=0;j<LVGL_ALLOC_TRACK_SLOTS;j++)
    if (_trk[j].p && _trk[j].sz>m) m=_trk[j].sz;
  _trk_max_block=m;
}

/* Backend wählen */
#if __has_include(<esp_heap_caps.h>)
  #include <esp_heap_caps.h>
  #define LVGL_SYS_ALLOC(n)      heap_caps_malloc((n), MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM)
  #define LVGL_SYS_REALLOC(p,n)  heap_caps_realloc((p),(n), MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM)
  #define LVGL_SYS_FREE(p)       heap_caps_free((p))
#else
  #include <stdlib.h>
  #define LVGL_SYS_ALLOC(n)      malloc((n))
  #define LVGL_SYS_REALLOC(p,n)  realloc((p),(n))
  #define LVGL_SYS_FREE(p)       free((p))
#endif

/* --- von lv_conf.h referenzierte Funktionen --- */
void* lvgl_mem_alloc(size_t n){
  if (!n) return NULL;
  void* p = LVGL_SYS_ALLOC(n);
  if (!p) return NULL;
  int slot = _trk_find_free();
  if (slot>=0){
    _trk[slot].p = p;
    _trk[slot].sz= n;
    _trk_blocks++;
    _trk_used_bytes += n;
    if (_trk_used_bytes > _trk_peak_bytes) _trk_peak_bytes = _trk_used_bytes;
    if (n > _trk_max_block) _trk_max_block = n;
  }
  return p;
}

void  lvgl_mem_free(void* p){
  if (!p) return;
  int slot = _trk_find_slot(p);
  if (slot>=0){
    size_t n = _trk[slot].sz;
    _trk[slot].p = NULL; _trk[slot].sz = 0;
    if (_trk_used_bytes >= n) _trk_used_bytes -= n; else _trk_used_bytes=0;
    if (_trk_blocks>0) _trk_blocks--;
    if (n == _trk_max_block) _trk_recompute_max();
  }
  LVGL_SYS_FREE(p);
}

void* lvgl_mem_realloc(void* p, size_t n){
  if (!p) return lvgl_mem_alloc(n);
  if (n==0){ lvgl_mem_free(p); return NULL; }

  size_t old=0;
  int slot = _trk_find_slot(p);
  if (slot>=0) old = _trk[slot].sz;

  void* np = LVGL_SYS_REALLOC(p,n);
  if (!np) return NULL;

  if (slot>=0){
    _trk[slot].p = np;
    _trk[slot].sz= n;
  }else{
    int s2=_trk_find_free();
    if (s2>=0){ _trk[s2].p=np; _trk[s2].sz=n; _trk_blocks++; }
  }
  if (_trk_used_bytes >= old) _trk_used_bytes -= old; else _trk_used_bytes=0;
  _trk_used_bytes += n;
  if (_trk_used_bytes > _trk_peak_bytes) _trk_peak_bytes = _trk_used_bytes;
  if (n > _trk_max_block) _trk_max_block = n;
  return np;
}

/* --- Stats-Getter (für dein Logging) --- */
size_t lvgl_mem_stat_used(void)      { return _trk_used_bytes; }
size_t lvgl_mem_stat_peak(void)      { return _trk_peak_bytes; }
size_t lvgl_mem_stat_blocks(void)    { return _trk_blocks; }
size_t lvgl_mem_stat_max_alloc(void) { return _trk_max_block; }

#ifdef __cplusplus
}
#endif
#endif /* LVGL_MEM_CUSTOM_H */

