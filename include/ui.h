#ifndef EEZ_LVGL_UI_GUI_H
#define EEZ_LVGL_UI_GUI_H

#include <lvgl.h>
#include "screens.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialisiert die gesamte Benutzeroberfläche, erstellt alle Screens und Widgets.
 */
void ui_init();

/**
 * @brief Diese Funktion kann für periodische UI-Updates genutzt werden.
 * In der bereinigten Version ist sie meist leer und die Logik
 * sollte direkt in deiner Applikation oder den `tick_screen_*` Funktionen liegen.
 */
void ui_tick();

/**
 * @brief Lädt einen bestimmten Screen. Dies ist die zentrale Navigationsfunktion.
 * @param screenId Die ID des zu ladenden Screens aus der `ScreensEnum`.
 */
void loadScreen(enum ScreensEnum screenId);

#ifdef __cplusplus
}
#endif

#endif