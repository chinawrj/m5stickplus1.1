#pragma once

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

#ifdef __cplusplus
}
#endif