/*
 * LVGL Integration for M5StickC Plus 1.1
 * Header file
 */

#ifndef LVGL_INIT_H
#define LVGL_INIT_H

#include "esp_err.h"
#include "lvgl.h"

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

/**
 * @brief Initialize LVGL with M5StickC Plus LCD (without demo UI)
 * 
 * This function initializes the LVGL library and sets up the LCD panel,
 * but does not create any demo UI. Use this for custom page management.
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t lvgl_init_base(void);

/**
 * @brief Initialize LVGL with M5StickC Plus LCD and return display handle
 * 
 * This function initializes LVGL without creating any demo UI,
 * allowing for custom page management.
 * 
 * @param display Pointer to store the display handle
 * @return ESP_OK on success, error code otherwise  
 */
esp_err_t lvgl_init_for_page_manager(lv_display_t **display);

#ifdef __cplusplus
}
#endif

#endif // LVGL_INIT_H