#include "page_manager_monitor.h"
#include "core/lv_obj_style_gen.h"
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

// Monitor page UI objects - Complete design with dual panel layout
static lv_obj_t *g_monitor_uptime_label = NULL;
static lv_obj_t *g_monitor_memory_label = NULL;
static lv_obj_t *g_monitor_battery_voltage_label = NULL;  // Large voltage display
static lv_obj_t *g_monitor_voltage_unit_label = NULL;     // V unit
static lv_obj_t *g_monitor_usb_panel = NULL;              // Left blue panel for USB voltage
static lv_obj_t *g_monitor_usb_title_label = NULL;        // "USB V" title (18pt)
static lv_obj_t *g_monitor_usb_value_label = NULL;        // voltage value (24pt)
static lv_obj_t *g_monitor_current_panel = NULL;          // Right blue panel for battery current
static lv_obj_t *g_monitor_current_title_label = NULL;    // "CHR I" title (18pt)
static lv_obj_t *g_monitor_current_value_label = NULL;    // current value (24pt)
static lv_obj_t *g_monitor_temp_panel = NULL;             // Green background panel for temperature
static lv_obj_t *g_monitor_temp_label = NULL;             // Temperature value (24pt)
static lv_obj_t *g_monitor_power_source_label = NULL;     // "Power Source:" text
static lv_obj_t *g_monitor_power_status_panel = NULL;     // Background panel for USB/Battery
static lv_obj_t *g_monitor_power_status_label = NULL;     // "USB" or "BATTERY" text

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
static esp_err_t monitor_page_init(void);
static esp_err_t monitor_page_create(void);
static esp_err_t monitor_page_update(void);
static esp_err_t monitor_page_destroy(void);
static bool monitor_page_is_data_updated(void);
static bool monitor_page_handle_key_event(uint32_t key);

// Page controller interface implementation
static const page_controller_t monitor_controller = {
    .init = monitor_page_init,
    .create = monitor_page_create,
    .update = monitor_page_update, 
    .destroy = monitor_page_destroy,
    .is_data_updated = monitor_page_is_data_updated,
    .handle_key_event = monitor_page_handle_key_event,
    .name = "Monitor",
    .page_id = PAGE_MONITOR
};const page_controller_t* get_monitor_page_controller(void)
{
    return &monitor_controller;
}

static esp_err_t monitor_page_init(void)
{
    ESP_LOGI(TAG, "Initializing Monitor page module");
    
    // Reset UI object pointers
    g_monitor_uptime_label = NULL;
    g_monitor_memory_label = NULL;
    g_monitor_battery_voltage_label = NULL;
    g_monitor_voltage_unit_label = NULL;
    g_monitor_usb_panel = NULL;
    g_monitor_usb_title_label = NULL;
    g_monitor_usb_value_label = NULL;
    g_monitor_current_panel = NULL;
    g_monitor_current_title_label = NULL;
    g_monitor_current_value_label = NULL;
    g_monitor_temp_panel = NULL;
    g_monitor_temp_label = NULL;
    g_monitor_power_source_label = NULL;
    g_monitor_power_status_panel = NULL;
    g_monitor_power_status_label = NULL;
    
    ESP_LOGI(TAG, "Monitor page module initialized");
    return ESP_OK;
}

static esp_err_t monitor_page_create(void)
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

static esp_err_t monitor_page_update(void)
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

static esp_err_t monitor_page_destroy(void)
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

static bool monitor_page_is_data_updated(void)
{
    // Monitor page uses system_monitor data update status
    // The system_monitor module handles its own thread safety
    return system_monitor_is_data_updated();
}

// Internal UI creation function - Fusion Design (Reference Style + Complete Information)
static esp_err_t create_monitor_page_ui(void)
{
    lv_obj_t *scr = lv_scr_act();
    if (scr == NULL) {
        ESP_LOGE(TAG, "Failed to get active screen");
        return ESP_FAIL;
    }
    
    // Clear screen
    lv_obj_clean(scr);
    
    // Set black background
    lv_obj_set_style_bg_color(scr, lv_color_black(), LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    
    // Title: "BATTERY MONITOR" - descriptive title
    lv_obj_t *title = lv_label_create(scr);
    if (title != NULL) {
        lv_label_set_text(title, "BATTERY MONITOR");
        lv_obj_set_style_text_color(title, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_text_font(title, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_obj_set_pos(title, 10, 5);
    }
    
    // Main battery voltage - LARGE display like reference
    g_monitor_battery_voltage_label = lv_label_create(scr);
    if (g_monitor_battery_voltage_label != NULL) {
        lv_label_set_text(g_monitor_battery_voltage_label, "4.12");
        lv_obj_set_style_text_color(g_monitor_battery_voltage_label, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_text_font(g_monitor_battery_voltage_label, &lv_font_montserrat_48, LV_PART_MAIN);
        lv_obj_set_pos(g_monitor_battery_voltage_label, 10, 25);
    }
    
    // Voltage unit "V" - positioned correctly to the right (修复位置)
    g_monitor_voltage_unit_label = lv_label_create(scr);
    if (g_monitor_voltage_unit_label != NULL) {
        lv_label_set_text(g_monitor_voltage_unit_label, "V");
        lv_obj_set_style_text_color(g_monitor_voltage_unit_label, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_text_font(g_monitor_voltage_unit_label, &lv_font_montserrat_18, LV_PART_MAIN);
        lv_obj_set_pos(g_monitor_voltage_unit_label, 115, 55);  // 进一步右移，避免与数字重叠
    }
    
    // Left panel - USB voltage (blue background)
    g_monitor_usb_panel = lv_obj_create(scr);
    if (g_monitor_usb_panel != NULL) {
        lv_obj_set_size(g_monitor_usb_panel, 63, 50);  // Increased height for larger font
        lv_obj_set_pos(g_monitor_usb_panel, 2, 85);    // Left side
        lv_obj_set_style_bg_color(g_monitor_usb_panel, lv_color_make(0x20, 0x40, 0x80), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(g_monitor_usb_panel, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(g_monitor_usb_panel, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(g_monitor_usb_panel, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(g_monitor_usb_panel, 0, LV_PART_MAIN);
        
        // USB title label (18pt font)
        g_monitor_usb_title_label = lv_label_create(g_monitor_usb_panel);
        if (g_monitor_usb_title_label != NULL) {
            lv_label_set_text(g_monitor_usb_title_label, "USB V");
            lv_obj_set_style_text_color(g_monitor_usb_title_label, lv_color_white(), LV_PART_MAIN);
            lv_obj_set_style_text_font(g_monitor_usb_title_label, &lv_font_montserrat_14, LV_PART_MAIN);
            lv_obj_set_style_text_align(g_monitor_usb_title_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
            lv_obj_set_pos(g_monitor_usb_title_label, 0, 2);  // Top of panel
            lv_obj_set_size(g_monitor_usb_title_label, 63, 20);
        }
        
        // USB voltage value (24pt font)
        g_monitor_usb_value_label = lv_label_create(g_monitor_usb_panel);
        if (g_monitor_usb_value_label != NULL) {
            lv_label_set_text(g_monitor_usb_value_label, "-.--");
            lv_obj_set_style_text_color(g_monitor_usb_value_label, lv_color_white(), LV_PART_MAIN);
            lv_obj_set_style_text_font(g_monitor_usb_value_label, &lv_font_montserrat_24, LV_PART_MAIN);
            lv_obj_set_style_text_align(g_monitor_usb_value_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
            lv_obj_set_pos(g_monitor_usb_value_label, 0, 22);  // Below title
            lv_obj_set_size(g_monitor_usb_value_label, 63, 28);
        }
    }
    
    // Right panel - Battery current (blue background)
    g_monitor_current_panel = lv_obj_create(scr);
    if (g_monitor_current_panel != NULL) {
        lv_obj_set_size(g_monitor_current_panel, 63, 50);  // Increased height for larger font
        lv_obj_set_pos(g_monitor_current_panel, 70, 85);   // Right side
        lv_obj_set_style_bg_color(g_monitor_current_panel, lv_color_make(0x20, 0x40, 0x80), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(g_monitor_current_panel, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(g_monitor_current_panel, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(g_monitor_current_panel, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(g_monitor_current_panel, 0, LV_PART_MAIN);
        
        // Battery current title label (18pt font)
        g_monitor_current_title_label = lv_label_create(g_monitor_current_panel);
        if (g_monitor_current_title_label != NULL) {
            lv_label_set_text(g_monitor_current_title_label, "CHG I");
            lv_obj_set_style_text_color(g_monitor_current_title_label, lv_color_white(), LV_PART_MAIN);
            lv_obj_set_style_text_font(g_monitor_current_title_label, &lv_font_montserrat_14, LV_PART_MAIN);
            lv_obj_set_style_text_align(g_monitor_current_title_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
            lv_obj_set_pos(g_monitor_current_title_label, 0, 2);  // Top of panel
            lv_obj_set_size(g_monitor_current_title_label, 63, 20);
        }
        
        // Battery current value (24pt font)
        g_monitor_current_value_label = lv_label_create(g_monitor_current_panel);
        if (g_monitor_current_value_label != NULL) {
            lv_label_set_text(g_monitor_current_value_label, "---");
            lv_obj_set_style_text_color(g_monitor_current_value_label, lv_color_white(), LV_PART_MAIN);
            lv_obj_set_style_text_font(g_monitor_current_value_label, &lv_font_montserrat_24, LV_PART_MAIN);
            lv_obj_set_style_text_align(g_monitor_current_value_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
            lv_obj_set_pos(g_monitor_current_value_label, 0, 22);  // Below title
            lv_obj_set_size(g_monitor_current_value_label, 63, 28);
        }
    }
    
    // Temperature panel - full width green background
    g_monitor_temp_panel = lv_obj_create(scr);
    if (g_monitor_temp_panel != NULL) {
        lv_obj_set_size(g_monitor_temp_panel, 135, 30);  // Full width panel
        lv_obj_set_pos(g_monitor_temp_panel, 0, 145);    // Positioned after dual panels
        lv_obj_set_style_bg_color(g_monitor_temp_panel, lv_color_make(0x20, 0x80, 0x20), LV_PART_MAIN);  // Green background
        lv_obj_set_style_bg_opa(g_monitor_temp_panel, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(g_monitor_temp_panel, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(g_monitor_temp_panel, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(g_monitor_temp_panel, 0, LV_PART_MAIN);
        
        // Temperature value label (24pt white font on green background)
        g_monitor_temp_label = lv_label_create(g_monitor_temp_panel);
        if (g_monitor_temp_label != NULL) {
            lv_label_set_text(g_monitor_temp_label, "--.-°C");
            lv_obj_set_style_text_color(g_monitor_temp_label, lv_color_white(), LV_PART_MAIN);
            lv_obj_set_style_text_font(g_monitor_temp_label, &lv_font_montserrat_24, LV_PART_MAIN);
            lv_obj_set_style_text_align(g_monitor_temp_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
            lv_obj_center(g_monitor_temp_label);
        }
    }
    
    // Power Source label (first line) - 下移以适应48pt字体
    g_monitor_power_source_label = lv_label_create(scr);
    if (g_monitor_power_source_label != NULL) {
        lv_label_set_text(g_monitor_power_source_label, "Power Source:");
        lv_obj_set_style_text_color(g_monitor_power_source_label, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_text_font(g_monitor_power_source_label, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_obj_set_pos(g_monitor_power_source_label, 10, 180);  // 从162进一步下移
    }
    
    // Power status panel with full-width background color (second line) - 下移以适应48pt字体
    g_monitor_power_status_panel = lv_obj_create(scr);
    if (g_monitor_power_status_panel != NULL) {
        // 使面板占据整行宽度 (屏幕宽度135像素，左右各留5像素边距)
        lv_obj_set_size(g_monitor_power_status_panel, 135, 24);
        lv_obj_set_pos(g_monitor_power_status_panel, 5, 197);  // 从179进一步下移
        lv_obj_set_style_bg_color(g_monitor_power_status_panel, lv_color_hex(0x00AA00), LV_PART_MAIN);  // Green for USB
        lv_obj_set_style_border_width(g_monitor_power_status_panel, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(g_monitor_power_status_panel, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(g_monitor_power_status_panel, 0, LV_PART_MAIN);
        
        // Power status text inside the panel - 完全居中
        g_monitor_power_status_label = lv_label_create(g_monitor_power_status_panel);
        if (g_monitor_power_status_label != NULL) {
            lv_label_set_text(g_monitor_power_status_label, "USB");
            lv_obj_set_style_text_color(g_monitor_power_status_label, lv_color_white(), LV_PART_MAIN);
            lv_obj_set_style_text_font(g_monitor_power_status_label, &lv_font_montserrat_18, LV_PART_MAIN);
            lv_obj_set_style_text_align(g_monitor_power_status_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
            lv_obj_center(g_monitor_power_status_label);  // 水平和垂直居中
        }
    }
    
    // Uptime at bottom-left - 调整到屏幕内
    g_monitor_uptime_label = lv_label_create(scr);
    if (g_monitor_uptime_label != NULL) {
        char uptime_text[16];
        format_uptime_string(uptime_text, sizeof(uptime_text));
        lv_label_set_text(g_monitor_uptime_label, uptime_text);
        lv_obj_set_style_text_color(g_monitor_uptime_label, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_text_font(g_monitor_uptime_label, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_obj_set_pos(g_monitor_uptime_label, 5, 225);  // 调整到屏幕内（240-15=225）
    }
    
    // Memory at bottom-right - 右对齐并调整到屏幕内
    g_monitor_memory_label = lv_label_create(scr);
    if (g_monitor_memory_label != NULL) {
        char memory_text[16];
        format_free_memory_string(memory_text, sizeof(memory_text));
        lv_label_set_text(g_monitor_memory_label, memory_text);
        lv_obj_set_style_text_color(g_monitor_memory_label, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_text_font(g_monitor_memory_label, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_obj_set_style_text_align(g_monitor_memory_label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
        lv_obj_set_pos(g_monitor_memory_label, 80, 225);  // 右对齐显示，x位置调整为80
        lv_obj_set_size(g_monitor_memory_label, 50, 15);  // 设置宽度以便右对齐
    }
    
    ESP_LOGI(TAG, "Monitor page UI created successfully (reference style)");
    return ESP_OK;
}

// Internal UI update function - Complete with all original functionality
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
    
    // Update main battery voltage - large display like reference
    if (g_monitor_battery_voltage_label != NULL) {
        char battery_voltage[16];
        snprintf(battery_voltage, sizeof(battery_voltage), "%.2f", sys_data.battery_voltage);
        lv_label_set_text(g_monitor_battery_voltage_label, battery_voltage);
    }
    
    // Update left panel - USB voltage value only
    if (g_monitor_usb_value_label != NULL) {
        char usb_value[16];
        float vbus_voltage = sys_data.vbus_voltage;
        snprintf(usb_value, sizeof(usb_value), "%.2f", vbus_voltage);
        lv_label_set_text(g_monitor_usb_value_label, usb_value);
    }
    
    // Update right panel - Battery current title and value
    if (g_monitor_current_title_label != NULL && g_monitor_current_value_label != NULL) {
        char current_value[16];
        float ibat = 0.0f;
        const char* title = "CHR I";  // Default to charge
        
        if (sys_data.is_charging) {
            ibat = sys_data.charge_current;    // mA (positive for charge)
            title = "CHG I";  // Charging
        } else {
            ibat = sys_data.discharge_current; // mA (positive for discharge)
            title = "DIS I";  // Discharging
        }
        
        // Update title label
        lv_label_set_text(g_monitor_current_title_label, title);
        
        // Update value label (just the number)
        snprintf(current_value, sizeof(current_value), "%.0f", ibat);
        lv_label_set_text(g_monitor_current_value_label, current_value);
    }
    
    // Update temperature display - only show temperature value (24pt)
    if (g_monitor_temp_label != NULL) {
        char temp_text[32];
        snprintf(temp_text, sizeof(temp_text), "%.1f°C", sys_data.internal_temp);
        lv_label_set_text(g_monitor_temp_label, temp_text);
    }
    
    // Update power status with dynamic background color (全宽度面板)
    if (g_monitor_power_status_label != NULL && g_monitor_power_status_panel != NULL) {
        if (sys_data.is_usb_connected) {
            lv_label_set_text(g_monitor_power_status_label, "USB");
            lv_obj_set_style_bg_color(g_monitor_power_status_panel, lv_color_hex(0x00AA00), LV_PART_MAIN);  // Green for USB
        } else {
            lv_label_set_text(g_monitor_power_status_label, "BATTERY");
            lv_obj_set_style_bg_color(g_monitor_power_status_panel, lv_color_hex(0xFF6600), LV_PART_MAIN);  // Orange for Battery
            // 面板保持全宽度，无需调整大小
        }
    }
    
    // Update memory display (small, grey text)
    if (g_monitor_memory_label != NULL) {
        char memory_text[16];
        format_free_memory_string(memory_text, sizeof(memory_text));
        lv_label_set_text(g_monitor_memory_label, memory_text);
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
    g_monitor_battery_voltage_label = NULL;
    g_monitor_voltage_unit_label = NULL;
    g_monitor_usb_panel = NULL;
    g_monitor_usb_title_label = NULL;
    g_monitor_usb_value_label = NULL;
    g_monitor_current_panel = NULL;
    g_monitor_current_title_label = NULL;
    g_monitor_current_value_label = NULL;
    g_monitor_temp_panel = NULL;
    g_monitor_temp_label = NULL;
    g_monitor_power_source_label = NULL;
    g_monitor_power_status_panel = NULL;
    g_monitor_power_status_label = NULL;
    
    return ESP_OK;
}

// Page-specific key event handler
static bool monitor_page_handle_key_event(uint32_t key)
{
    ESP_LOGI(TAG, "📊 Monitor page received key: %lu", key);
    
    switch (key) {
        case LV_KEY_ENTER:
            ESP_LOGI(TAG, "⚡ Monitor page ENTER - Toggle power info display");
            // Example: Toggle between detailed and simple power display
            // This is just a demonstration - actual implementation would modify UI
            return true;  // We handled this key
            
        case LV_KEY_UP:
            ESP_LOGI(TAG, "⬆️ Monitor page UP - Increase brightness/contrast");
            // Example: Adjust display brightness
            return true;  // We handled this key
            
        case LV_KEY_DOWN:
            ESP_LOGI(TAG, "⬇️ Monitor page DOWN - Decrease brightness/contrast");
            // Example: Adjust display brightness
            return true;  // We handled this key
            
        default:
            ESP_LOGD(TAG, "🔹 Monitor page - unhandled key: %lu", key);
            return false;  // Let global handler process this key
    }
}