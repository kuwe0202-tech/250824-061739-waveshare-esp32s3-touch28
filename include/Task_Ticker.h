#ifndef TASK_TICKER_H
#define TASK_TICKER_H

#include <Arduino.h>

using TaskCallback = void (*)();
using TaskDropCallback = void (*)(const char* dropped_task_name);

struct ScheduledTask {
    unsigned long nextRun;   
    unsigned long interval;  
    TaskCallback  callback;  
    bool          repeat;    
    bool          active;    
    bool          running;   
    const char*   name;      
    uint32_t      overrun_count; 
};

#ifndef MAX_TASKS
#define MAX_TASKS 16
#endif

// -----------------------------------------------------------------------------
// --- API-Nutzungshinweise ---
//
// @warning **ISR-Sicherheit:** Diese API ist NICHT ISR-sicher. Alle Funktionen
//          (addTask*, removeTask, etc.) dürfen nur aus dem Task-Kontext
//          (z.B. setup(), loop(), oder von FreeRTOS-Tasks) aufgerufen werden.
//
// @warning **Reentranz:** Task-Callbacks sollten keine Ticker-APIs aufrufen,
//          die die Task-Liste modifizieren (z.B. `removeTask(my_own_id)`).
//          Dies kann zu unvorhersehbarem Verhalten oder Deadlocks führen.
//
// @note    **Lebensdauer von Namen:** Der `name`-Parameter für alle `*Named`-
//          Funktionen muss auf einen String mit statischer Lebensdauer zeigen
//          (z.B. ein String-Literal wie "mein.task" oder eine globale/statische
//          Variable). Ein Zeiger auf einen temporären Puffer ist ungültig.
//
// -----------------------------------------------------------------------------

void task_ticker_loop(); 

// --- Veraltete APIs ---
#if __cplusplus >= 201402L
[[deprecated("Bitte addTaskNamed verwenden. Anonyme Tasks sind nicht mehr vorgesehen.")]]
#endif
int  addTask(unsigned long delayMs, TaskCallback cb, bool repeat = false);

#if __cplusplus >= 201402L
[[deprecated("Bitte addTaskPhasedNamed verwenden. Anonyme Tasks sind nicht mehr vorgesehen.")]]
#endif
int  addTaskPhased(unsigned long initialDelayMs, unsigned long intervalMs, TaskCallback cb, bool repeat = false);

#if __cplusplus >= 201402L
[[deprecated("Bitte addTaskImmediateNamed verwenden. Anonyme Tasks sind nicht mehr vorgesehen.")]]
#endif
int  addTaskImmediate(unsigned long intervalMs, TaskCallback cb, bool repeat = false);

// --- Bevorzugte APIs ---
/**
 * @brief Fügt einen neuen benannten Task hinzu oder aktualisiert einen bestehenden.
 * @param name Ein eindeutiger Name für den Task. Muss eine statische Lebensdauer haben (z.B. ein String-Literal).
 * @param delayMs Verzögerung in Millisekunden, nach der der Task zum ersten Mal ausgeführt wird.
 * @param cb Die Callback-Funktion, die ausgeführt werden soll.
 * @param repeat Ob der Task wiederholt werden soll.
 * @return Die ID des Tasks oder -1 bei einem Fehler.
 */
int  addTaskNamed(const char* name, unsigned long delayMs, TaskCallback cb, bool repeat = false);
int  addTaskPhasedNamed(const char* name, unsigned long initialDelayMs, unsigned long intervalMs, TaskCallback cb, bool repeat = false);
int  addTaskImmediateNamed(const char* name, unsigned long intervalMs, TaskCallback cb, bool repeat = false);

void removeTask(int taskId);
void updateTaskInterval(int taskId, unsigned long newInterval);


int  task_slots_free();   
int  task_count_active(); 
void dump_tasks();        

void setTickBudgetMs(uint16_t ms);
uint16_t getTickBudgetMs();
unsigned long getTickerLoopLastUs();
uint32_t getTickerOverrunsCount();
long getTaskLastRuntimeUs(int taskId);
uint32_t getTaskOverrunCount(int taskId);

// --- APIs für Fehlersichtbarkeit ---
uint32_t getTaskDropsCount();
void setTaskDropCallback(TaskDropCallback cb);

#endif