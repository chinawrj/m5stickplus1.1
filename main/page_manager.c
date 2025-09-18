/*
 * Page Manager Implementation for M5StickC Plus 1.1 Multi-Page LVGL Application
 * Using LVGL Official Pattern: Single Screen + Clean Approach
 * Based on LVGL Benchmark Demo patterns for maximum stability
 */

#include "page_manager.h"
#include "esp_log.h"
#include "system_monitor.h"
#include "axp192.h"
#include "esp_wifi.h"
#include "esp_mac.h"
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

// Helper function to get WiFi MAC address as formatted string
static esp_err_t get_wifi_mac_string(char *mac_str, size_t mac_str_size)
{
    uint8_t mac_addr[6];
    esp_err_t ret = esp_read_mac(mac_addr, ESP_MAC_WIFI_STA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read WiFi MAC address: %s", esp_err_to_name(ret));
        snprintf(mac_str, mac_str_size, "MAC: Error");
        return ret;
    }
    
    // Format as MAC: XX:XX:XX:XX:XX:XX
    snprintf(mac_str, mac_str_size, "MAC: %02X:%02X:%02X:%02X:%02X:%02X", 
             mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    
    return ESP_OK;
}

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

    // Uptime at bottom-left, display real current time instead of placeholder
    lv_obj_t *uptime_label = lv_label_create(scr);
    
    // Get current system data to display real uptime immediately
    system_data_t sys_data;
    char uptime_text[16] = "00:00:00"; // fallback if data not available
    esp_err_t ret = system_monitor_get_data(&sys_data);
    if (ret == ESP_OK && sys_data.data_valid) {
        uint32_t uptime_sec = sys_data.uptime_seconds;
        uint32_t hours = uptime_sec / 3600;
        uint32_t minutes = (uptime_sec % 3600) / 60;
        uint32_t seconds = uptime_sec % 60;
        snprintf(uptime_text, sizeof(uptime_text), "%02" PRIu32 ":%02" PRIu32 ":%02" PRIu32, hours, minutes, seconds);
    }
    
    lv_label_set_text(uptime_label, uptime_text);
    lv_obj_set_style_text_color(uptime_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(uptime_label, &lv_font_montserrat_14, LV_PART_MAIN);
    // Ensure solid color rendering
    lv_obj_set_style_text_opa(uptime_label, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_pos(uptime_label, 5, 120);
    
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
    
    // MAC Address display (moved up to replace status)
    lv_obj_t *mac_label = lv_label_create(scr);
    
    // Get real MAC address
    char mac_str[32];
    get_wifi_mac_string(mac_str, sizeof(mac_str));
    lv_label_set_text(mac_label, mac_str);
    
    lv_obj_set_style_text_color(mac_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(mac_label, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_pos(mac_label, 10, 30);
    
    // Send counter (moved up)
    lv_obj_t *send_label = lv_label_create(scr);
    lv_label_set_text(send_label, "Sent: 0 packets");
    lv_obj_set_style_text_color(send_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(send_label, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_pos(send_label, 10, 52);
    
    // Receive counter (moved up)
    lv_obj_t *recv_label = lv_label_create(scr);
    lv_label_set_text(recv_label, "Received: 0 packets");
    lv_obj_set_style_text_color(recv_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(recv_label, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_pos(recv_label, 10, 74);
    
    // Signal strength indicator
    lv_obj_t *signal_bar = lv_bar_create(scr);
    lv_obj_set_size(signal_bar, 80, 8);
    lv_obj_set_pos(signal_bar, 150, 75);
    lv_obj_set_style_bg_color(signal_bar, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_bg_color(signal_bar, lv_color_white(), LV_PART_INDICATOR);
    lv_bar_set_value(signal_bar, 75, LV_ANIM_OFF);
    
    // Uptime at bottom-left, display real current time instead of placeholder
    lv_obj_t *uptime_label = lv_label_create(scr);
    
    // Get current system data to display real uptime immediately
    system_data_t sys_data;
    char uptime_text[16] = "00:00:00"; // fallback if data not available
    esp_err_t ret = system_monitor_get_data(&sys_data);
    if (ret == ESP_OK && sys_data.data_valid) {
        uint32_t uptime_sec = sys_data.uptime_seconds;
        uint32_t hours = uptime_sec / 3600;
        uint32_t minutes = (uptime_sec % 3600) / 60;
        uint32_t seconds = uptime_sec % 60;
        snprintf(uptime_text, sizeof(uptime_text), "%02" PRIu32 ":%02" PRIu32 ":%02" PRIu32, hours, minutes, seconds);
    }
    
    lv_label_set_text(uptime_label, uptime_text);
    lv_obj_set_style_text_color(uptime_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(uptime_label, &lv_font_montserrat_14, LV_PART_MAIN);
    // Ensure solid color rendering
    lv_obj_set_style_text_opa(uptime_label, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_pos(uptime_label, 5, 120);
    
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
        // No pending page switch - delete timer to prevent unnecessary runs
        if (g_page_switch_timer) {
            lv_timer_delete(g_page_switch_timer);
            g_page_switch_timer = NULL;
            ESP_LOGD(TAG, "Page switch timer cleaned up (no pending page)");
        }
        return;
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
            // Clean up timer on error
            if (g_page_switch_timer) {
                lv_timer_delete(g_page_switch_timer);
                g_page_switch_timer = NULL;
            }
            return;
    }
    
    if (ret == ESP_OK) {
        g_current_page = target_page;
        ESP_LOGI(TAG, "Page %d loaded successfully in LVGL timer", target_page);
    } else {
        ESP_LOGE(TAG, "Failed to create page %d", target_page);
    }
    
    // Clean up timer after successful page switch
    if (g_page_switch_timer) {
        lv_timer_delete(g_page_switch_timer);
        g_page_switch_timer = NULL;
        ESP_LOGD(TAG, "Page switch timer cleaned up after successful switch");
    }
}

// LVGL Official Pattern: Thread-safe page switching using timer
// Based on LVGL timer mechanism for thread safety
static void load_page(page_id_t page_id)
{
    ESP_LOGI(TAG, "Scheduling page switch to %d using LVGL timer (thread-safe)...", page_id);
    
    // Set pending page switch
    g_pending_page = page_id;
    
    // Create a one-shot timer for page switching if one doesn't exist
    if (!g_page_switch_timer) {
        g_page_switch_timer = lv_timer_create(page_switch_timer_cb, 1, NULL);
        if (!g_page_switch_timer) {
            ESP_LOGE(TAG, "Failed to create page switch timer");
            g_pending_page = PAGE_COUNT;
            return;
        }
        // Set to run only once, not infinite repeat
        lv_timer_set_repeat_count(g_page_switch_timer, 1);
        ESP_LOGD(TAG, "Created one-shot page switch timer");
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
            int32_t x_pos = lv_obj_get_x(child);
            
            // Update labels based on their Y position
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
                snprintf(battery_text, sizeof(battery_text), "Bat: %.2fV %+.0fmA%s", 
                         sys_data.battery_voltage, ibat, charge_indicator);
                lv_label_set_text(child, battery_text);
            } else if (y_pos == 52) {
                // USB: voltage and current combined
                char usb_text[64];
                float vbus_voltage = sys_data.vbus_voltage;
                // Get USB current using safe_read_float helper
                float vbus_current = 0.0f;
                esp_err_t ret = axp192_get_vbus_current(&vbus_current);
                if (ret != ESP_OK) {
                    vbus_current = 0.0f;
                }
                snprintf(usb_text, sizeof(usb_text), "USB: %.2fV %.0fmA", 
                         vbus_voltage, vbus_current);
                lv_label_set_text(child, usb_text);
            } else if (y_pos == 74) {
                // Temperature label
                char temp_text[32];
                snprintf(temp_text, sizeof(temp_text), "Temp: %.1f°C", sys_data.internal_temp);
                lv_label_set_text(child, temp_text);
            } else if (y_pos == 96) {
                // Power Source: USB connection status
                char status_text[64];
                if (sys_data.is_usb_connected) {
                    snprintf(status_text, sizeof(status_text), "Power Source: USB");
                } else {
                    snprintf(status_text, sizeof(status_text), "Power Source: Battery");
                }
                lv_label_set_text(child, status_text);
            } else if (y_pos == 120 && x_pos < 60) {
                // Uptime label (hh:mm:ss only)
                char uptime_text[16];
                uint32_t uptime_sec = sys_data.uptime_seconds;
                uint32_t hours = uptime_sec / 3600;
                uint32_t minutes = (uptime_sec % 3600) / 60;
                uint32_t seconds = uptime_sec % 60;
                snprintf(uptime_text, sizeof(uptime_text), "%02" PRIu32 ":%02" PRIu32 ":%02" PRIu32, hours, minutes, seconds);
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
    
    lv_obj_t *scr = lv_screen_active();
    if (!scr) return;
    
    // Get system data for uptime
    system_data_t sys_data;
    esp_err_t ret = system_monitor_get_data(&sys_data);
    if (ret != ESP_OK || !sys_data.data_valid) {
        return;
    }
    
    uint32_t child_count = lv_obj_get_child_count(scr);
    bool mac_label_found = false;
    
    for (uint32_t i = 0; i < child_count; i++) {
        lv_obj_t *child = lv_obj_get_child(scr, i);
        if (!child) continue;
        
        // Update signal strength bar
        if (lv_obj_check_type(child, &lv_bar_class)) {
            static int signal_value = 75;
            signal_value = (signal_value + 5) % 100;
            lv_bar_set_value(child, signal_value, LV_ANIM_OFF);
        }
        
        // Update labels based on position and content
        if (lv_obj_check_type(child, &lv_label_class)) {
            int32_t y_pos = lv_obj_get_y(child);
            int32_t x_pos = lv_obj_get_x(child);
            const char *text = lv_label_get_text(child);
            
            // Update MAC address label
            if (!mac_label_found && text && strncmp(text, "MAC:", 4) == 0) {
                char mac_str[32];
                get_wifi_mac_string(mac_str, sizeof(mac_str));
                lv_label_set_text(child, mac_str);
                mac_label_found = true;
            }
            // Update uptime label (same position as Monitor page)
            else if (y_pos == 120 && x_pos < 60) {
                char uptime_text[16];
                uint32_t uptime_sec = sys_data.uptime_seconds;
                uint32_t hours = uptime_sec / 3600;
                uint32_t minutes = (uptime_sec % 3600) / 60;
                uint32_t seconds = uptime_sec % 60;
                snprintf(uptime_text, sizeof(uptime_text), "%02" PRIu32 ":%02" PRIu32 ":%02" PRIu32, hours, minutes, seconds);
                lv_label_set_text(child, uptime_text);
            }
        }
    }
}

// Timer callback for page updates - optimized with data update flag
static void page_update_timer_cb(lv_timer_t *timer)
{
    // Skip updates during page switches (when pending page is set)
    if (g_pending_page < PAGE_COUNT) {
        ESP_LOGD(TAG, "Skipping page update during page switch");
        return;
    }
    
    // Check if system data has been updated since last check
    if (!system_monitor_is_data_updated()) {
        ESP_LOGD(TAG, "No data update, skipping UI refresh");
        return;
    }
    
    ESP_LOGD(TAG, "Data updated, refreshing UI");
    
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
    
    // Clear the data updated flag after consuming the data
    system_monitor_clear_updated_flag();
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
    
    // Create update timer with 500ms interval for responsive updates
    g_update_timer = lv_timer_create(page_update_timer_cb, 500, NULL);
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