/*
 * Page Manager Implementation for M5StickC Plus 1.1 Multi-Page LVGL Application
 * Using LVGL Official Pattern: Single Screen + Clean Approach
 * Based on LVGL Benchmark Demo patterns for maximum stability
 */

#include "page_manager.h"
#include "esp_log.h"
#include "system_monitor.h"
#include <string.h>
#include <inttypes.h>

static const char *TAG = "PAGE_MANAGER";

// LVGL Official Pattern: Single Screen + Clean Approach
// Based on lvgl/demos/benchmark pattern for stability
static lv_obj_t *g_main_screen = NULL;
static page_id_t g_current_page = PAGE_MONITOR;
static bool g_navigation_enabled = true;
static lv_timer_t *g_update_timer = NULL;

// Forward declarations
static esp_err_t create_monitor_page(void);
static esp_err_t create_espnow_page(void);
static void update_monitor_page(void);
static void update_espnow_page(void);
static void load_page(page_id_t page_id);

// Page factory functions that create content directly on active screen
// Following LVGL official examples pattern
static esp_err_t create_monitor_page(void)
{
    ESP_LOGI(TAG, "Creating Monitor page...");
    
    // Get the active screen (our single main screen)
    lv_obj_t *scr = lv_screen_active();
    
    // Set black background - LVGL official pattern
    lv_obj_set_style_bg_color(scr, lv_color_black(), LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    
    // Title with page indicator
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Monitor [1/2]");
    lv_obj_set_style_text_color(title, lv_color_hex(0x00FFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_pos(title, 60, 5);
    
    // Navigation hint
    lv_obj_t *nav_hint = lv_label_create(scr);
    lv_label_set_text(nav_hint, "A:Next B:Prev");
    lv_obj_set_style_text_color(nav_hint, lv_color_hex(0x666666), LV_PART_MAIN);
    lv_obj_set_style_text_font(nav_hint, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_pos(nav_hint, 150, 120);
    
    // Battery info
    lv_obj_t *battery_label = lv_label_create(scr);
    lv_label_set_text(battery_label, "Battery: --%");
    lv_obj_set_style_text_color(battery_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(battery_label, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_pos(battery_label, 10, 30);
    
    // Voltage info
    lv_obj_t *voltage_label = lv_label_create(scr);
    lv_label_set_text(voltage_label, "Voltage: -.--V");
    lv_obj_set_style_text_color(voltage_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(voltage_label, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_pos(voltage_label, 10, 52);
    
    // Temperature info - Use white for debugging
    lv_obj_t *temp_label = lv_label_create(scr);
    lv_label_set_text(temp_label, "Temp: --.-°C");
    lv_obj_set_style_text_color(temp_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(temp_label, &lv_font_montserrat_18, LV_PART_MAIN);
    // Ensure solid color rendering
    lv_obj_set_style_text_opa(temp_label, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_pos(temp_label, 10, 74);
    
    // Uptime info - Use white for debugging
    lv_obj_t *uptime_label = lv_label_create(scr);
    lv_label_set_text(uptime_label, "Uptime: 00:00:00");
    lv_obj_set_style_text_color(uptime_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(uptime_label, &lv_font_montserrat_18, LV_PART_MAIN);
    // Disable text antialiasing to prevent pixel color bleeding
    lv_obj_set_style_text_opa(uptime_label, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_pos(uptime_label, 10, 96);
    
    ESP_LOGI(TAG, "Monitor page created successfully");
    return ESP_OK;
}

// ESP-NOW page creation function following LVGL official pattern
static esp_err_t create_espnow_page(void)
{
    ESP_LOGI(TAG, "Creating ESP-NOW page...");
    
    // Get the active screen (our single main screen)
    lv_obj_t *scr = lv_screen_active();
    
    // Set black background to match Monitor page
    lv_obj_set_style_bg_color(scr, lv_color_black(), LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    
    // Title with page indicator
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "ESP-NOW [2/2]");
    // Match Monitor page title color (cyan)
    lv_obj_set_style_text_color(title, lv_color_hex(0x00FFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_pos(title, 60, 5);
    
    // Navigation hint
    lv_obj_t *nav_hint = lv_label_create(scr);
    lv_label_set_text(nav_hint, "A:Next B:Prev");
    // Match Monitor page nav hint color (gray)
    lv_obj_set_style_text_color(nav_hint, lv_color_hex(0x666666), LV_PART_MAIN);
    lv_obj_set_style_text_font(nav_hint, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_pos(nav_hint, 150, 120);
    
    // ESP-NOW status
    lv_obj_t *status_label = lv_label_create(scr);
    lv_label_set_text(status_label, "Status: Ready");
    lv_obj_set_style_text_color(status_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_pos(status_label, 10, 30);
    
    // MAC Address display
    lv_obj_t *mac_label = lv_label_create(scr);
    lv_label_set_text(mac_label, "MAC: --:--:--:--:--:--");
    lv_obj_set_style_text_color(mac_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(mac_label, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_pos(mac_label, 10, 52);
    
    // Send counter
    lv_obj_t *send_label = lv_label_create(scr);
    lv_label_set_text(send_label, "Sent: 0 packets");
    lv_obj_set_style_text_color(send_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(send_label, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_pos(send_label, 10, 74);
    
    // Receive counter
    lv_obj_t *recv_label = lv_label_create(scr);
    lv_label_set_text(recv_label, "Received: 0 packets");
    lv_obj_set_style_text_color(recv_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(recv_label, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_pos(recv_label, 10, 96);
    
    // Signal strength indicator
    lv_obj_t *signal_bar = lv_bar_create(scr);
    lv_obj_set_size(signal_bar, 80, 8);
    lv_obj_set_pos(signal_bar, 150, 75);
    lv_obj_set_style_bg_color(signal_bar, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_bg_color(signal_bar, lv_color_white(), LV_PART_INDICATOR);
    lv_bar_set_value(signal_bar, 75, LV_ANIM_OFF);
    
    ESP_LOGI(TAG, "ESP-NOW page created successfully");
    return ESP_OK;
}

// LVGL task-safe page switching using timer (LVGL official pattern)
static page_id_t g_pending_page = PAGE_COUNT; // PAGE_COUNT means no pending switch
static lv_timer_t *g_page_switch_timer = NULL;

// LVGL timer callback for page switching (thread-safe)
static void page_switch_timer_cb(lv_timer_t *timer)
{
    if (g_pending_page >= PAGE_COUNT) {
        return; // No pending page switch
    }
    
    page_id_t target_page = g_pending_page;
    g_pending_page = PAGE_COUNT; // Clear pending flag
    
    ESP_LOGI(TAG, "Executing page switch to %d in LVGL timer context", target_page);
    
    // Get the active screen - LVGL official pattern
    lv_obj_t *scr = lv_screen_active();
    
    // Clean all content from screen - LVGL official benchmark pattern
    lv_obj_clean(scr);
    
    // Create content based on page ID
    esp_err_t ret = ESP_FAIL;
    switch (target_page) {
        case PAGE_MONITOR:
            ret = create_monitor_page();
            break;
        case PAGE_ESPNOW:
            ret = create_espnow_page();
            break;
        default:
            ESP_LOGE(TAG, "Unknown page ID: %d", target_page);
            return;
    }
    
    if (ret == ESP_OK) {
        g_current_page = target_page;
        ESP_LOGI(TAG, "Page %d loaded successfully in LVGL timer", target_page);
    } else {
        ESP_LOGE(TAG, "Failed to create page %d", target_page);
    }
}

// LVGL Official Pattern: Thread-safe page switching using timer
// Based on LVGL timer mechanism for thread safety
static void load_page(page_id_t page_id)
{
    ESP_LOGI(TAG, "Scheduling page switch to %d using LVGL timer (thread-safe)...", page_id);
    
    // Set pending page switch
    g_pending_page = page_id;
    
    // Ensure timer exists for page switching
    if (!g_page_switch_timer) {
        g_page_switch_timer = lv_timer_create(page_switch_timer_cb, 1, NULL);
        if (!g_page_switch_timer) {
            ESP_LOGE(TAG, "Failed to create page switch timer");
            g_pending_page = PAGE_COUNT;
            return;
        }
        lv_timer_set_repeat_count(g_page_switch_timer, -1); // Infinite repeat
    }
    
    // Trigger immediate execution of timer
    lv_timer_ready(g_page_switch_timer);
}

// Monitor page update function
static void update_monitor_page(void)
{
    if (g_current_page != PAGE_MONITOR) {
        return;
    }
    
    // Get the active screen
    lv_obj_t *scr = lv_screen_active();
    if (!scr) return;
    
    // Get system data
    system_data_t sys_data;
    esp_err_t ret = system_monitor_get_data(&sys_data);
    if (ret != ESP_OK || !sys_data.data_valid) {
        return;
    }
    
    // Find labels by position-based pattern matching (LVGL official approach)
    uint32_t child_count = lv_obj_get_child_count(scr);
    for (uint32_t i = 0; i < child_count; i++) {
        lv_obj_t *child = lv_obj_get_child(scr, i);
        if (child && lv_obj_check_type(child, &lv_label_class)) {
            int32_t y_pos = lv_obj_get_y(child);
            
            // Update labels based on their Y position
            if (y_pos == 30) {
                // Battery label
                char battery_text[32];
                snprintf(battery_text, sizeof(battery_text), "Battery: %d%%", (int)sys_data.battery_percentage);
                lv_label_set_text(child, battery_text);
            } else if (y_pos == 52) {
                // Voltage label
                char voltage_text[32];
                snprintf(voltage_text, sizeof(voltage_text), "Voltage: %.2fV", sys_data.battery_voltage);
                lv_label_set_text(child, voltage_text);
            } else if (y_pos == 74) {
                // Temperature label
                char temp_text[32];
                snprintf(temp_text, sizeof(temp_text), "Temp: %.1f°C", sys_data.internal_temp);
                lv_label_set_text(child, temp_text);
            } else if (y_pos == 96) {
                // Uptime label
                char uptime_text[32];
                uint32_t uptime_sec = sys_data.uptime_seconds;
                uint32_t hours = uptime_sec / 3600;
                uint32_t minutes = (uptime_sec % 3600) / 60;
                uint32_t seconds = uptime_sec % 60;
                snprintf(uptime_text, sizeof(uptime_text), "Uptime: %02" PRIu32 ":%02" PRIu32 ":%02" PRIu32, hours, minutes, seconds);
                lv_label_set_text(child, uptime_text);
            }
        }
    }
}

// ESP-NOW page update function
static void update_espnow_page(void)
{
    if (g_current_page != PAGE_ESPNOW) {
        return;
    }
    
    // ESP-NOW page update logic here
    // For now, just refresh signal bar
    lv_obj_t *scr = lv_screen_active();
    if (!scr) return;
    
    uint32_t child_count = lv_obj_get_child_count(scr);
    for (uint32_t i = 0; i < child_count; i++) {
        lv_obj_t *child = lv_obj_get_child(scr, i);
        if (child && lv_obj_check_type(child, &lv_bar_class)) {
            // Update signal strength (example)
            static int signal_value = 75;
            signal_value = (signal_value + 5) % 100;
            lv_bar_set_value(child, signal_value, LV_ANIM_OFF);
            break;
        }
    }
}

// Timer callback for page updates
static void page_update_timer_cb(lv_timer_t *timer)
{
    switch (g_current_page) {
        case PAGE_MONITOR:
            update_monitor_page();
            break;
        case PAGE_ESPNOW:
            update_espnow_page();
            break;
        default:
            break;
    }
}

// Public API functions

esp_err_t page_manager_init(lv_display_t *display)
{
    ESP_LOGI(TAG, "Initializing page manager with LVGL official pattern...");
    
    if (!display) {
        ESP_LOGE(TAG, "Invalid display parameter");
        return ESP_ERR_INVALID_ARG;
    }
    
    g_main_screen = lv_screen_active();
    if (!g_main_screen) {
        ESP_LOGE(TAG, "Failed to get active screen");
        return ESP_FAIL;
    }
    
    // Load initial page using LVGL official pattern
    load_page(PAGE_MONITOR);
    
    // Create update timer
    g_update_timer = lv_timer_create(page_update_timer_cb, 1000, NULL);
    if (!g_update_timer) {
        ESP_LOGE(TAG, "Failed to create update timer");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Page manager initialized successfully");
    return ESP_OK;
}

esp_err_t page_manager_goto(page_id_t page_id)
{
    if (!g_navigation_enabled) {
        ESP_LOGW(TAG, "Navigation disabled");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (page_id >= PAGE_COUNT) {
        ESP_LOGE(TAG, "Invalid page ID: %d", page_id);
        return ESP_ERR_INVALID_ARG;
    }
    
    if (page_id == g_current_page) {
        ESP_LOGD(TAG, "Already on page %d", page_id);
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Navigating from page %d to page %d", g_current_page, page_id);
    
    // Use LVGL official pattern: clean and rebuild
    load_page(page_id);
    
    return ESP_OK;
}

esp_err_t page_manager_next(void)
{
    page_id_t next_page = (g_current_page + 1) % PAGE_COUNT;
    return page_manager_goto(next_page);
}

esp_err_t page_manager_prev(void)
{
    page_id_t prev_page = (g_current_page - 1 + PAGE_COUNT) % PAGE_COUNT;
    return page_manager_goto(prev_page);
}

page_id_t page_manager_get_current(void)
{
    return g_current_page;
}

const char* page_manager_get_name(page_id_t page_id)
{
    switch (page_id) {
        case PAGE_MONITOR:
            return "Monitor";
        case PAGE_ESPNOW:
            return "ESP-NOW";
        default:
            return "Unknown";
    }
}

void page_manager_update_current(void)
{
    switch (g_current_page) {
        case PAGE_MONITOR:
            update_monitor_page();
            break;
        case PAGE_ESPNOW:
            update_espnow_page();
            break;
        default:
            break;
    }
}

bool page_manager_is_navigation_enabled(void)
{
    return g_navigation_enabled;
}

void page_manager_set_navigation_enabled(bool enabled)
{
    g_navigation_enabled = enabled;
    ESP_LOGI(TAG, "Navigation %s", enabled ? "enabled" : "disabled");
}

esp_err_t page_manager_deinit(void)
{
    ESP_LOGI(TAG, "Deinitializing page manager...");
    
    if (g_update_timer) {
        lv_timer_delete(g_update_timer);
        g_update_timer = NULL;
    }
    
    if (g_page_switch_timer) {
        lv_timer_delete(g_page_switch_timer);
        g_page_switch_timer = NULL;
    }
    
    // Clean the active screen - LVGL official pattern
    if (g_main_screen) {
        lv_obj_clean(g_main_screen);
    }
    
    g_main_screen = NULL;
    g_current_page = PAGE_MONITOR;
    g_navigation_enabled = true;
    g_pending_page = PAGE_COUNT;
    
    ESP_LOGI(TAG, "Page manager deinitialized");
    return ESP_OK;
}