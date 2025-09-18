/**
 * @file lvgl_integration_demo.c
 * @brief Integration Demo Implementation for LVGL Button System
 */

#include "lvgl_integration_demo.h"
#include "lvgl_button_input.h"
#include "page_manager_lvgl.h"
#include "lvgl_button_test.h"
#include "lvgl_init.h"
#include "button_nav.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "LVGL_INTEGRATION_DEMO";

esp_err_t lvgl_integration_demo_run_complete(void)
{
    ESP_LOGI(TAG, "🚀 Starting Complete LVGL Button Integration Demo");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "This demo will:");
    ESP_LOGI(TAG, "1. Initialize LVGL and display system");
    ESP_LOGI(TAG, "2. Set up LVGL button input device (A=OK, B=NEXT)");
    ESP_LOGI(TAG, "3. Initialize LVGL-integrated page manager");
    ESP_LOGI(TAG, "4. Run comprehensive tests");
    ESP_LOGI(TAG, "5. Start interactive navigation demo");
    ESP_LOGI(TAG, "");
    
    // Step 1: Initialize LVGL with display
    ESP_LOGI(TAG, "📱 Step 1: Initializing LVGL display system...");
    lv_display_t *display = NULL;
    esp_err_t ret = lvgl_init_for_page_manager(&display);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to initialize LVGL display: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "✅ LVGL display system initialized");
    
    // Step 2: Initialize LVGL button input device
    ESP_LOGI(TAG, "🔘 Step 2: Initializing LVGL button input device...");
    ret = lvgl_button_input_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to initialize LVGL button input: %s", esp_err_to_name(ret));
        return ret;
    }
    
    lv_indev_t *input_device = lvgl_button_input_get_device();
    if (input_device == NULL) {
        ESP_LOGE(TAG, "❌ Failed to get LVGL input device handle");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "✅ LVGL button input device initialized (A=OK, B=NEXT)");
    
    // Step 3: Initialize LVGL-integrated page manager
    ESP_LOGI(TAG, "📄 Step 3: Initializing LVGL page manager...");
    ret = page_manager_lvgl_init(display, input_device);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to initialize LVGL page manager: %s", esp_err_to_name(ret));
        lvgl_button_input_deinit();
        return ret;
    }
    ESP_LOGI(TAG, "✅ LVGL page manager initialized with key navigation");
    
    // Step 4: Run comprehensive tests
    ESP_LOGI(TAG, "🧪 Step 4: Running comprehensive integration tests...");
    lvgl_button_test_result_t test_result;
    ret = lvgl_button_test_run_comprehensive(&test_result);
    
    // Print detailed test results
    lvgl_button_test_print_results(&test_result);
    
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "⚠️  Some tests failed, but continuing with demo...");
    } else {
        ESP_LOGI(TAG, "✅ All integration tests passed!");
    }
    
    // Step 5: Start interactive navigation demo
    ESP_LOGI(TAG, "🎮 Step 5: Starting interactive navigation demo...");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "📋 Interactive Demo Instructions:");
    ESP_LOGI(TAG, "   • Press Button A (GPIO37) for OK/ENTER actions");
    ESP_LOGI(TAG, "   • Press Button B (GPIO39) to navigate between pages");
    ESP_LOGI(TAG, "   • Watch the logs for real-time event feedback");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "⏱️  Demo will run for 30 seconds...");
    
    // Monitor events for 30 seconds
    ret = lvgl_button_test_monitor_events(30);
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "🎉 Complete LVGL Button Integration Demo finished!");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "📊 Final Statistics:");
    uint32_t button_a_count, button_b_count;
    lvgl_button_input_get_stats(&button_a_count, &button_b_count);
    ESP_LOGI(TAG, "   • Button A presses: %lu", button_a_count);
    ESP_LOGI(TAG, "   • Button B presses: %lu", button_b_count);
    
    uint32_t key_nav_count, manual_nav_count;
    page_manager_lvgl_get_nav_stats(&key_nav_count, &manual_nav_count);
    ESP_LOGI(TAG, "   • Key-based navigations: %lu", key_nav_count);
    ESP_LOGI(TAG, "   • Manual navigations: %lu", manual_nav_count);
    
    return ESP_OK;
}

esp_err_t lvgl_integration_demo_run_quick_test(void)
{
    ESP_LOGI(TAG, "⚡ Running Quick LVGL Button Integration Test");
    
    // Initialize LVGL button input
    esp_err_t ret = lvgl_button_input_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Quick test failed: LVGL button input init failed");
        return ret;
    }
    
    // Run quick test
    lvgl_button_test_result_t test_result;
    ret = lvgl_button_test_run_quick(&test_result);
    
    // Print results
    lvgl_button_test_print_results(&test_result);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ Quick test PASSED - LVGL button integration is working!");
    } else {
        ESP_LOGE(TAG, "❌ Quick test FAILED - check test results above");
    }
    
    return ret;
}

esp_err_t lvgl_integration_demo_init_button_system(lv_display_t *display)
{
    ESP_LOGI(TAG, "🔧 Initializing LVGL button system for your application...");
    
    if (display == NULL) {
        ESP_LOGE(TAG, "❌ Invalid display parameter");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Initialize LVGL button input
    esp_err_t ret = lvgl_button_input_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to initialize LVGL button input: %s", esp_err_to_name(ret));
        return ret;
    }
    
    lv_indev_t *input_device = lvgl_button_input_get_device();
    if (input_device == NULL) {
        ESP_LOGE(TAG, "❌ Failed to get input device handle");
        lvgl_button_input_deinit();
        return ESP_FAIL;
    }
    
    // Initialize LVGL page manager (optional)
    ret = page_manager_lvgl_init(display, input_device);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "⚠️  Page manager init failed, continuing without it: %s", esp_err_to_name(ret));
        // Don't fail here, as the application might want to handle pages differently
    }
    
    ESP_LOGI(TAG, "✅ LVGL button system initialized successfully");
    ESP_LOGI(TAG, "📋 Button mapping: A (GPIO37) = OK/ENTER, B (GPIO39) = NEXT");
    
    return ESP_OK;
}

esp_err_t lvgl_integration_demo_migrate_from_button_nav(lv_display_t *display)
{
    ESP_LOGI(TAG, "🔄 Migrating from old button_nav system to LVGL button system...");
    
    // Step 1: Deinitialize old button_nav system
    ESP_LOGI(TAG, "🧹 Step 1: Cleaning up old button_nav system...");
    if (button_nav_is_enabled()) {
        ESP_LOGI(TAG, "   • Disabling old button navigation");
        button_nav_set_enabled(false);
        button_nav_deinit();
    }
    ESP_LOGI(TAG, "✅ Old button_nav system cleaned up");
    
    // Step 2: Initialize new LVGL button system
    ESP_LOGI(TAG, "🔧 Step 2: Initializing new LVGL button system...");
    esp_err_t ret = lvgl_integration_demo_init_button_system(display);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to initialize new LVGL button system");
        return ret;
    }
    ESP_LOGI(TAG, "✅ New LVGL button system initialized");
    
    // Step 3: Verify migration with quick test
    ESP_LOGI(TAG, "🧪 Step 3: Verifying migration with quick test...");
    lvgl_button_test_result_t test_result;
    ret = lvgl_button_test_run_quick(&test_result);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ Migration SUCCESSFUL!");
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "📋 Migration Changes:");
        ESP_LOGI(TAG, "   • Button A (GPIO37): Now generates LV_KEY_ENTER events");
        ESP_LOGI(TAG, "   • Button B (GPIO39): Now generates LV_KEY_NEXT events");
        ESP_LOGI(TAG, "   • Page navigation: Now handled through LVGL key events");
        ESP_LOGI(TAG, "   • Thread safety: Improved with LVGL integration");
        ESP_LOGI(TAG, "   • Event handling: Now uses LVGL group system");
    } else {
        ESP_LOGE(TAG, "❌ Migration FAILED - check test results");
        lvgl_button_test_print_results(&test_result);
    }
    
    return ret;
}