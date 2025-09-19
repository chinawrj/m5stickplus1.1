#ifndef UX_SERVICE_H
#define UX_SERVICE_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <stdint.h>

// UX Service Configuration
#define UX_SERVICE_QUEUE_SIZE           10
#define UX_SERVICE_TASK_STACK_SIZE      4096
#define UX_SERVICE_TASK_PRIORITY        5
#define UX_SERVICE_TASK_NAME            "ux_service_task"

/**
 * @brief UX Effect Types
 */
typedef enum {
    UX_EFFECT_NONE = 0,
    
    // LED Effects (Maximum 5)
    UX_LED_OFF,                         // Turn LED off
    UX_LED_ON,                          // Turn LED on
    UX_LED_BLINK_FAST,                  // Fast blink pattern
    UX_LED_BREATHING,                   // Breathing effect
    UX_LED_SUCCESS_PATTERN,             // Success indication pattern
    
    // Buzzer Effects (Maximum 5)
    UX_BUZZER_SILENCE,                  // Stop buzzer
    UX_BUZZER_STARTUP,                  // Startup melody
    UX_BUZZER_SUCCESS,                  // Success melody
    UX_BUZZER_ERROR,                    // Error melody
    UX_BUZZER_NOTIFICATION,             // Notification beep
    
    UX_EFFECT_MAX
} ux_effect_type_t;

/**
 * @brief UX Message Structure
 */
typedef struct {
    ux_effect_type_t effect_type;       // Type of effect to execute
    uint32_t duration_ms;               // Duration for the effect (0 = default)
    uint32_t repeat_count;              // Number of repetitions (0 = default/single)
    uint32_t parameter;                 // Additional parameter (frequency, interval, etc.)
} ux_message_t;

/**
 * @brief UX Service Statistics
 */
typedef struct {
    uint32_t messages_processed;        // Total messages processed
    uint32_t led_effects_count;         // LED effects executed
    uint32_t buzzer_effects_count;      // Buzzer effects executed
    uint32_t queue_full_errors;         // Queue full error count
    uint32_t execution_errors;          // Effect execution error count
} ux_service_stats_t;

/**
 * @brief Initialize UX Service
 * 
 * Creates message queue and starts the UX service task.
 * Must be called early in app_main() before sending any messages.
 * 
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t ux_service_init(void);

/**
 * @brief Deinitialize UX Service
 * 
 * Stops the service task and cleans up resources.
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ux_service_deinit(void);

/**
 * @brief Send UX Effect Message
 * 
 * Sends a message to the UX service queue for processing.
 * 
 * @param effect_type Type of effect to execute
 * @param duration_ms Duration in milliseconds (0 = use default)
 * @param repeat_count Number of repetitions (0 = use default)
 * @param parameter Additional parameter (frequency, interval, etc.)
 * @return esp_err_t ESP_OK on success, ESP_ERR_TIMEOUT if queue full
 */
esp_err_t ux_service_send_effect(ux_effect_type_t effect_type, 
                                  uint32_t duration_ms,
                                  uint32_t repeat_count,
                                  uint32_t parameter);

/**
 * @brief Send Simple UX Effect Message
 * 
 * Simplified version with default parameters.
 * 
 * @param effect_type Type of effect to execute
 * @return esp_err_t ESP_OK on success, ESP_ERR_TIMEOUT if queue full
 */
esp_err_t ux_service_send_simple_effect(ux_effect_type_t effect_type);

/**
 * @brief Get UX Service Statistics
 * 
 * Returns current service statistics.
 * 
 * @param stats Pointer to statistics structure to fill
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ux_service_get_stats(ux_service_stats_t *stats);

/**
 * @brief Check if UX Service is Running
 * 
 * @return bool true if service is running, false otherwise
 */
bool ux_service_is_running(void);

/**
 * @brief LED Effect Convenience Functions
 * 
 * These functions provide easy access to common LED effects
 * without needing to construct messages manually.
 */
esp_err_t ux_led_off(void);
esp_err_t ux_led_on(void);
esp_err_t ux_led_blink_fast(uint32_t duration_ms);
esp_err_t ux_led_breathing(uint32_t cycles);
esp_err_t ux_led_success_pattern(void);

/**
 * @brief Buzzer Effect Convenience Functions
 * 
 * These functions provide easy access to common buzzer effects
 * without needing to construct messages manually.
 */
esp_err_t ux_buzzer_silence(void);
esp_err_t ux_buzzer_startup(void);
esp_err_t ux_buzzer_success(void);
esp_err_t ux_buzzer_error(void);
esp_err_t ux_buzzer_notification(void);

/**
 * @brief Demo Functions for Testing
 * 
 * These functions demonstrate all available UX effects
 * for testing and validation purposes.
 */
esp_err_t ux_service_demo_all_led_effects(void);
esp_err_t ux_service_demo_all_buzzer_effects(void);
esp_err_t ux_service_demo_all_effects(void);

#endif // UX_SERVICE_H