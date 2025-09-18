/*
 * DEPRECATED: Old LVGL Demo UI for M5StickC Plus 1.1 
 * This file contains the old single-screen demo UI implementation.
 * It has been replaced by the new page manager system (page_manager_lvgl.c).
 * Kept for reference only.
 * 
 * Shows battery, temperature, and system information - LANDSCAPE MODE (240x135)
 */

#if 0  // DEPRECATED - Old system, use page_manager_lvgl.c instead

#include "lvgl.h"
#include "esp_log.h"
#include "system_monitor.h"
#include <stdio.h>

static const char *TAG = "LVGL_DEMO";

// Global references to UI elements for updates
static lv_obj_t *g_battery_label = NULL;
static lv_obj_t *g_voltage_label = NULL;
static lv_obj_t *g_current_label = NULL;
static lv_obj_t *g_temp_label = NULL;
static lv_obj_t *g_usb_label = NULL;
static lv_obj_t *g_uptime_label = NULL;
static lv_obj_t *g_memory_label = NULL;
static lv_obj_t *g_battery_bar = NULL;

// Timer for updating display
static lv_timer_t *g_update_timer = NULL;

// Update callback function
static void update_display_data(lv_timer_t *timer)
{
    system_data_t sys_data;
    esp_err_t ret = system_monitor_get_data(&sys_data);
    
    if (ret != ESP_OK || !sys_data.data_valid) {
        ESP_LOGW(TAG, "Failed to get system data or data invalid");
        return;
    }
    
    char buf[64];
    
    // Update battery percentage and bar
    if (g_battery_label && g_battery_bar) {
        snprintf(buf, sizeof(buf), "Battery: %d%%", sys_data.battery_percentage);
        lv_label_set_text(g_battery_label, buf);
        lv_bar_set_value(g_battery_bar, sys_data.battery_percentage, LV_ANIM_ON);
    }
    
    // Update voltage
    if (g_voltage_label) {
        snprintf(buf, sizeof(buf), "Voltage: %.2fV", sys_data.battery_voltage);
        lv_label_set_text(g_voltage_label, buf);
    }
    
    // Update current (charging or discharging)
    if (g_current_label) {
        if (sys_data.is_charging) {
            snprintf(buf, sizeof(buf), "Charging: %.1fmA", sys_data.charge_current);
        } else {
            snprintf(buf, sizeof(buf), "Current: %.1fmA", sys_data.discharge_current);
        }
        lv_label_set_text(g_current_label, buf);
    }
    
    // Update temperature
    if (g_temp_label) {
        snprintf(buf, sizeof(buf), "Temp: %.1f°C", sys_data.internal_temp);
        lv_label_set_text(g_temp_label, buf);
    }
    
    // Update USB status
    if (g_usb_label) {
        snprintf(buf, sizeof(buf), "USB: %s", sys_data.is_usb_connected ? "Connected" : "Disconnected");
        lv_label_set_text(g_usb_label, buf);
        
        // Change color based on status
        lv_color_t color = sys_data.is_usb_connected ? lv_color_hex(0x00FF00) : lv_color_hex(0xFF0000);
        lv_obj_set_style_text_color(g_usb_label, color, LV_PART_MAIN);
    }
    
    // Update uptime with seconds precision
    if (g_uptime_label) {
        uint32_t hours = sys_data.uptime_seconds / 3600;
        uint32_t minutes = (sys_data.uptime_seconds % 3600) / 60;
        uint32_t seconds = sys_data.uptime_seconds % 60;
        snprintf(buf, sizeof(buf), "Uptime: %02"PRIu32":%02"PRIu32":%02"PRIu32, hours, minutes, seconds);
        lv_label_set_text(g_uptime_label, buf);
    }
    
    // Update memory info
    if (g_memory_label) {
        snprintf(buf, sizeof(buf), "RAM: %"PRIu32"KB free", sys_data.free_heap / 1024);
        lv_label_set_text(g_memory_label, buf);
    }
}

/**
 * System monitor display with real-time data - landscape mode
 */
void m5stick_lvgl_demo_ui(lv_display_t *disp)
{
    ESP_LOGI(TAG, "Creating system monitor UI (240x135)...");
    
    // Get the active screen
    lv_obj_t *scr = lv_display_get_screen_active(disp);
    
    // Set black background and clear all default styles
    lv_obj_set_style_bg_color(scr, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_width(scr, 0, LV_PART_MAIN);
    lv_obj_set_style_outline_width(scr, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(scr, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(scr, 0, LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    
    // Title at top
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "M5StickC Plus Monitor");
    lv_obj_set_style_text_color(title, lv_color_hex(0x00FFFF), LV_PART_MAIN);  // Cyan
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_pos(title, 40, 5);
    
    // Battery percentage label
    g_battery_label = lv_label_create(scr);
    lv_label_set_text(g_battery_label, "Battery: --%");
    lv_obj_set_style_text_color(g_battery_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(g_battery_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_pos(g_battery_label, 10, 25);
    
    // Battery progress bar
    g_battery_bar = lv_bar_create(scr);
    lv_obj_set_size(g_battery_bar, 100, 8);
    lv_obj_set_pos(g_battery_bar, 130, 28);
    lv_obj_set_style_bg_color(g_battery_bar, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_bg_color(g_battery_bar, lv_color_hex(0x00FF00), LV_PART_INDICATOR);
    lv_obj_set_style_border_width(g_battery_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_outline_width(g_battery_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(g_battery_bar, 0, LV_PART_MAIN);
    lv_bar_set_value(g_battery_bar, 0, LV_ANIM_OFF);
    
    // Voltage label
    g_voltage_label = lv_label_create(scr);
    lv_label_set_text(g_voltage_label, "Voltage: -.--V");
    lv_obj_set_style_text_color(g_voltage_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(g_voltage_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_pos(g_voltage_label, 10, 45);
    
    // Current label  
    g_current_label = lv_label_create(scr);
    lv_label_set_text(g_current_label, "Current: ---mA");
    lv_obj_set_style_text_color(g_current_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(g_current_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_pos(g_current_label, 10, 65);
    
    // Temperature label
    g_temp_label = lv_label_create(scr);
    lv_label_set_text(g_temp_label, "Temp: --.-°C");
    lv_obj_set_style_text_color(g_temp_label, lv_color_hex(0xFFFF00), LV_PART_MAIN);  // Yellow
    lv_obj_set_style_text_font(g_temp_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_pos(g_temp_label, 130, 45);
    
    // USB status label
    g_usb_label = lv_label_create(scr);
    lv_label_set_text(g_usb_label, "USB: Unknown");
    lv_obj_set_style_text_color(g_usb_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(g_usb_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_pos(g_usb_label, 130, 65);
    
    // Uptime label
    g_uptime_label = lv_label_create(scr);
    lv_label_set_text(g_uptime_label, "Uptime: 00:00");
    lv_obj_set_style_text_color(g_uptime_label, lv_color_hex(0x888888), LV_PART_MAIN);  // Gray
    lv_obj_set_style_text_font(g_uptime_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_pos(g_uptime_label, 10, 85);
    
    // Memory label
    g_memory_label = lv_label_create(scr);
    lv_label_set_text(g_memory_label, "RAM: ---KB free");
    lv_obj_set_style_text_color(g_memory_label, lv_color_hex(0x888888), LV_PART_MAIN);  // Gray
    lv_obj_set_style_text_font(g_memory_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_pos(g_memory_label, 10, 105);
    
    // Status indicator in bottom right
    lv_obj_t *status_dot = lv_obj_create(scr);
    lv_obj_set_size(status_dot, 8, 8);
    lv_obj_set_pos(status_dot, 225, 120);
    lv_obj_set_style_bg_color(status_dot, lv_color_hex(0x00FF00), LV_PART_MAIN);  // Green = running
    lv_obj_set_style_border_width(status_dot, 0, LV_PART_MAIN);
    lv_obj_set_style_outline_width(status_dot, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(status_dot, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(status_dot, 4, LV_PART_MAIN);  // Make it circular
    
    // Create update timer (update every 1 second for real-time data)
    g_update_timer = lv_timer_create(update_display_data, 1000, NULL);
    
    // Initial data update
    update_display_data(NULL);
    
    ESP_LOGI(TAG, "System monitor UI created successfully");
}

#endif  // DEPRECATED - Old system