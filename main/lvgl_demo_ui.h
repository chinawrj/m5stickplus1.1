/*
 * Simple LVGL Text Demo for M5StickC Plus 1.1
 * Header file
 */

#ifndef LVGL_DEMO_UI_H
#define LVGL_DEMO_UI_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create simple LVGL demo UI with text display
 * 
 * @param disp LVGL display handle
 */
void m5stick_lvgl_demo_ui(lv_display_t *disp);

#ifdef __cplusplus
}
#endif

#endif // LVGL_DEMO_UI_H