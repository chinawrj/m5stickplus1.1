/*
 * Page Manager Implementation for M5StickC Plus 1.1 Multi-Page LVGL Application
 */

#include "page_manager.h"
#include "esp_log.h"
#include "system_monitor.h"
#include <string.h>

static const char *TAG = "PAGE_MANAGER";

// Forward declarations for page functions
static esp_err_t create_monitor_page(lv_obj_t *screen);
static esp_err_t create_espnow_page(lv_obj_t *screen);
static void update_monitor_page(void);
static void update_espnow_page(void);

// Global state
static lv_display_t *g_display = NULL;
static page_id_t g_current_page = PAGE_MONITOR;
static bool g_navigation_enabled = true;
static lv_timer_t *g_update_timer = NULL;

// Page registry - defines all available pages
static page_info_t g_pages[PAGE_COUNT] = {
    {
        .name = "Monitor",
        .create = create_monitor_page,
        .update = update_monitor_page,
        .cleanup = NULL,
        .screen = NULL,
        .created = false
    },
    {
        .name = "ESP-NOW", 
        .create = create_espnow_page,
        .update = update_espnow_page,
        .cleanup = NULL,
        .screen = NULL,
        .created = false
    }
};

// Page update timer callback
static void page_update_timer_cb(lv_timer_t *timer)
{
    page_manager_update_current();
}

// Monitor page creation function
static esp_err_t create_monitor_page(lv_obj_t *screen)
{
    ESP_LOGI(TAG, "Creating Monitor page...");
    
    // Set black background
    lv_obj_set_style_bg_color(screen, lv_color_black(), LV_PART_MAIN);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    
    // Title with page indicator
    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "Monitor [1/2]");
    lv_obj_set_style_text_color(title, lv_color_hex(0x00FFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_pos(title, 60, 5);
    
    // Navigation hint
    lv_obj_t *nav_hint = lv_label_create(screen);
    lv_label_set_text(nav_hint, "A:Next B:Prev");
    lv_obj_set_style_text_color(nav_hint, lv_color_hex(0x666666), LV_PART_MAIN);
    lv_obj_set_style_text_font(nav_hint, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_pos(nav_hint, 150, 120);
    
    // Battery info (simplified for multi-page)
    lv_obj_t *battery_label = lv_label_create(screen);
    lv_label_set_text(battery_label, "Battery: --%");
    lv_obj_set_style_text_color(battery_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(battery_label, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_pos(battery_label, 10, 30);
    
    // Voltage info
    lv_obj_t *voltage_label = lv_label_create(screen);
    lv_label_set_text(voltage_label, "Voltage: -.--V");
    lv_obj_set_style_text_color(voltage_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(voltage_label, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_pos(voltage_label, 10, 52);
    
    // Temperature info
    lv_obj_t *temp_label = lv_label_create(screen);
    lv_label_set_text(temp_label, "Temp: --.-°C");
    lv_obj_set_style_text_color(temp_label, lv_color_hex(0xFFFF00), LV_PART_MAIN);
    lv_obj_set_style_text_font(temp_label, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_pos(temp_label, 10, 74);
    
    // Uptime info
    lv_obj_t *uptime_label = lv_label_create(screen);
    lv_label_set_text(uptime_label, "Uptime: 00:00:00");
    lv_obj_set_style_text_color(uptime_label, lv_color_hex(0x888888), LV_PART_MAIN);
    lv_obj_set_style_text_font(uptime_label, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_pos(uptime_label, 10, 96);
    
    ESP_LOGI(TAG, "Monitor page created successfully");
    return ESP_OK;
}

// ESP-NOW page creation function  
static esp_err_t create_espnow_page(lv_obj_t *screen)
{
    ESP_LOGI(TAG, "Creating ESP-NOW page...");
    
    // Set dark blue background
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x001122), LV_PART_MAIN);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    
    // Title with page indicator
    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "ESP-NOW [2/2]");
    lv_obj_set_style_text_color(title, lv_color_hex(0x00FF88), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_pos(title, 60, 5);
    
    // Navigation hint
    lv_obj_t *nav_hint = lv_label_create(screen);
    lv_label_set_text(nav_hint, "A:Next B:Prev");
    lv_obj_set_style_text_color(nav_hint, lv_color_hex(0x666666), LV_PART_MAIN);
    lv_obj_set_style_text_font(nav_hint, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_pos(nav_hint, 150, 120);
    
    // ESP-NOW status
    lv_obj_t *status_label = lv_label_create(screen);
    lv_label_set_text(status_label, "Status: Ready");
    lv_obj_set_style_text_color(status_label, lv_color_hex(0x00FF88), LV_PART_MAIN);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_pos(status_label, 10, 30);
    
    // MAC Address display
    lv_obj_t *mac_label = lv_label_create(screen);
    lv_label_set_text(mac_label, "MAC: --:--:--:--:--:--");
    lv_obj_set_style_text_color(mac_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(mac_label, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_pos(mac_label, 10, 52);
    
    // Send counter
    lv_obj_t *send_label = lv_label_create(screen);
    lv_label_set_text(send_label, "Sent: 0 packets");
    lv_obj_set_style_text_color(send_label, lv_color_hex(0xFFAA00), LV_PART_MAIN);
    lv_obj_set_style_text_font(send_label, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_pos(send_label, 10, 74);
    
    // Receive counter
    lv_obj_t *recv_label = lv_label_create(screen);
    lv_label_set_text(recv_label, "Received: 0 packets");
    lv_obj_set_style_text_color(recv_label, lv_color_hex(0x88FF88), LV_PART_MAIN);
    lv_obj_set_style_text_font(recv_label, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_pos(recv_label, 10, 96);
    
    // Signal strength indicator
    lv_obj_t *signal_bar = lv_bar_create(screen);
    lv_obj_set_size(signal_bar, 80, 8);
    lv_obj_set_pos(signal_bar, 150, 75);
    lv_obj_set_style_bg_color(signal_bar, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_bg_color(signal_bar, lv_color_hex(0x00FF88), LV_PART_INDICATOR);
    lv_bar_set_value(signal_bar, 75, LV_ANIM_OFF);
    
    ESP_LOGI(TAG, "ESP-NOW page created successfully");
    return ESP_OK;
}

// Monitor page update function
static void update_monitor_page(void)
{
    if (!g_pages[PAGE_MONITOR].created || !g_pages[PAGE_MONITOR].screen) {
        return;
    }
    
    // Get system data
    system_data_t sys_data;
    esp_err_t ret = system_monitor_get_data(&sys_data);
    if (ret != ESP_OK || !sys_data.data_valid) {
        return;
    }
    
    // Find and update labels by searching through children
    lv_obj_t *screen = g_pages[PAGE_MONITOR].screen;
    uint32_t child_count = lv_obj_get_child_cnt(screen);
    
    for (uint32_t i = 0; i < child_count; i++) {
        lv_obj_t *child = lv_obj_get_child(screen, i);
        if (child == NULL || !lv_obj_check_type(child, &lv_label_class)) {
            continue;
        }
        
        // Get current label text to identify which label this is
        const char *current_text = lv_label_get_text(child);
        if (current_text == NULL) {
            continue;
        }
        
        // Update based on label content pattern
        if (strncmp(current_text, "Battery:", 8) == 0) {
            char buf[32];
            snprintf(buf, sizeof(buf), "Battery: %d%%", sys_data.battery_percentage);
            lv_label_set_text(child, buf);
        }
        else if (strncmp(current_text, "Voltage:", 8) == 0) {
            char buf[32];
            snprintf(buf, sizeof(buf), "Voltage: %.2fV", sys_data.battery_voltage);
            lv_label_set_text(child, buf);
        }
        else if (strncmp(current_text, "Temp:", 5) == 0) {
            char buf[32];
            snprintf(buf, sizeof(buf), "Temp: %.1f°C", sys_data.internal_temp);
            lv_label_set_text(child, buf);
        }
        else if (strncmp(current_text, "Uptime:", 7) == 0) {
            char buf[32];
            uint32_t hours = sys_data.uptime_seconds / 3600;
            uint32_t minutes = (sys_data.uptime_seconds % 3600) / 60;
            uint32_t seconds = sys_data.uptime_seconds % 60;
            snprintf(buf, sizeof(buf), "Uptime: %02"PRIu32":%02"PRIu32":%02"PRIu32, hours, minutes, seconds);
            lv_label_set_text(child, buf);
        }
    }
}

// ESP-NOW page update function
static void update_espnow_page(void)
{
    if (!g_pages[PAGE_ESPNOW].created || !g_pages[PAGE_ESPNOW].screen) {
        return;
    }
    
    // TODO: Update ESP-NOW status, counters, etc.
    // This is where you would update ESP-NOW specific data
    ESP_LOGD(TAG, "Updating ESP-NOW page");
}

// Public functions implementation
esp_err_t page_manager_init(lv_display_t *disp)
{
    ESP_LOGI(TAG, "Initializing page manager");
    
    if (disp == NULL) {
        ESP_LOGE(TAG, "Display handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    g_display = disp;
    g_current_page = PAGE_MONITOR;
    g_navigation_enabled = true;
    
    // Create first page (Monitor)
    esp_err_t ret = page_manager_goto(PAGE_MONITOR);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create initial page");
        return ret;
    }
    
    // Create update timer (1 second interval)
    g_update_timer = lv_timer_create(page_update_timer_cb, 1000, NULL);
    if (g_update_timer == NULL) {
        ESP_LOGE(TAG, "Failed to create update timer");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Page manager initialized successfully");
    return ESP_OK;
}

esp_err_t page_manager_next(void)
{
    if (!g_navigation_enabled) {
        ESP_LOGW(TAG, "Navigation is disabled");
        return ESP_ERR_INVALID_STATE;
    }
    
    page_id_t next_page = (g_current_page + 1) % PAGE_COUNT;
    return page_manager_goto(next_page);
}

esp_err_t page_manager_prev(void)
{
    if (!g_navigation_enabled) {
        ESP_LOGW(TAG, "Navigation is disabled");
        return ESP_ERR_INVALID_STATE;
    }
    
    page_id_t prev_page = (g_current_page + PAGE_COUNT - 1) % PAGE_COUNT;
    return page_manager_goto(prev_page);
}

esp_err_t page_manager_goto(page_id_t page_id)
{
    if (page_id >= PAGE_COUNT) {
        ESP_LOGE(TAG, "Invalid page ID: %d", page_id);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Defensive programming: check if page info is valid
    if (g_pages[page_id].name == NULL || g_pages[page_id].create == NULL) {
        ESP_LOGE(TAG, "Page %d is not properly initialized", page_id);
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Switching to page: %s", g_pages[page_id].name);
    
    // Mark old page as inactive (but don't delete - LVGL will handle this)
    if (g_current_page != page_id && g_current_page < PAGE_COUNT) {
        if (g_pages[g_current_page].created) {
            ESP_LOGD(TAG, "Marking current page as inactive: %s", g_pages[g_current_page].name);
            
            // Call cleanup function if provided (for application-level cleanup only)
            if (g_pages[g_current_page].cleanup != NULL) {
                g_pages[g_current_page].cleanup();
            }
            
            // IMPORTANT: Do NOT manually delete screen here! 
            // lv_scr_load() will handle the old screen deletion automatically
            g_pages[g_current_page].screen = NULL;  // Clear reference (LVGL will delete)
            g_pages[g_current_page].created = false;
        }
    }
    
    // Always create fresh page content
    ESP_LOGD(TAG, "Creating new screen for %s", g_pages[page_id].name);
    
    // Create new screen - this is a fresh LVGL screen object
    g_pages[page_id].screen = lv_obj_create(NULL);
    if (g_pages[page_id].screen == NULL) {
        ESP_LOGE(TAG, "Failed to create screen for page %d", page_id);
        return ESP_FAIL;
    }
    
    // Create page content
    esp_err_t ret = g_pages[page_id].create(g_pages[page_id].screen);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create page content for %s", g_pages[page_id].name);
        lv_obj_del(g_pages[page_id].screen);  // Safe to delete here as we just created it
        g_pages[page_id].screen = NULL;
        return ret;
    }
    
    g_pages[page_id].created = true;
    
    // CRITICAL: lv_scr_load() will automatically delete the previous active screen
    // This is LVGL's built-in screen lifecycle management
    lv_scr_load(g_pages[page_id].screen);
    g_current_page = page_id;
    
    ESP_LOGI(TAG, "Successfully switched to %s page", g_pages[page_id].name);
    return ESP_OK;
}

page_id_t page_manager_get_current(void)
{
    return g_current_page;
}

const char* page_manager_get_name(page_id_t page_id)
{
    if (page_id >= PAGE_COUNT) {
        return "Invalid";
    }
    return g_pages[page_id].name;
}

void page_manager_update_current(void)
{
    if (g_current_page < PAGE_COUNT && g_pages[g_current_page].update != NULL) {
        g_pages[g_current_page].update();
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
    ESP_LOGI(TAG, "Deinitializing page manager");
    
    // Delete update timer
    if (g_update_timer != NULL) {
        lv_timer_del(g_update_timer);
        g_update_timer = NULL;
    }
    
    // Clean up all pages - but be careful about screen deletion
    for (int i = 0; i < PAGE_COUNT; i++) {
        if (g_pages[i].cleanup != NULL) {
            g_pages[i].cleanup();
        }
        
        // Only delete screen if it's the current active screen
        // Other screens may have already been auto-deleted by LVGL
        if (g_pages[i].created && g_pages[i].screen != NULL && i == g_current_page) {
            ESP_LOGD(TAG, "Deleting active screen for page %d", i);
            lv_obj_del(g_pages[i].screen);
        }
        
        g_pages[i].screen = NULL;
        g_pages[i].created = false;
    }
    
    g_display = NULL;
    g_current_page = PAGE_MONITOR;
    g_navigation_enabled = false;
    
    ESP_LOGI(TAG, "Page manager deinitialized");
    return ESP_OK;
}