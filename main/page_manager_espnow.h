#pragma once

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
 * @brief Notify ESP-NOW page of data update
 * 
 * Called by ESP-NOW communication handlers to indicate that
 * page data should be refreshed on next update cycle.
 */
void espnow_page_notify_data_update(void);

#ifdef __cplusplus
}
#endif