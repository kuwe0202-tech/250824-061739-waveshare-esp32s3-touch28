/* === BEGIN FILE: src\ui.cpp === */
#include "ui.h"
#include "screens.h"
#include "images.h"
#include "actions.h"
#include "styles.h"
#include "Konfiguration.h" 


static int16_t currentScreen = -1;


void loadScreen(enum ScreensEnum screenId) {
    currentScreen = screenId - 1;
    
    lv_obj_t *screen = ((lv_obj_t **)&objects)[currentScreen];
    if (screen) {
        
        lv_scr_load_anim(screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
    }
}


void ui_init() {
    
    lv_disp_t *dispp = lv_disp_get_default();
    lv_theme_t *theme = lv_theme_default_init(dispp, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED), false, LV_FONT_DEFAULT);
    lv_disp_set_theme(dispp, theme);

    
    create_screens();

    
    // NEU: Bedingtes Laden des Startbildschirms basierend auf FSM-Konfiguration
#if CFG_BOOT_FSM
    
    loadScreen(SCREEN_ID_PAGE_STARTBILDSCHIRM);
#else
    
    loadScreen(SCREEN_ID_PAGE_DESKTOP);
#endif
}


void ui_tick() {
    
    
    if (currentScreen != -1) {
        tick_screen(currentScreen);
    }
}
/* === END FILE: src\ui.cpp === */