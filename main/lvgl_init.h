/*
 * LVGL Integration for M5StickC Plus 1.1
 * Header file
 */

#ifndef LVGL_INIT_H
#define LVGL_INIT_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize LVGL with M5StickC Plus LCD
 * 
 * This function initializes the LVGL library, sets up the LCD panel,
 * configures display drivers, and creates the demo UI.
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t lvgl_init_with_m5stick_lcd(void);

#ifdef __cplusplus
}
#endif

#endif // LVGL_INIT_H