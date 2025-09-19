/*
 * Page Manager for M5StickC Plus 1.1 Multi-Page LVGL Application
 * Manages page switching and navigation using screen management
 */

#ifndef PAGE_MANAGER_H
#define PAGE_MANAGER_H

#include "lvgl.h"
#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Page identifiers
typedef enum {
    PAGE_MONITOR = 0,    // System monitor page
    PAGE_ESPNOW,         // ESP-NOW communication page
    PAGE_COUNT           // Total number of pages
} page_id_t;

/**
 * @brief Page controller interface
 * Provides standardized interface for modular page management
 */
typedef struct {
    // Page lifecycle functions
    esp_err_t (*init)(void);            // Initialize page module (called once)
    esp_err_t (*create)(void);          // Create page UI elements
    esp_err_t (*update)(void);          // Update page data display
    esp_err_t (*destroy)(void);         // Clean up page resources
    
    // Data state management
    bool (*is_data_updated)(void);      // Check if page data needs update (atomically clears flag)
    
    // Input event handling (optional)
    bool (*handle_key_event)(uint32_t key);  // Handle page-specific key events, return true if handled
    
    // Page information
    const char *name;                   // Page display name
    page_id_t page_id;                  // Page identifier
} page_controller_t;

// Page navigation direction
typedef enum {
    NAV_NEXT = 0,        // Navigate to next page
    NAV_PREV,            // Navigate to previous page
    NAV_DIRECT           // Navigate directly to specific page
} nav_direction_t;

// Page creation function pointer type
typedef esp_err_t (*page_create_func_t)(lv_obj_t *screen);

// Page update function pointer type
typedef void (*page_update_func_t)(void);

// Page cleanup function pointer type
typedef void (*page_cleanup_func_t)(void);

// Page information structure
typedef struct {
    const char *name;              // Page name for logging
    page_create_func_t create;     // Page creation function
    page_update_func_t update;     // Page update function (optional)
    page_cleanup_func_t cleanup;   // Page cleanup function (optional)
    lv_obj_t *screen;             // Screen object handle
    bool created;                 // Page creation status
} page_info_t;

/**
 * @brief Initialize page manager system
 * @param disp LVGL display handle
 * @return ESP_OK on success
 */
esp_err_t page_manager_init(lv_display_t *disp);

/**
 * @brief Navigate to next page
 * @return ESP_OK on success
 */
esp_err_t page_manager_next(void);

/**
 * @brief Navigate to previous page  
 * @return ESP_OK on success
 */
esp_err_t page_manager_prev(void);

/**
 * @brief Navigate to specific page
 * @param page_id Target page ID
 * @return ESP_OK on success
 */
esp_err_t page_manager_goto(page_id_t page_id);

/**
 * @brief Get current active page ID
 * @return Current page ID
 */
page_id_t page_manager_get_current(void);

/**
 * @brief Get page name by ID
 * @param page_id Page ID
 * @return Page name string
 */
const char* page_manager_get_name(page_id_t page_id);

/**
 * @brief Update current page (if page has update function)
 */
void page_manager_update_current(void);

/**
 * @brief Handle key event for current page
 * @param key LVGL key code
 * @return true if key was handled by page, false if should be handled globally
 */
bool page_manager_handle_key_event(uint32_t key);

/**
 * @brief Check if navigation is available
 * @return true if pages can be switched
 */
bool page_manager_is_navigation_enabled(void);

/**
 * @brief Enable/disable navigation
 * @param enabled Navigation state
 */
void page_manager_set_navigation_enabled(bool enabled);

/**
 * @brief Cleanup page manager
 * @return ESP_OK on success
 */
esp_err_t page_manager_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // PAGE_MANAGER_H