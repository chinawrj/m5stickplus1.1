/**
 * @file lvgl_button_input.h
 * @brief LVGL Input Device Driver for M5StickC Plus 1.1 GPIO Buttons
 * 
 * This driver bridges the GPIO button system with LVGL's input device framework,
 * converting Button A/B presses into LVGL keypad events (OK/NEXT).
 * 
 * Button Mapping:
 * - Button A (GPIO37) -> LV_KEY_ENTER (OK action)
 * - Button B (GPIO39) -> LV_KEY_NEXT (Navigation action)
 * 
 * Hardware:
 * - Button A: GPIO37 (active LOW, input-only pin, external pull-up)
 * - Button B: GPIO39 (active LOW, input-only pin, external pull-up)
 * 
 * @author M5StickC Plus Project
 * @date 2025-09-18
 */

#ifndef LVGL_BUTTON_INPUT_H
#define LVGL_BUTTON_INPUT_H

#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief LVGL Key mapping for M5StickC Plus buttons
 */
typedef enum {
    LVGL_KEY_NONE = 0,              ///< No key pressed
    LVGL_KEY_OK = LV_KEY_ENTER,     ///< Button A -> OK/Enter (for confirmations)
    LVGL_KEY_NEXT = LV_KEY_RIGHT,   ///< Button B -> Right Arrow (for navigation)
} lvgl_key_t;

/**
 * @brief Initialize LVGL input device driver for buttons
 * 
 * This function:
 * 1. Initializes the GPIO button system (if not already done)
 * 2. Creates and registers an LVGL input device with LV_INDEV_TYPE_KEYPAD
 * 3. Sets up the button-to-key mapping (A=OK, B=NEXT)
 * 4. Enables input device for page navigation
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t lvgl_button_input_init(void);

/**
 * @brief Get the LVGL input device handle
 * 
 * @return lv_indev_t* Input device handle, or NULL if not initialized
 */
lv_indev_t* lvgl_button_input_get_device(void);

/**
 * @brief Enable/disable button input processing
 * 
 * When disabled, button presses will not generate LVGL events.
 * Useful for temporarily disabling input during certain operations.
 * 
 * @param enabled Input processing state
 */
void lvgl_button_input_set_enabled(bool enabled);

/**
 * @brief Check if button input processing is enabled
 * 
 * @return true if input processing is enabled
 */
bool lvgl_button_input_is_enabled(void);

/**
 * @brief Check if input device is in event-driven mode
 * 
 * @return true if device is in LV_INDEV_MODE_EVENT, false otherwise
 */
bool lvgl_button_input_is_event_driven(void);

/**
 * @brief Get the last key pressed (for debugging)
 * 
 * @return lvgl_key_t Last key that was pressed, or LVGL_KEY_NONE
 */
lvgl_key_t lvgl_button_input_get_last_key(void);

/**
 * @brief Deinitialize LVGL input device driver
 * 
 * Cleans up the input device registration and resources.
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t lvgl_button_input_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // LVGL_BUTTON_INPUT_H