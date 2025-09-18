/**
 * @file page_manager_lvgl.h
 * @brief LVGL-integrated Page Manager for M5StickC Plus 1.1
 * 
 * This is an enhanced version of page_manager.h that integrates with LVGL's
 * input device system to handle navigation through key events.
 * 
 * Key features:
 * - LVGL key event integration (LV_KEY_NEXT for navigation)
 * - Thread-safe page switching using LVGL timers
 * - Automatic LVGL group management for input focus
 * - Compatible with existing page content and update system
 * 
 * @author M5StickC Plus Project
 * @date 2025-09-18
 */

#ifndef PAGE_MANAGER_LVGL_H
#define PAGE_MANAGER_LVGL_H

#include "esp_err.h"
#include "lvgl.h"
#include "page_manager.h"  // Reuse existing page definitions

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize LVGL-integrated page manager
 * 
 * This function:
 * 1. Initializes the base page manager functionality
 * 2. Creates an LVGL group for input device focus
 * 3. Sets up key event handling for navigation
 * 4. Associates the input device with the group
 * 
 * @param display LVGL display handle
 * @param indev LVGL input device handle (from lvgl_button_input)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t page_manager_lvgl_init(lv_display_t *display, lv_indev_t *indev);

/**
 * @brief Enable/disable LVGL key event handling
 * 
 * When enabled, the page manager will respond to LV_KEY_NEXT events
 * for page navigation. When disabled, only manual navigation is allowed.
 * 
 * @param enabled Key event handling state
 */
void page_manager_lvgl_set_key_enabled(bool enabled);

/**
 * @brief Check if LVGL key event handling is enabled
 * 
 * @return true if key events are processed for navigation
 */
bool page_manager_lvgl_is_key_enabled(void);

/**
 * @brief Get the LVGL group used for navigation
 * 
 * @return lv_group_t* Navigation group, or NULL if not initialized
 */
lv_group_t* page_manager_lvgl_get_group(void);

/**
 * @brief Manual page navigation (same as page_manager_next)
 * 
 * This function provides the same functionality as the base page manager
 * but ensures proper LVGL group focus management.
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t page_manager_lvgl_next(void);

/**
 * @brief Manual page navigation (same as page_manager_prev)
 * 
 * This function provides the same functionality as the base page manager
 * but ensures proper LVGL group focus management.
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t page_manager_lvgl_prev(void);

/**
 * @brief Manual key event handler as fallback
 * 
 * This function is called directly from input device when LVGL's
 * automatic event routing is not working properly.
 * 
 * @param key LVGL key code (LV_KEY_NEXT, LV_KEY_ENTER, etc.)
 */
void page_manager_manual_key_event(uint32_t key);

/**
 * @brief Deinitialize LVGL-integrated page manager
 * 
 * Cleans up LVGL groups and delegates to base page manager cleanup.
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t page_manager_lvgl_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // PAGE_MANAGER_LVGL_H