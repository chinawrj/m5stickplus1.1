#pragma once

#include "esp_err.h"
#include "page_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief ESP-NOW Page Manager
 * 
 * This module manages the ESP-NOW page independently, including:
 * - Page UI creation and destruction
 * - ESP-NOW communication status updates
 * - Independent data update checking
 * - Network statistics management
 */

/**
 * @brief Get ESP-NOW page controller instance
 * 
 * Returns a pointer to the ESP-NOW page controller with all function
 * pointers properly initialized.
 * 
 * @return Pointer to ESP-NOW page controller interface
 */
const page_controller_t* get_espnow_page_controller(void);

/**
 * @brief Initialize ESP-NOW page module
 * 
 * Sets up internal state and prepares the module for use.
 * Must be called before using any other functions.
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t espnow_page_init(void);

/**
 * @brief Create ESP-NOW page UI
 * 
 * Creates all UI elements for the ESP-NOW page including:
 * - Communication status labels
 * - Packet send/receive counters
 * - Signal strength indicators
 * - Network configuration display
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t espnow_page_create(void);

/**
 * @brief Update ESP-NOW page data
 * 
 * Refreshes all dynamic data on the ESP-NOW page:
 * - Packet statistics
 * - Signal strength
 * - Connection status
 * - Network performance metrics
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t espnow_page_update(void);

/**
 * @brief Destroy ESP-NOW page
 * 
 * Cleans up all LVGL objects and resources used by the ESP-NOW page.
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t espnow_page_destroy(void);

/**
 * @brief Check if ESP-NOW page data has been updated (atomically clears flag)
 * @return true if data was updated since last check, false otherwise
 * @note This function atomically checks and clears the update flag to avoid race conditions
 */
bool espnow_page_is_data_updated(void);/**
 * @brief Notify ESP-NOW page of data update
 * 
 * Called by ESP-NOW communication handlers to indicate that
 * page data should be refreshed on next update cycle.
 */
void espnow_page_notify_data_update(void);

#ifdef __cplusplus
}
#endif