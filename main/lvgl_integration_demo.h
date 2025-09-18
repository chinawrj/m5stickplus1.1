/**
 * @file lvgl_integration_demo.h
 * @brief Integration Demo for LVGL Button System with M5StickC Plus 1.1
 * 
 * This demo shows how to integrate the new LVGL button input system
 * with the page manager for seamless navigation using Button A/B.
 * 
 * @author M5StickC Plus Project
 * @date 2025-09-18
 */

#ifndef LVGL_INTEGRATION_DEMO_H
#define LVGL_INTEGRATION_DEMO_H

#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Run complete LVGL button integration demonstration
 * 
 * This function demonstrates the complete workflow of the new LVGL button system:
 * 
 * 1. Initialize LVGL with display (using existing lvgl_init functions)
 * 2. Initialize LVGL button input device driver 
 * 3. Initialize LVGL-integrated page manager
 * 4. Run comprehensive tests to verify functionality
 * 5. Start page navigation with Button A (OK) and Button B (NEXT)
 * 6. Display real-time button event monitoring
 * 
 * Button Mapping:
 * - Button A (GPIO37) -> LV_KEY_ENTER (OK action)
 * - Button B (GPIO39) -> LV_KEY_NEXT (page navigation)
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t lvgl_integration_demo_run_complete(void);

/**
 * @brief Run quick integration test (for development)
 * 
 * This function runs a quick test to verify the basic integration
 * without full UI initialization.
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t lvgl_integration_demo_run_quick_test(void);

/**
 * @brief Initialize only the LVGL button input system
 * 
 * This function can be called from your main application to set up
 * just the LVGL button input system without the full demo.
 * 
 * @param display LVGL display handle (from lvgl_init)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t lvgl_integration_demo_init_button_system(lv_display_t *display);

/**
 * @brief Migration example: Convert from old button_nav to new LVGL system
 * 
 * This function shows how to migrate from the old button_nav.c system
 * to the new LVGL-integrated button system.
 * 
 * @param display LVGL display handle
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t lvgl_integration_demo_migrate_from_button_nav(lv_display_t *display);

#ifdef __cplusplus
}
#endif

#endif // LVGL_INTEGRATION_DEMO_H