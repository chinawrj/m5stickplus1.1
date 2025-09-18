/**
 * @file lvgl_button_test.h
 * @brief Comprehensive Test Suite for LVGL Button Integration
 * 
 * This test suite validates the complete button-to-LVGL pipeline:
 * 1. GPIO button detection and interrupt handling
 * 2. Button-to-LVGL key event conversion
 * 3. LVGL input device registration and operation
 * 4. Page navigation through LVGL key events
 * 5. Integration with LVGL group system
 * 
 * @author M5StickC Plus Project
 * @date 2025-09-18
 */

#ifndef LVGL_BUTTON_TEST_H
#define LVGL_BUTTON_TEST_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Test result structure
 */
typedef struct {
    bool gpio_button_test_passed;          ///< GPIO button detection test result
    bool lvgl_input_device_test_passed;    ///< LVGL input device registration test result
    bool key_event_generation_test_passed; ///< Key event generation test result
    bool page_navigation_test_passed;      ///< Page navigation test result
    bool integration_test_passed;          ///< Overall integration test result
    uint32_t button_a_presses;             ///< Number of Button A presses detected
    uint32_t button_b_presses;             ///< Number of Button B presses detected
    uint32_t page_navigation_count;        ///< Number of successful page navigations
    char last_error[128];                  ///< Last error message
} lvgl_button_test_result_t;

/**
 * @brief Run comprehensive LVGL button integration test
 * 
 * This function performs a complete test of the button-to-LVGL pipeline:
 * 
 * 1. **GPIO Button Test**: Validates GPIO button detection and interrupts
 * 2. **LVGL Input Device Test**: Verifies LVGL input device registration
 * 3. **Key Event Generation Test**: Tests button-to-key event conversion
 * 4. **Page Navigation Test**: Validates LVGL key-based page navigation
 * 5. **Integration Test**: Comprehensive end-to-end validation
 * 
 * The test will prompt the user to press buttons and verify the complete
 * pipeline works correctly with comprehensive logging.
 * 
 * @param result Pointer to store detailed test results
 * @return ESP_OK if all tests pass, error code otherwise
 */
esp_err_t lvgl_button_test_run_comprehensive(lvgl_button_test_result_t *result);

/**
 * @brief Run quick LVGL button integration test (automated)
 * 
 * This function performs automated testing without user interaction,
 * focusing on initialization and basic functionality validation.
 * 
 * @param result Pointer to store test results
 * @return ESP_OK if basic tests pass, error code otherwise
 */
esp_err_t lvgl_button_test_run_quick(lvgl_button_test_result_t *result);

/**
 * @brief Test button press simulation (for debugging)
 * 
 * This function simulates button presses programmatically to test
 * the LVGL integration without physical button interaction.
 * 
 * @param button_a_presses Number of Button A presses to simulate
 * @param button_b_presses Number of Button B presses to simulate
 * @param result Pointer to store test results
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t lvgl_button_test_simulate_presses(uint32_t button_a_presses, 
                                          uint32_t button_b_presses,
                                          lvgl_button_test_result_t *result);

/**
 * @brief Display test results in a formatted way
 * 
 * @param result Test results to display
 */
void lvgl_button_test_print_results(const lvgl_button_test_result_t *result);

/**
 * @brief Monitor button events in real-time (for debugging)
 * 
 * This function runs a continuous monitoring loop that logs all
 * button events and LVGL key events as they occur. Useful for
 * debugging and verification during development.
 * 
 * @param duration_seconds Duration to monitor (0 = infinite)
 * @return ESP_OK when monitoring completes
 */
esp_err_t lvgl_button_test_monitor_events(uint32_t duration_seconds);

#ifdef __cplusplus
}
#endif

#endif // LVGL_BUTTON_TEST_H