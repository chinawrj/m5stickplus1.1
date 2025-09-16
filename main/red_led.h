#ifndef RED_LED_H
#define RED_LED_H

#include "esp_err.h"
#include "driver/gpio.h"
#include <stdint.h>
#include <stdbool.h>

// M5StickC Plus 1.1 Red LED Configuration
#define RED_LED_PIN             GPIO_NUM_10
#define RED_LED_ON_LEVEL        0    // LED is active LOW (0 = ON, 1 = OFF)
#define RED_LED_OFF_LEVEL       1

// Blink patterns for different states
#define BLINK_FAST_MS           100   // Fast blink interval
#define BLINK_NORMAL_MS         250   // Normal blink interval  
#define BLINK_SLOW_MS           500   // Slow blink interval
#define BLINK_VERY_SLOW_MS      1000  // Very slow blink interval

/**
 * @brief LED state enumeration
 */
typedef enum {
    LED_STATE_OFF = 0,        // LED off
    LED_STATE_ON,             // LED on
    LED_STATE_BLINK_FAST,     // Fast blink
    LED_STATE_BLINK_NORMAL,   // Normal blink
    LED_STATE_BLINK_SLOW,     // Slow blink
    LED_STATE_BLINK_VERY_SLOW // Very slow blink
} led_state_t;

/**
 * @brief LED blink pattern structure
 */
typedef struct {
    uint32_t on_time_ms;      // On time (milliseconds)
    uint32_t off_time_ms;     // Off time (milliseconds)
    uint32_t repeat_count;    // Repeat count (0 = infinite loop)
} led_blink_pattern_t;

/**
 * @brief Initialize the red LED (GPIO10)
 * 
 * This function configures GPIO10 as an output pin for controlling
 * the red LED on M5StickC Plus.
 * 
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t red_led_init(void);

/**
 * @brief Deinitialize the red LED and free resources
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t red_led_deinit(void);

/**
 * @brief Turn on the red LED
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t red_led_on(void);

/**
 * @brief Turn off the red LED
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t red_led_off(void);

/**
 * @brief Toggle the red LED state
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t red_led_toggle(void);

/**
 * @brief Set LED state (on/off)
 * 
 * @param state true to turn on, false to turn off
 * @return esp_err_t ESP_OK on success
 */
esp_err_t red_led_set_state(bool state);

/**
 * @brief Get current LED state
 * 
 * @return bool true if LED is on, false if off
 */
bool red_led_get_state(void);

/**
 * @brief Blink the LED with specified pattern
 * 
 * @param on_time_ms Time to keep LED on (milliseconds)
 * @param off_time_ms Time to keep LED off (milliseconds)  
 * @param repeat_count Number of blink cycles (0 = infinite)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t red_led_blink_pattern(uint32_t on_time_ms, uint32_t off_time_ms, uint32_t repeat_count);

/**
 * @brief Simple blink function
 * 
 * @param count Number of blinks
 * @param interval_ms Blink interval in milliseconds
 * @return esp_err_t ESP_OK on success
 */
esp_err_t red_led_blink(uint32_t count, uint32_t interval_ms);

/**
 * @brief Start continuous blinking with preset patterns
 * 
 * @param state LED blinking state
 * @return esp_err_t ESP_OK on success
 */
esp_err_t red_led_set_blink_state(led_state_t state);

/**
 * @brief Stop any ongoing LED operations
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t red_led_stop(void);

/**
 * @brief LED breathing effect (fade in/out)
 * 
 * Creates a breathing effect by rapidly toggling the LED.
 * Note: This is a basic implementation for hardware without PWM dimming.
 * 
 * @param cycles Number of breathing cycles
 * @param cycle_duration_ms Duration of one complete breathing cycle
 * @return esp_err_t ESP_OK on success
 */
esp_err_t red_led_breathing(uint32_t cycles, uint32_t cycle_duration_ms);

/**
 * @brief Morse code transmission using LED
 * 
 * @param message Message to transmit in Morse code (A-Z, 0-9, space)
 * @param dot_duration_ms Duration of a dot (base unit)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t red_led_morse_code(const char* message, uint32_t dot_duration_ms);

/**
 * @brief LED status indication patterns
 * 
 * Predefined patterns for common status indications:
 * - Boot sequence
 * - Success indication  
 * - Error indication
 * - Warning indication
 * - Processing indication
 */
esp_err_t red_led_indicate_boot(void);      // Boot sequence pattern
esp_err_t red_led_indicate_success(void);   // Success pattern
esp_err_t red_led_indicate_error(void);     // Error pattern  
esp_err_t red_led_indicate_warning(void);   // Warning pattern
esp_err_t red_led_indicate_processing(void); // Processing pattern

/**
 * @brief Comprehensive LED test patterns
 * 
 * Tests all LED functionality:
 * - Basic on/off
 * - Toggle test
 * - Various blink patterns
 * - Breathing effect
 * - Status indications
 * - Morse code demo
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t red_led_test_patterns(void);

#endif // RED_LED_H