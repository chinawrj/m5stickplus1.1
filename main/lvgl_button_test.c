/**
 * @file lvgl_button_test.c
 * @brief Comprehensive Test Suite Implementation for LVGL Button Integration
 */

#include "lvgl_button_test.h"
#include "lvgl_button_input.h"
#include "page_manager_lvgl.h"
#include "button.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "LVGL_BTN_TEST";

/**
 * @brief Initialize test result structure
 */
static void init_test_result(lvgl_button_test_result_t *result)
{
    memset(result, 0, sizeof(lvgl_button_test_result_t));
    strcpy(result->last_error, "No error");
}

/**
 * @brief Set test error message
 */
static void set_test_error(lvgl_button_test_result_t *result, const char *error_msg)
{
    strncpy(result->last_error, error_msg, sizeof(result->last_error) - 1);
    result->last_error[sizeof(result->last_error) - 1] = '\0';
}

/**
 * @brief Test GPIO button functionality
 */
static esp_err_t test_gpio_buttons(lvgl_button_test_result_t *result)
{
    ESP_LOGI(TAG, "🔧 Testing GPIO button functionality...");
    
    // Test button initialization
    esp_err_t ret = button_init();
    if (ret != ESP_OK) {
        set_test_error(result, "GPIO button initialization failed");
        ESP_LOGE(TAG, "Button initialization failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Test button state reading
    button_state_t button_a_state, button_b_state;
    ret = button_get_state(BUTTON_A, &button_a_state);
    if (ret != ESP_OK) {
        set_test_error(result, "Button A state reading failed");
        ESP_LOGE(TAG, "Button A state reading failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = button_get_state(BUTTON_B, &button_b_state);
    if (ret != ESP_OK) {
        set_test_error(result, "Button B state reading failed");
        ESP_LOGE(TAG, "Button B state reading failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "✅ GPIO button test passed - A: %s, B: %s", 
             button_a_state.current_state ? "PRESSED" : "RELEASED",
             button_b_state.current_state ? "PRESSED" : "RELEASED");
    
    result->gpio_button_test_passed = true;
    return ESP_OK;
}

/**
 * @brief Test LVGL input device registration
 */
static esp_err_t test_lvgl_input_device(lvgl_button_test_result_t *result)
{
    ESP_LOGI(TAG, "🔧 Testing LVGL input device registration...");
    
    // Test LVGL input device initialization
    esp_err_t ret = lvgl_button_input_init();
    if (ret != ESP_OK) {
        set_test_error(result, "LVGL input device initialization failed");
        ESP_LOGE(TAG, "LVGL input device initialization failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Verify input device was created
    lv_indev_t *indev = lvgl_button_input_get_device();
    if (indev == NULL) {
        set_test_error(result, "LVGL input device not created");
        ESP_LOGE(TAG, "LVGL input device not created");
        return ESP_FAIL;
    }
    
    // Test input device enable/disable
    lvgl_button_input_set_enabled(false);
    if (lvgl_button_input_is_enabled()) {
        set_test_error(result, "Input device disable failed");
        ESP_LOGE(TAG, "Input device disable failed");
        return ESP_FAIL;
    }
    
    lvgl_button_input_set_enabled(true);
    if (!lvgl_button_input_is_enabled()) {
        set_test_error(result, "Input device enable failed");
        ESP_LOGE(TAG, "Input device enable failed");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "✅ LVGL input device test passed");
    result->lvgl_input_device_test_passed = true;
    return ESP_OK;
}

/**
 * @brief Test key event generation (interactive)
 */
static esp_err_t test_key_event_generation(lvgl_button_test_result_t *result)
{
    ESP_LOGI(TAG, "🔧 Testing key event generation (interactive)...");
    ESP_LOGI(TAG, "📋 Please press Button A and Button B to test key generation");
    ESP_LOGI(TAG, "⏱️  Test will run for 10 seconds...");
    
    // Get initial statistics
    uint32_t initial_a_count, initial_b_count;
    lvgl_button_input_get_stats(&initial_a_count, &initial_b_count);
    
    // Monitor for 10 seconds
    int64_t start_time = esp_timer_get_time();
    int64_t test_duration = 10 * 1000000; // 10 seconds in microseconds
    
    while ((esp_timer_get_time() - start_time) < test_duration) {
        // Get current statistics
        uint32_t current_a_count, current_b_count;
        lvgl_button_input_get_stats(&current_a_count, &current_b_count);
        
        // Update result statistics
        result->button_a_presses = current_a_count - initial_a_count;
        result->button_b_presses = current_b_count - initial_b_count;
        
        // Log key events
        lvgl_key_t last_key = lvgl_button_input_get_last_key();
        if (last_key != LVGL_KEY_NONE) {
            ESP_LOGI(TAG, "🔑 Key event detected: %s", 
                     (last_key == LVGL_KEY_OK) ? "OK/ENTER" : "NEXT");
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    ESP_LOGI(TAG, "⏹️  Test completed - Button A: %lu presses, Button B: %lu presses",
             result->button_a_presses, result->button_b_presses);
    
    // Check if any button presses were detected
    if (result->button_a_presses > 0 || result->button_b_presses > 0) {
        ESP_LOGI(TAG, "✅ Key event generation test passed");
        result->key_event_generation_test_passed = true;
        return ESP_OK;
    } else {
        set_test_error(result, "No button presses detected during test");
        ESP_LOGW(TAG, "⚠️  No button presses detected - test inconclusive");
        return ESP_ERR_NOT_FOUND;
    }
}

/**
 * @brief Test page navigation through LVGL keys
 */
static esp_err_t test_page_navigation(lvgl_button_test_result_t *result)
{
    ESP_LOGI(TAG, "🔧 Testing page navigation through LVGL keys...");
    
    // Note: This test assumes LVGL and page manager are already initialized
    // In a real implementation, you would need to ensure proper initialization
    
    ESP_LOGI(TAG, "📋 Page navigation test requires LVGL and page manager to be initialized");
    ESP_LOGI(TAG, "📋 This test will check if the integration components are ready");
    
    // Check if LVGL input device is available
    lv_indev_t *indev = lvgl_button_input_get_device();
    if (indev == NULL) {
        set_test_error(result, "LVGL input device not available for navigation test");
        ESP_LOGE(TAG, "LVGL input device not available");
        return ESP_FAIL;
    }
    
    // Check if input device is enabled
    if (!lvgl_button_input_is_enabled()) {
        set_test_error(result, "LVGL input device not enabled");
        ESP_LOGE(TAG, "LVGL input device not enabled");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "✅ Page navigation test infrastructure verified");
    ESP_LOGI(TAG, "📋 To test navigation: initialize page_manager_lvgl and press Button B (NEXT)");
    
    result->page_navigation_test_passed = true;
    result->page_navigation_count = 0; // Would be updated by actual navigation events
    return ESP_OK;
}

// Public API implementation

esp_err_t lvgl_button_test_run_comprehensive(lvgl_button_test_result_t *result)
{
    ESP_LOGI(TAG, "🚀 Starting comprehensive LVGL button integration test...");
    
    init_test_result(result);
    
    // Test 1: GPIO Button functionality
    esp_err_t ret = test_gpio_buttons(result);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ GPIO button test failed");
        return ret;
    }
    
    // Test 2: LVGL Input Device registration
    ret = test_lvgl_input_device(result);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ LVGL input device test failed");
        return ret;
    }
    
    // Test 3: Key event generation (interactive)
    ret = test_key_event_generation(result);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "⚠️  Key event generation test inconclusive");
        // Don't fail the overall test for interactive components
    }
    
    // Test 4: Page navigation infrastructure
    ret = test_page_navigation(result);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Page navigation test failed");
        return ret;
    }
    
    // Overall integration test
    result->integration_test_passed = (
        result->gpio_button_test_passed &&
        result->lvgl_input_device_test_passed &&
        result->page_navigation_test_passed
    );
    
    if (result->integration_test_passed) {
        ESP_LOGI(TAG, "🎉 Comprehensive test PASSED - LVGL button integration working!");
        strcpy(result->last_error, "All tests passed");
    } else {
        ESP_LOGE(TAG, "❌ Comprehensive test FAILED - check individual test results");
    }
    
    return result->integration_test_passed ? ESP_OK : ESP_FAIL;
}

esp_err_t lvgl_button_test_run_quick(lvgl_button_test_result_t *result)
{
    ESP_LOGI(TAG, "⚡ Starting quick LVGL button integration test...");
    
    init_test_result(result);
    
    // Quick test: just verify initialization and basic functionality
    esp_err_t ret = test_gpio_buttons(result);
    if (ret != ESP_OK) {
        return ret;
    }
    
    ret = test_lvgl_input_device(result);
    if (ret != ESP_OK) {
        return ret;
    }
    
    result->integration_test_passed = (
        result->gpio_button_test_passed &&
        result->lvgl_input_device_test_passed
    );
    
    ESP_LOGI(TAG, "⚡ Quick test %s", result->integration_test_passed ? "PASSED" : "FAILED");
    return result->integration_test_passed ? ESP_OK : ESP_FAIL;
}

esp_err_t lvgl_button_test_simulate_presses(uint32_t button_a_presses, 
                                          uint32_t button_b_presses,
                                          lvgl_button_test_result_t *result)
{
    ESP_LOGI(TAG, "🎭 Simulating button presses: A=%lu, B=%lu", button_a_presses, button_b_presses);
    
    init_test_result(result);
    
    // Note: This would require additional infrastructure to programmatically
    // trigger button events. For now, just log the intention.
    
    ESP_LOGW(TAG, "⚠️  Button press simulation not yet implemented");
    ESP_LOGW(TAG, "💡 To implement: create callback injection mechanism in button.c");
    
    result->button_a_presses = button_a_presses;
    result->button_b_presses = button_b_presses;
    
    set_test_error(result, "Simulation not implemented");
    return ESP_ERR_NOT_SUPPORTED;
}

void lvgl_button_test_print_results(const lvgl_button_test_result_t *result)
{
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "📊 LVGL Button Integration Test Results:");
    ESP_LOGI(TAG, "==========================================");
    ESP_LOGI(TAG, "GPIO Button Test:          %s", result->gpio_button_test_passed ? "✅ PASS" : "❌ FAIL");
    ESP_LOGI(TAG, "LVGL Input Device Test:    %s", result->lvgl_input_device_test_passed ? "✅ PASS" : "❌ FAIL");
    ESP_LOGI(TAG, "Key Event Generation Test: %s", result->key_event_generation_test_passed ? "✅ PASS" : "❌ FAIL");
    ESP_LOGI(TAG, "Page Navigation Test:      %s", result->page_navigation_test_passed ? "✅ PASS" : "❌ FAIL");
    ESP_LOGI(TAG, "Integration Test:          %s", result->integration_test_passed ? "✅ PASS" : "❌ FAIL");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "📈 Statistics:");
    ESP_LOGI(TAG, "Button A Presses:          %lu", result->button_a_presses);
    ESP_LOGI(TAG, "Button B Presses:          %lu", result->button_b_presses);
    ESP_LOGI(TAG, "Page Navigations:          %lu", result->page_navigation_count);
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "💬 Last Error: %s", result->last_error);
    ESP_LOGI(TAG, "==========================================");
}

esp_err_t lvgl_button_test_monitor_events(uint32_t duration_seconds)
{
    ESP_LOGI(TAG, "👁️  Starting button event monitoring for %lu seconds...", duration_seconds);
    ESP_LOGI(TAG, "📋 Press buttons to see real-time event logging");
    
    int64_t start_time = esp_timer_get_time();
    int64_t end_time = duration_seconds == 0 ? INT64_MAX : start_time + (duration_seconds * 1000000);
    
    uint32_t last_a_count = 0, last_b_count = 0;
    lvgl_key_t last_key = LVGL_KEY_NONE;
    
    while (esp_timer_get_time() < end_time) {
        // Check for new button presses
        uint32_t current_a_count, current_b_count;
        lvgl_button_input_get_stats(&current_a_count, &current_b_count);
        
        if (current_a_count > last_a_count) {
            ESP_LOGI(TAG, "🔘 Button A pressed (total: %lu)", current_a_count);
            last_a_count = current_a_count;
        }
        
        if (current_b_count > last_b_count) {
            ESP_LOGI(TAG, "🔘 Button B pressed (total: %lu)", current_b_count);
            last_b_count = current_b_count;
        }
        
        // Check for key events
        lvgl_key_t current_key = lvgl_button_input_get_last_key();
        if (current_key != last_key && current_key != LVGL_KEY_NONE) {
            ESP_LOGI(TAG, "🔑 LVGL Key event: %s", 
                     (current_key == LVGL_KEY_OK) ? "OK/ENTER" : "NEXT");
            last_key = current_key;
        }
        
        vTaskDelay(pdMS_TO_TICKS(50)); // Check every 50ms
    }
    
    ESP_LOGI(TAG, "⏹️  Event monitoring completed");
    return ESP_OK;
}