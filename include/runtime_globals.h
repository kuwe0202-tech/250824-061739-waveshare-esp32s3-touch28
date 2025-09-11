// In neuer Datei: src/runtime_globals.h

#pragma once
#include <atomic>

// Deklariert unsere globale Variable, damit andere Dateien davon wissen.
// 'extern' bedeutet "Diese Variable existiert, aber sie ist in einer anderen .cpp-Datei definiert".
extern std::atomic<bool> g_boot_phase_active;