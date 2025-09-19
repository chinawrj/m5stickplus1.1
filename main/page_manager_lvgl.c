/**
 * @file page_manager_lvgl.c
 * @brief LVGL-integrated Page Manager Implementation for M5StickC Plus 1.1
 * 
 * This implementation extends the base page manager with LVGL key event handling,
 * enabling navigation through button presses converted to LVGL key events.
 */

#include "page_manager_lvgl.h"
#include "page_manager.h"
#include "lvgl_button_input.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "PAGE_MGR_LVGL";

// LVGL integration state
static bool g_lvgl_page_manager_initialized = false;
static bool g_key_events_enabled = true;
static lv_group_t *g_nav_group = NULL;
static lv_indev_t *g_input_device = NULL;

/**
 * @brief Screen-level key event handler (兜底处理器)
 * 
 * This function handles key events that bubble up to the screen level.
 * It serves as the fallback handler for unhandled key events.
 * 
 * @param e LVGL event containing key information
 */
static void screen_key_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    
    // 增强日志 - 显示所有事件
    ESP_LOGI(TAG, "🖥️ Screen event received: code=%d", code);
    
    if (code != LV_EVENT_KEY) {
        ESP_LOGI(TAG, "🖥️ Screen non-key event: %d (ignored)", code);
        return;
    }
    
    if (!g_key_events_enabled) {
        ESP_LOGI(TAG, "🖥️ Screen key events disabled, ignoring");
        return;
    }
    
    // LVGL 9.x: Get key from event parameter
    uint32_t key = lv_event_get_key(e);
    ESP_LOGI(TAG, "🏠 Screen-level key event: %lu", key);
    
    // First, check if current page wants to handle this key
    bool page_handled = page_manager_handle_key_event(key);
    if (page_handled) {
        ESP_LOGI(TAG, "✅ Key %lu handled by current page", key);
        return;  // Page handled it, we're done
    }
    
    // Page didn't handle it, process global navigation keys
    if (key == LV_KEY_RIGHT) {
        ESP_LOGI(TAG, "🚀 Screen RIGHT key - navigating to next page");
        
        esp_err_t ret = page_manager_lvgl_next();
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "✅ Successfully navigated to page %d (%s)", 
                     page_manager_get_current(), 
                     page_manager_get_name(page_manager_get_current()));
        } else {
            ESP_LOGW(TAG, "❌ Failed to navigate to next page: %s", esp_err_to_name(ret));
        }
    } else if (key == LV_KEY_ENTER) {
        ESP_LOGI(TAG, "⭐ Screen ENTER key - page-specific action (not implemented globally)");
        // This could be used for global actions if needed
    } else {
        ESP_LOGI(TAG, "🔹 Screen unhandled key: %lu", key);
    }
}

// Public API implementation

esp_err_t page_manager_lvgl_init(lv_display_t *display, lv_indev_t *indev)
{
    ESP_LOGI(TAG, "Initializing LVGL-integrated page manager...");
    
    if (g_lvgl_page_manager_initialized) {
        ESP_LOGW(TAG, "LVGL page manager already initialized");
        return ESP_OK;
    }
    
    if (!display || !indev) {
        ESP_LOGE(TAG, "Invalid display or input device parameter");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Initialize base page manager first
    esp_err_t ret = page_manager_init(display);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize base page manager: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Store input device reference
    g_input_device = indev;
    
    // Create LVGL group for navigation
    g_nav_group = lv_group_create();
    if (g_nav_group == NULL) {
        ESP_LOGE(TAG, "Failed to create LVGL navigation group");
        page_manager_deinit();
        return ESP_ERR_NO_MEM;
    }
    
    // Associate input device with group
    lv_indev_set_group(indev, g_nav_group);
    
    // CRITICAL: Add screen object to group as fallback for unhandled key events
    lv_obj_t *screen = lv_screen_active();
    if (!screen) {
        ESP_LOGE(TAG, "No active screen found");
        lv_group_delete(g_nav_group);
        g_nav_group = NULL;
        page_manager_deinit();
        return ESP_FAIL;
    }
    
    // Add screen-level key event handler (兜底处理器)
    lv_obj_add_event_cb(screen, screen_key_event_cb, LV_EVENT_KEY, NULL);
    
    // Add screen to group so it can receive key events when no other object has focus
    lv_group_add_obj(g_nav_group, screen);
    
    ESP_LOGI(TAG, "Screen added to group with key event handler - ready for navigation");
    
    // Initialize state
    g_key_events_enabled = true;
    
    g_lvgl_page_manager_initialized = true;
    
    ESP_LOGI(TAG, "LVGL-integrated page manager initialized successfully");
    ESP_LOGI(TAG, "Key navigation: LV_KEY_RIGHT->Next Page, LV_KEY_ENTER->Page Action");
    
    return ESP_OK;
}

void page_manager_lvgl_set_key_enabled(bool enabled)
{
    g_key_events_enabled = enabled;
    ESP_LOGI(TAG, "LVGL key event handling %s", enabled ? "enabled" : "disabled");
    
    // Key events are now handled at screen level, no need to manage focus
}

bool page_manager_lvgl_is_key_enabled(void)
{
    return g_key_events_enabled;
}

lv_group_t* page_manager_lvgl_get_group(void)
{
    return g_nav_group;
}

esp_err_t page_manager_lvgl_next(void)
{
    ESP_LOGI(TAG, "Manual navigation to next page");
    return page_manager_next();
}

esp_err_t page_manager_lvgl_prev(void)
{
    ESP_LOGI(TAG, "Manual navigation to previous page");
    return page_manager_prev();
}

/**
 * @brief Manual key event handler as fallback (DEPRECATED)
 * 
 * This function is no longer needed since we use screen-level key handling.
 * Kept for backward compatibility but does nothing.
 */
void page_manager_manual_key_event(uint32_t key)
{
    // No longer needed with proper screen-level key handling
    ESP_LOGD(TAG, "Manual key event deprecated: %lu", key);
}

esp_err_t page_manager_lvgl_deinit(void)
{
    ESP_LOGI(TAG, "Deinitializing LVGL-integrated page manager...");
    
    if (!g_lvgl_page_manager_initialized) {
        return ESP_OK;
    }
    
    g_key_events_enabled = false;
    
    // Clean up LVGL group
    if (g_nav_group) {
        lv_group_delete(g_nav_group);
        g_nav_group = NULL;
    }
    
    // Deinitialize base page manager
    esp_err_t ret = page_manager_deinit();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Base page manager deinitialization failed: %s", esp_err_to_name(ret));
    }
    
    g_input_device = NULL;
    g_lvgl_page_manager_initialized = false;
    
    ESP_LOGI(TAG, "LVGL-integrated page manager deinitialized");
    return ESP_OK;
}