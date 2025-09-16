/**
 * @file button.h
 * @brief M5StickC Plus Button Driver
 * 
 * This driver provides dual-mode button reading support:
 * 1. Interrupt mode - Real-time button press detection with immediate callbacks
 * 2. Polling mode - Periodic button state checking in monitoring task
 * 
 * Hardware:
 * - Button A: GPIO37 (active LOW, input-only pin, external pull-up required)
 * - Button B: GPIO39 (active LOW, input-only pin, external pull-up required)
 * 
 * Features:
 * - Interrupt-driven button press detection
 * - Debouncing support
 * - Polling mode for battery monitoring task integration
 * - Short press and long press detection
 * - Button state tracking
 * 
 * @author M5StickC Plus Project
 * @date 2025-09-16
 */

#ifndef BUTTON_H
#define BUTTON_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

// Button GPIO definitions
#define BUTTON_A_PIN    GPIO_NUM_37    // Button A on GPIO37
#define BUTTON_B_PIN    GPIO_NUM_39    // Button B on GPIO39

// Button states (active LOW)
#define BUTTON_PRESSED_LEVEL    0      // Button pressed = LOW
#define BUTTON_RELEASED_LEVEL   1      // Button released = HIGH

// Button debounce settings
#define BUTTON_DEBOUNCE_MS      50     // Debounce time in milliseconds
#define BUTTON_LONG_PRESS_MS    1000   // Long press threshold in milliseconds

/**
 * @brief Button identifiers
 */
typedef enum {
    BUTTON_A = 0,    ///< Button A (GPIO37)
    BUTTON_B = 1,    ///< Button B (GPIO39)
    BUTTON_COUNT     ///< Total number of buttons
} button_id_t;

/**
 * @brief Button event types
 */
typedef enum {
    BUTTON_EVENT_PRESSED,      ///< Button just pressed
    BUTTON_EVENT_RELEASED,     ///< Button just released
    BUTTON_EVENT_SHORT_PRESS,  ///< Short press completed
    BUTTON_EVENT_LONG_PRESS,   ///< Long press detected
    BUTTON_EVENT_NONE          ///< No event
} button_event_t;

/**
 * @brief Button state information
 */
typedef struct {
    bool current_state;         ///< Current button state (true = pressed)
    bool previous_state;        ///< Previous button state
    uint32_t press_start_time;  ///< Time when button was pressed (ms)
    uint32_t press_duration;    ///< Duration of current/last press (ms)
    uint32_t press_count;       ///< Total press count since init
    bool long_press_triggered;  ///< Flag to prevent multiple long press events
} button_state_t;

/**
 * @brief Button interrupt callback function type
 * 
 * @param button_id Which button triggered the interrupt
 * @param event Type of button event
 * @param press_duration Duration of press if applicable (ms)
 */
typedef void (*button_callback_t)(button_id_t button_id, button_event_t event, uint32_t press_duration);

/**
 * @brief Initialize button driver
 * 
 * Sets up GPIO pins for both buttons with interrupt capability
 * and internal pull-up resistors.
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t button_init(void);

/**
 * @brief Deinitialize button driver
 * 
 * Disables interrupts and resets GPIO configuration.
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t button_deinit(void);

/**
 * @brief Set interrupt callback function
 * 
 * @param callback Function to call when button events occur
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t button_set_interrupt_callback(button_callback_t callback);

/**
 * @brief Enable/disable interrupt mode for buttons
 * 
 * @param enable true to enable interrupts, false to disable
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t button_set_interrupt_mode(bool enable);

/**
 * @brief Read current button state (polling mode)
 * 
 * This function should be called periodically from the battery monitoring task.
 * It performs debouncing and event detection without interrupts.
 * 
 * @param button_id Which button to read
 * @return true if button is currently pressed, false otherwise
 */
bool button_is_pressed(button_id_t button_id);

/**
 * @brief Poll all buttons and detect events (polling mode)
 * 
 * This function should be called periodically from the battery monitoring task.
 * It performs debouncing and generates button events without interrupts.
 * 
 * @param button_id Button to poll
 * @return Button event detected, or BUTTON_EVENT_NONE if no event
 */
button_event_t button_poll_event(button_id_t button_id);

/**
 * @brief Get button state information
 * 
 * @param button_id Which button to query
 * @param state Pointer to store button state information
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t button_get_state(button_id_t button_id, button_state_t *state);

/**
 * @brief Get button press count
 * 
 * @param button_id Which button to query
 * @return Number of times button has been pressed since init
 */
uint32_t button_get_press_count(button_id_t button_id);

/**
 * @brief Reset button press count
 * 
 * @param button_id Which button to reset
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t button_reset_press_count(button_id_t button_id);

/**
 * @brief Test all button functions
 * 
 * Demonstrates both interrupt and polling modes with comprehensive testing.
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t button_test_all_functions(void);

/**
 * @brief Convert button ID to string name
 * 
 * @param button_id Button identifier
 * @return String name of the button
 */
const char* button_get_name(button_id_t button_id);

#ifdef __cplusplus
}
#endif

#endif // BUTTON_H