/*
 * Button Navigation Handler for M5StickC Plus 1.1 Multi-Page Application
 * Handles button press events for page navigation
 */

#ifndef BUTTON_NAV_H
#define BUTTON_NAV_H

#include "esp_err.h"
#include "button.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize button navigation system
 * @return ESP_OK on success
 */
esp_err_t button_nav_init(void);

/**
 * @brief Enable/disable button navigation
 * @param enabled Navigation state
 */
void button_nav_set_enabled(bool enabled);

/**
 * @brief Check if navigation is enabled
 * @return true if navigation is enabled
 */
bool button_nav_is_enabled(void);

/**
 * @brief Cleanup button navigation
 * @return ESP_OK on success
 */
esp_err_t button_nav_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // BUTTON_NAV_H