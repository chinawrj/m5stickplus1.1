#pragma once

#include "esp_err.h"
#include "lvgl.h"
#include "page_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Monitor Page Manager
 * 
 * This module manages the Monitor page independently, including:
 * - Page UI creation and destruction
 * - System data updates and display
 * - Independent data update checking
 * - Integration with system_monitor module
 */

/**
 * @brief Get Monitor page controller instance
 * 
 * Returns a pointer to the Monitor page controller with all function
 * pointers properly initialized.
 * 
 * @return Pointer to Monitor page controller interface
 */
const page_controller_t* get_monitor_page_controller(void);

/**
 * @brief Initialize Monitor page module
 * 
 * Sets up internal state and prepares the module for use.
 * Must be called before using any other functions.
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t monitor_page_init(void);

/**
 * @brief Create Monitor page UI
 * 
 * Creates all UI elements for the Monitor page including:
 * - System status labels
 * - Uptime display
 * - Memory usage display
 * - Task count and status
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t monitor_page_create(void);

/**
 * @brief Update Monitor page data
 * 
 * Refreshes all dynamic data on the Monitor page:
 * - System uptime
 * - Available memory
 * - Task statistics
 * - System health indicators
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t monitor_page_update(void);

/**
 * @brief Destroy Monitor page
 * 
 * Cleans up all LVGL objects and resources used by the Monitor page.
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t monitor_page_destroy(void);

/**
 * @brief Check if monitor page data has been updated (atomically clears flag)
 * @return true if data was updated since last check, false otherwise
 * @note This function atomically checks and clears the update flag to avoid race conditions
 */
bool monitor_page_is_data_updated(void);

#ifdef __cplusplus
}
#endif