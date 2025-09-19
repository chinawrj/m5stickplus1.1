#include "page_manager_monitor.h"
#include "page_manager.h"
#include "system_monitor.h"
#include "axp192.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include <inttypes.h>

static const char *TAG = "MONITOR_PAGE";

// Monitor page UI objects - stored as static variables for updates
static lv_obj_t *g_monitor_uptime_label = NULL;
static lv_obj_t *g_monitor_memory_label = NULL;
static lv_obj_t *g_monitor_tasks_label = NULL;
static lv_obj_t *g_monitor_status_label = NULL;

// Helper function to format uptime as HH:MM:SS string
static void format_uptime_string(char *buffer, size_t buffer_size)
{
    // Get system uptime directly in microseconds, then convert to seconds
    int64_t uptime_us = esp_timer_get_time();
    uint32_t uptime_sec = (uint32_t)(uptime_us / 1000000);
    
    uint32_t hours = uptime_sec / 3600;
    uint32_t minutes = (uptime_sec % 3600) / 60;
    uint32_t seconds = uptime_sec % 60;
    snprintf(buffer, buffer_size, "%02" PRIu32 ":%02" PRIu32 ":%02" PRIu32, hours, minutes, seconds);
}

// Helper function to format free memory as "XXX KB" string  
static void format_free_memory_string(char *buffer, size_t buffer_size)
{
    // Get current free heap memory directly
    uint32_t free_heap_bytes = esp_get_free_heap_size();
    uint32_t free_heap_kb = free_heap_bytes / 1024;  // Convert to KB
    
    snprintf(buffer, buffer_size, "%"PRIu32" KB", free_heap_kb);
}

// Internal functions
static esp_err_t create_monitor_page_ui(void);
static esp_err_t update_monitor_page_ui(void);
static esp_err_t destroy_monitor_page_ui(void);

// Page controller interface implementation
static const page_controller_t monitor_controller = {
    .init = monitor_page_init,
    .create = monitor_page_create,
    .update = monitor_page_update, 
    .destroy = monitor_page_destroy,
    .is_data_updated = monitor_page_is_data_updated,
    .name = "Monitor",
    .page_id = PAGE_MONITOR
};const page_controller_t* get_monitor_page_controller(void)
{
    return &monitor_controller;
}

esp_err_t monitor_page_init(void)
{
    ESP_LOGI(TAG, "Initializing Monitor page module");
    
    // Reset UI object pointers
    g_monitor_uptime_label = NULL;
    g_monitor_memory_label = NULL;
    g_monitor_tasks_label = NULL;
    g_monitor_status_label = NULL;
    
    ESP_LOGI(TAG, "Monitor page module initialized");
    return ESP_OK;
}

esp_err_t monitor_page_create(void)
{
    ESP_LOGI(TAG, "Creating Monitor page UI...");
    
    esp_err_t ret = create_monitor_page_ui();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create Monitor page UI: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Monitor page created successfully");
    return ESP_OK;
}

esp_err_t monitor_page_update(void)
{
    ESP_LOGD(TAG, "Updating Monitor page data...");
    
    esp_err_t ret = update_monitor_page_ui();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to update Monitor page: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGD(TAG, "Monitor page updated successfully");
    return ESP_OK;
}

esp_err_t monitor_page_destroy(void)
{
    ESP_LOGI(TAG, "Destroying Monitor page...");
    
    esp_err_t ret = destroy_monitor_page_ui();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to destroy Monitor page: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Monitor page destroyed successfully");
    return ESP_OK;
}

bool monitor_page_is_data_updated(void)
{
    // Monitor page uses system_monitor data update status
    // The system_monitor module handles its own thread safety
    return system_monitor_is_data_updated();
}

// Internal UI creation function
static esp_err_t create_monitor_page_ui(void)
{
    lv_obj_t *scr = lv_scr_act();
    
    // Clear screen
    lv_obj_clean(scr);
    
    // Set black background - LVGL official pattern
    lv_obj_set_style_bg_color(scr, lv_color_black(), LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    
    // Title with page indicator
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Monitor [1/2]");
    lv_obj_set_style_text_color(title, lv_color_hex(0x00FFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_pos(title, 60, 5);
    
    // Battery: voltage and current on one line
    lv_obj_t *battery_label = lv_label_create(scr);
    lv_label_set_text(battery_label, "Bat: -.--V ---mA");
    lv_obj_set_style_text_color(battery_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(battery_label, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_pos(battery_label, 10, 30);
    
    // USB: voltage and current on one line
    lv_obj_t *usb_label = lv_label_create(scr);
    lv_label_set_text(usb_label, "USB: -.--V ---mA");
    lv_obj_set_style_text_color(usb_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(usb_label, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_pos(usb_label, 10, 52);
    
    // Temperature info
    lv_obj_t *temp_label = lv_label_create(scr);
    lv_label_set_text(temp_label, "Temp: --.-°C");
    lv_obj_set_style_text_color(temp_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(temp_label, &lv_font_montserrat_18, LV_PART_MAIN);
    // Ensure solid color rendering
    lv_obj_set_style_text_opa(temp_label, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_pos(temp_label, 10, 74);
    
    // USB connection status
    lv_obj_t *status_label = lv_label_create(scr);
    lv_label_set_text(status_label, "Power Source: USB");
    lv_obj_set_style_text_color(status_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_pos(status_label, 10, 96);
    
    // Store references to labels that need updates
    g_monitor_status_label = status_label;  // For USB status
    g_monitor_tasks_label = temp_label;     // For temperature
    // Note: battery_label and usb_label will be found by position during updates
    
    // Uptime at bottom-left
    g_monitor_uptime_label = lv_label_create(scr);
    char uptime_text[16];
    format_uptime_string(uptime_text, sizeof(uptime_text));
    lv_label_set_text(g_monitor_uptime_label, uptime_text);
    lv_obj_set_style_text_color(g_monitor_uptime_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(g_monitor_uptime_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_opa(g_monitor_uptime_label, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_pos(g_monitor_uptime_label, 5, 120);
    
    // Memory at bottom-right  
    g_monitor_memory_label = lv_label_create(scr);
    char memory_text[16];
    format_free_memory_string(memory_text, sizeof(memory_text));
    lv_label_set_text(g_monitor_memory_label, memory_text);
    lv_obj_set_style_text_color(g_monitor_memory_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(g_monitor_memory_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_opa(g_monitor_memory_label, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_pos(g_monitor_memory_label, 170, 120);  // Right-aligned
    
    return ESP_OK;
}

// Internal UI update function
static esp_err_t update_monitor_page_ui(void)
{
    // Update uptime display
    if (g_monitor_uptime_label != NULL) {
        char uptime_text[16];
        format_uptime_string(uptime_text, sizeof(uptime_text));
        lv_label_set_text(g_monitor_uptime_label, uptime_text);
    }
    
    // Update memory display
    if (g_monitor_memory_label != NULL) {
        char memory_text[16];
        format_free_memory_string(memory_text, sizeof(memory_text));
        lv_label_set_text(g_monitor_memory_label, memory_text);
    }
    
    // Get system data
    system_data_t sys_data;
    esp_err_t ret = system_monitor_get_data(&sys_data);
    if (ret != ESP_OK || !sys_data.data_valid) {
        return ESP_OK;  // Don't fail if system data unavailable
    }
    
    // Find labels by position and update them (following original logic)
    lv_obj_t *scr = lv_scr_act();
    if (!scr) return ESP_OK;
    
    uint32_t child_count = lv_obj_get_child_count(scr);
    for (uint32_t i = 0; i < child_count; i++) {
        lv_obj_t *child = lv_obj_get_child(scr, i);
        if (child && lv_obj_check_type(child, &lv_label_class)) {
            int32_t y_pos = lv_obj_get_y(child);
            
            // Update labels based on their Y position (original approach)
            if (y_pos == 30) {
                // Battery: voltage and current combined with charging status
                char battery_text[64];
                float ibat = 0.0f;
                if (sys_data.is_charging) {
                    ibat = sys_data.charge_current;    // mA
                } else {
                    ibat = -sys_data.discharge_current; // mA (negative for discharge)
                }
                const char* charge_indicator = sys_data.is_charging ? " CHG" : "";
                snprintf(battery_text, sizeof(battery_text), "Bat: %.2fV %3.0fmA%s", 
                         sys_data.battery_voltage, ibat, charge_indicator);
                lv_label_set_text(child, battery_text);
            }
            else if (y_pos == 52) {
                // USB: voltage and current
                char usb_text[64];
                float vbus_voltage = sys_data.vbus_voltage;
                float vbus_current = 0.0f;
                esp_err_t ret = axp192_get_vbus_current(&vbus_current);
                if (ret == ESP_OK) {
                    snprintf(usb_text, sizeof(usb_text), "USB: %.2fV %3.0fmA", vbus_voltage, vbus_current);
                } else {
                    snprintf(usb_text, sizeof(usb_text), "USB: %.2fV ---mA", vbus_voltage);
                }
                lv_label_set_text(child, usb_text);
            }
            else if (y_pos == 74) {
                // Temperature
                char temp_text[32];
                snprintf(temp_text, sizeof(temp_text), "Temp: %.1f°C", sys_data.internal_temp);
                lv_label_set_text(child, temp_text);
            }
            else if (y_pos == 96) {
                // USB connection status
                const char* power_source = sys_data.is_usb_connected ? "Power Source: USB" : "Power Source: Battery";
                lv_label_set_text(child, power_source);
            }
        }
    }
    
    return ESP_OK;
}

// Internal UI destruction function  
static esp_err_t destroy_monitor_page_ui(void)
{
    // Clear screen will automatically clean up all child objects
    lv_obj_t *scr = lv_scr_act();
    lv_obj_clean(scr);
    
    // Reset object pointers
    g_monitor_uptime_label = NULL;
    g_monitor_memory_label = NULL;
    g_monitor_tasks_label = NULL;
    g_monitor_status_label = NULL;
    
    return ESP_OK;
}