#include "page_manager_espnow.h"
#include "core/lv_group.h"
#include "core/lv_obj_pos.h"
#include "misc/lv_color.h"
#include "page_manager.h"
#include "espnow_manager.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_mac.h"
#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include <stdatomic.h>
#include <inttypes.h>

static const char *TAG = "ESPNOW_PAGE";

// ESP-NOW subpage identifiers for future expansion
typedef enum {
    ESPNOW_SUBPAGE_OVERVIEW = 0,    // Overview/statistics subpage (current default)
    ESPNOW_SUBPAGE_NODE_DETAIL,     // Node detail subpage (future)
    ESPNOW_SUBPAGE_COUNT            // Total number of subpages
} espnow_subpage_id_t;

// ESP-NOW page UI objects structure for better organization and debugging
typedef struct {
    lv_obj_t *uptime_label;     // System uptime display
    lv_obj_t *memory_label;     // Free memory display
    lv_obj_t *status_label;     // ESP-NOW status display (currently unused)
    lv_obj_t *sent_label;       // Sent packets counter (36pt)
    lv_obj_t *sent_tx_label;    // "Tx" label (12pt) for sent packets
    lv_obj_t *recv_label;       // Received packets counter (36pt)
    lv_obj_t *recv_rx_label;    // "Rx" label (12pt) for received packets
    lv_obj_t *title_label;      // Page title
    lv_obj_t *mac_label;        // MAC address display
    // Online nodes triple panel layout
    lv_obj_t *online_panel;     // Left panel: Online nodes count
    lv_obj_t *used_panel;       // Center panel: Used nodes count  
    lv_obj_t *total_panel;      // Right panel: Total nodes count
} espnow_overview_t;

// ESP-NOW node detail page UI objects structure (TLV data display)
typedef struct {
    lv_obj_t *title_label;      // Page title
    
    // Row 1: Device ID and Network Information (2 items)
    lv_obj_t *network_row_label; // "ID: M5NanoC6-123 | RSSI: -XX"
    
    // Row 2: Power Information - Main Display (48pt, like battery voltage)
    lv_obj_t *power_label;        // "520.0" (48pt, main display)
    
    // Row 3: Electrical Measurements - Dual Panel Layout
    lv_obj_t *electrical_row_label; // Stores voltage panel reference for updates
    lv_obj_t *current_panel;        // Current panel reference for updates
    
    // Row 4: System Information with Firmware (3 items)  
    lv_obj_t *system_row_label;   // "UP: 12:34:56 | 45KB | FW: v1.2.3"
    
    // Row 5: Version Information (not used anymore - FW moved to system_row)
    lv_obj_t *version_row_label;  // Unused, kept for structure compatibility
    
    // Bottom status labels (like overview page)
    lv_obj_t *uptime_label;     // System uptime display (bottom-left)
    lv_obj_t *memory_label;     // Free memory display (bottom-right)
    
    // Additional information labels
    lv_obj_t *compile_label;
} espnow_node_detail_t;

// Node detail data structure (TLV format simulation)
typedef struct {
    // Network data
    uint8_t mac_address[6];          // TLV_TYPE_MAC_ADDRESS
    int rssi;                        // Signal strength (not in TLV, but ESP-NOW specific)
    bool wifi_connected;             // STATUS_FLAG_WIFI_CONNECTED
    bool espnow_active;              // STATUS_FLAG_ESP_NOW_ACTIVE
    
    // Electrical measurements (from TLV format)
    float ac_voltage;                // TLV_TYPE_AC_VOLTAGE (volts)
    float ac_current;                // TLV_TYPE_AC_CURRENT (amperes) 
    float ac_power;                  // TLV_TYPE_AC_POWER (watts)
    float power_factor;              // TLV_TYPE_AC_POWER_FACTOR
    
    // System information
    uint32_t uptime_seconds;         // TLV_TYPE_UPTIME
    float temperature;               // TLV_TYPE_TEMPERATURE (celsius)
    uint32_t free_memory_kb;         // Memory in KB
    uint16_t error_code;             // TLV_TYPE_ERROR_CODE
    
    // Version information
    char device_id[17];              // TLV_TYPE_DEVICE_ID (max 16 + null)
    char firmware_version[17];       // TLV_TYPE_FIRMWARE_VER (max 16 + null)
    char compile_time[33];           // TLV_TYPE_COMPILE_TIME (max 32 + null)
} espnow_node_data_t;

/*=============================================================================
 * 🏗️  PAGE MANAGEMENT VARIABLES (Core subpage state and control)
 *=============================================================================*/
static espnow_subpage_id_t g_current_subpage = ESPNOW_SUBPAGE_OVERVIEW;
static atomic_bool g_espnow_data_updated = ATOMIC_VAR_INIT(false);

/*=============================================================================
 * 📊  OVERVIEW PAGE VARIABLES (ESP-NOW statistics and system monitoring)
 *=============================================================================*/
// Overview page UI objects
static espnow_overview_t g_overview_ui = {0};

// Overview page data
static espnow_stats_t g_espnow_stats = {0};

/*=============================================================================
 * 🔍  NODE DETAIL PAGE VARIABLES (Individual device monitoring)
 *=============================================================================*/
// Node detail page UI objects
static espnow_node_detail_t g_node_detail_ui = {0};

// Node detail page data and state
static int g_current_device_index = 0;
static espnow_node_data_t g_current_node_data = {
    .mac_address = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    .rssi = 0,
    .wifi_connected = false,
    .espnow_active = false,
    .ac_voltage = 0.0f,
    .ac_current = 0.0f,
    .ac_power = 0.0f,
    .power_factor = 0.0f,
    .uptime_seconds = 0,
    .temperature = 0.0f,
    .free_memory_kb = 0,
    .error_code = 0,
    .device_id = "-",
    .firmware_version = "-",
    .compile_time = "-"
};

/*=============================================================================
 * 🔄  SHARED SYSTEM VARIABLES (Change detection and system state)
 *=============================================================================*/
// Previous state tracking for both pages
static uint32_t g_prev_uptime_sec = 0;
static uint32_t g_prev_free_heap_kb = 0;
static uint32_t g_prev_packets_sent = 0;
static uint32_t g_prev_packets_received = 0;

/*=============================================================================
 * 🔧  HELPER FUNCTIONS (Utility functions shared between pages)
 *=============================================================================*/

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
    snprintf(mac_str, mac_str_size, "%02X:%02X:%02X:%02X:%02X:%02X", 
             mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    
    return ESP_OK;
}

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

/*=============================================================================
 * 📋  FUNCTION DECLARATIONS (Forward declarations organized by functionality)
 *=============================================================================*/

// Overview page functions
static esp_err_t espnow_overview_create(void);
static esp_err_t espnow_overview_update(void);
static esp_err_t espnow_overview_destroy(void);

// Node detail page functions  
static esp_err_t espnow_node_detail_create(void);
static esp_err_t espnow_node_detail_update(void);
static esp_err_t espnow_node_detail_destroy(void);

// Internal helper function for updating node detail data and UI
static esp_err_t espnow_node_detail_refresh_data_and_ui(void);

// Main page interface functions
static esp_err_t espnow_page_init(void);
static esp_err_t espnow_page_create(void);
static esp_err_t espnow_page_update(void);
static esp_err_t espnow_page_destroy(void);
static bool espnow_page_is_data_updated(void);
static bool espnow_page_handle_key_event(uint32_t key);

// Subpage management functions
static esp_err_t espnow_subpage_switch(espnow_subpage_id_t subpage_id);
static esp_err_t espnow_subpage_create_current(void);
static esp_err_t espnow_subpage_update_current(void);
static esp_err_t espnow_subpage_destroy_current(void);

/*=============================================================================
 * 🎮  PAGE CONTROLLER INTERFACE (Public API implementation)
 *=============================================================================*/

// Page controller interface implementation
static const page_controller_t espnow_controller = {
    .init = espnow_page_init,
    .create = espnow_page_create,
    .update = espnow_page_update,
    .destroy = espnow_page_destroy,
    .is_data_updated = espnow_page_is_data_updated,
    .handle_key_event = espnow_page_handle_key_event,
    .name = "ESP-NOW",
    .page_id = PAGE_ESPNOW
};

const page_controller_t* get_espnow_page_controller(void)
{
    return &espnow_controller;
}

/*=============================================================================
 * 🔄  MAIN PAGE INTERFACE IMPLEMENTATION (Page lifecycle management)
 *=============================================================================*/

static esp_err_t espnow_page_init(void)
{
    ESP_LOGI(TAG, "Initializing ESP-NOW page module");
    
    // Reset UI object pointers in overview structure
    memset(&g_overview_ui, 0, sizeof(espnow_overview_t));
    
    // Reset UI object pointers in node detail structure
    memset(&g_node_detail_ui, 0, sizeof(espnow_node_detail_t));
    
    // Initialize subpage state
    g_current_subpage = ESPNOW_SUBPAGE_OVERVIEW;
    
    // Reset data state - atomic variable doesn't need explicit initialization
    atomic_store(&g_espnow_data_updated, false);
    
    // Initialize previous state tracking variables
    int64_t uptime_us = esp_timer_get_time();
    g_prev_uptime_sec = (uint32_t)(uptime_us / 1000000);
    g_prev_free_heap_kb = esp_get_free_heap_size() / 1024;
    
    // Initialize previous ESP-NOW statistics
    espnow_stats_t current_stats = {0};
    if (espnow_manager_get_stats(&current_stats) == ESP_OK) {
        g_prev_packets_sent = current_stats.packets_sent;
        g_prev_packets_received = current_stats.packets_received;
    } else {
        g_prev_packets_sent = 0;
        g_prev_packets_received = 0;
    }
    
    ESP_LOGI(TAG, "ESP-NOW page module initialized");
    return ESP_OK;
}

static esp_err_t espnow_page_create(void)
{
    ESP_LOGI(TAG, "Creating ESP-NOW page UI...");
    
    esp_err_t ret = espnow_subpage_create_current();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create ESP-NOW page UI: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "ESP-NOW page created successfully");
    return ESP_OK;
}

static esp_err_t espnow_page_update(void)
{
    ESP_LOGD(TAG, "Updating ESP-NOW page data...");
    
    esp_err_t ret = espnow_subpage_update_current();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to update ESP-NOW page: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGD(TAG, "ESP-NOW page updated successfully");
    return ESP_OK;
}

static esp_err_t espnow_page_destroy(void)
{
    ESP_LOGI(TAG, "Destroying ESP-NOW page...");
    
    esp_err_t ret = espnow_subpage_destroy_current();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to destroy ESP-NOW page: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "ESP-NOW page destroyed successfully");

    g_current_subpage = ESPNOW_SUBPAGE_OVERVIEW; // Reset to default subpage
    return ESP_OK;
}

static bool espnow_page_is_data_updated(void)
{
    bool data_changed = false;
    
    // Check if ESP-NOW statistics notification was triggered
    bool stats_notified = atomic_exchange(&g_espnow_data_updated, false);
    if (stats_notified) {
        data_changed = true;
    }
    
    // Get current data for comparison
    // 1. Current uptime in seconds
    int64_t uptime_us = esp_timer_get_time();
    uint32_t current_uptime_sec = (uint32_t)(uptime_us / 1000000);
    
    // 2. Current free heap in KB
    uint32_t current_free_heap_kb = esp_get_free_heap_size() / 1024;
    
    // 3. Current ESP-NOW statistics
    espnow_stats_t current_stats = {0};
    espnow_manager_get_stats(&current_stats);
    
    // Compare with previous values to detect changes
    
    // Check uptime change (second-level precision)
    if (current_uptime_sec != g_prev_uptime_sec) {
        g_prev_uptime_sec = current_uptime_sec;
        data_changed = true;
    }
    
    // Check free heap change (KB-level precision)
    if (current_free_heap_kb != g_prev_free_heap_kb) {
        g_prev_free_heap_kb = current_free_heap_kb;
        data_changed = true;
    }
    
    // Check ESP-NOW statistics changes
    if (current_stats.packets_sent != g_prev_packets_sent) {
        g_prev_packets_sent = current_stats.packets_sent;
        data_changed = true;
    }
    
    if (current_stats.packets_received != g_prev_packets_received) {
        g_prev_packets_received = current_stats.packets_received;
        data_changed = true;
    }
    
    return data_changed;
}

void espnow_page_notify_data_update(void)
{
    // Atomic store: thread-safe assignment
    atomic_store(&g_espnow_data_updated, true);
}

/*=============================================================================
 * 📊  OVERVIEW PAGE IMPLEMENTATION (ESP-NOW statistics and system monitoring)
 *=============================================================================*/

// Internal UI creation function
static esp_err_t espnow_overview_create(void)
{
    // Get latest statistics from ESP-NOW manager before creating UI
    espnow_stats_t latest_stats = {0};
    if (espnow_manager_get_stats(&latest_stats) == ESP_OK) {
        // Update local stats with latest data
        g_espnow_stats = latest_stats;
    }
    
    lv_obj_t *scr = lv_scr_act();
    
    // Clear screen
    lv_obj_clean(scr);
    
    // Set black background to match Monitor page
    lv_obj_set_style_bg_color(scr, lv_color_black(), LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    
    // Title with page indicator - centered for 135px screen
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "ESP-NOW [2/2]");
    // Match Monitor page title color (cyan)
    lv_obj_set_style_text_color(title, lv_color_hex(0x00FFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_pos(title, 25, 5);  // Centered position for 135px screen
    // Store in overview structure for debugging
    g_overview_ui.title_label = title;

    // MAC Address display (moved up to replace status)
    lv_obj_t *mac_label = lv_label_create(scr);
    
    // Get real MAC address
    char mac_str[32];
    get_wifi_mac_string(mac_str, sizeof(mac_str));
    // Create MAC address background panel (full-width row)
    lv_obj_t *mac_panel = lv_obj_create(scr);
    lv_obj_set_size(mac_panel, 130, 20);  // Full width panel, 20px height
    lv_obj_set_pos(mac_panel, 2, 28);
    lv_obj_set_style_bg_color(mac_panel, lv_color_hex(0x000000), LV_PART_MAIN);  // Dark gray background
    lv_obj_set_style_bg_opa(mac_panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(mac_panel, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(mac_panel, 0, LV_PART_MAIN);
    lv_obj_clear_flag(mac_panel, LV_OBJ_FLAG_SCROLLABLE);

    // Create MAC label as child of the panel for proper background
    lv_label_set_text(mac_label, mac_str);
    lv_obj_set_parent(mac_label, mac_panel);  // Make it child of the panel
    lv_obj_set_style_text_color(mac_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(mac_label, &lv_font_montserrat_12, LV_PART_MAIN);  // Smaller font to fit
    lv_obj_center(mac_label);  // Center within the panel
    // Store in overview structure for debugging
    g_overview_ui.mac_label = mac_label;
    
    // Send counter with 36pt font
    g_overview_ui.sent_label = lv_label_create(scr);
    char sent_text[16];
    snprintf(sent_text, sizeof(sent_text), "%"PRIu32, g_espnow_stats.packets_sent);
    lv_label_set_text(g_overview_ui.sent_label, sent_text);
    lv_obj_set_style_text_color(g_overview_ui.sent_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(g_overview_ui.sent_label, &lv_font_montserrat_36, LV_PART_MAIN);
    lv_obj_set_pos(g_overview_ui.sent_label, 10, 52);
    
    // "Tx" label for sent packets (12pt, right-aligned in row)
    g_overview_ui.sent_tx_label = lv_label_create(scr);
    lv_label_set_text(g_overview_ui.sent_tx_label, "Tx");
    lv_obj_set_style_text_color(g_overview_ui.sent_tx_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(g_overview_ui.sent_tx_label, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_pos(g_overview_ui.sent_tx_label, 115, 75);  // Row end position
    
    // Receive counter with 36pt font 
    g_overview_ui.recv_label = lv_label_create(scr);
    char recv_text[16];
    snprintf(recv_text, sizeof(recv_text), "%"PRIu32, g_espnow_stats.packets_received);
    lv_label_set_text(g_overview_ui.recv_label, recv_text);
    lv_obj_set_style_text_color(g_overview_ui.recv_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(g_overview_ui.recv_label, &lv_font_montserrat_36, LV_PART_MAIN);
    lv_obj_set_pos(g_overview_ui.recv_label, 10, 98);
    
    // "Rx" label for received packets (12pt, right-aligned in row)
    g_overview_ui.recv_rx_label = lv_label_create(scr);
    lv_label_set_text(g_overview_ui.recv_rx_label, "Rx");
    lv_obj_set_style_text_color(g_overview_ui.recv_rx_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(g_overview_ui.recv_rx_label, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_pos(g_overview_ui.recv_rx_label, 115, 121);  // Row end position
    
    // Triple panel layout for Online Nodes (Y:144-174, like monitor page dual panels)
    // Left Panel: Online nodes count
    g_overview_ui.online_panel = lv_obj_create(scr);
    lv_obj_set_size(g_overview_ui.online_panel, 42, 30);
    lv_obj_set_pos(g_overview_ui.online_panel, 2, 144);
    lv_obj_set_style_bg_color(g_overview_ui.online_panel, lv_color_hex(0x204080), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_overview_ui.online_panel, LV_OPA_COVER, LV_PART_MAIN);  // Ensure opaque background
    lv_obj_set_style_border_width(g_overview_ui.online_panel, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(g_overview_ui.online_panel, 0, LV_PART_MAIN);
    lv_obj_clear_flag(g_overview_ui.online_panel, LV_OBJ_FLAG_SCROLLABLE);  // Remove scrollable flag
    
    lv_obj_t *online_title = lv_label_create(g_overview_ui.online_panel);
    lv_label_set_text(online_title, "ON");
    lv_obj_set_style_text_color(online_title, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(online_title, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_opa(online_title, LV_OPA_COVER, LV_PART_MAIN);  // Ensure opaque text
    lv_obj_center(online_title);
    lv_obj_set_pos(online_title, 0, -8);
    
    lv_obj_t *online_value = lv_label_create(g_overview_ui.online_panel);
    char online_text[8];
    snprintf(online_text, sizeof(online_text), "%d", g_espnow_stats.online_nodes);
    lv_label_set_text(online_value, online_text);
    lv_obj_set_style_text_color(online_value, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(online_value, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_opa(online_value, LV_OPA_COVER, LV_PART_MAIN);  // Ensure opaque text
    lv_obj_center(online_value);
    lv_obj_set_pos(online_value, 0, 7);
    
    // Center Panel: Used nodes count
    g_overview_ui.used_panel = lv_obj_create(scr);
    lv_obj_set_size(g_overview_ui.used_panel, 42, 30);
    lv_obj_set_pos(g_overview_ui.used_panel, 46, 144);
    lv_obj_set_style_bg_color(g_overview_ui.used_panel, lv_color_hex(0x204080), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_overview_ui.used_panel, LV_OPA_COVER, LV_PART_MAIN);  // Ensure opaque background
    lv_obj_set_style_border_width(g_overview_ui.used_panel, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(g_overview_ui.used_panel, 0, LV_PART_MAIN);
    lv_obj_clear_flag(g_overview_ui.used_panel, LV_OBJ_FLAG_SCROLLABLE);  // Remove scrollable flag
    
    lv_obj_t *used_title = lv_label_create(g_overview_ui.used_panel);
    lv_label_set_text(used_title, "USE");
    lv_obj_set_style_text_color(used_title, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(used_title, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_opa(used_title, LV_OPA_COVER, LV_PART_MAIN);  // Ensure opaque text
    lv_obj_center(used_title);
    lv_obj_set_pos(used_title, 0, -8);
    
    lv_obj_t *used_value = lv_label_create(g_overview_ui.used_panel);
    char used_text[8];
    snprintf(used_text, sizeof(used_text), "%d", g_espnow_stats.used_nodes);
    lv_label_set_text(used_value, used_text);
    lv_obj_set_style_text_color(used_value, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(used_value, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_opa(used_value, LV_OPA_COVER, LV_PART_MAIN);  // Ensure opaque text
    lv_obj_center(used_value);
    lv_obj_set_pos(used_value, 0, 7);
    
    // Right Panel: Total nodes count
    g_overview_ui.total_panel = lv_obj_create(scr);
    lv_obj_set_size(g_overview_ui.total_panel, 42, 30);
    lv_obj_set_pos(g_overview_ui.total_panel, 90, 144);
    lv_obj_set_style_bg_color(g_overview_ui.total_panel, lv_color_hex(0x204080), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_overview_ui.total_panel, LV_OPA_COVER, LV_PART_MAIN);  // Ensure opaque background
    lv_obj_set_style_border_width(g_overview_ui.total_panel, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(g_overview_ui.total_panel, 0, LV_PART_MAIN);
    lv_obj_clear_flag(g_overview_ui.total_panel, LV_OBJ_FLAG_SCROLLABLE);  // Remove scrollable flag
    
    lv_obj_t *total_title = lv_label_create(g_overview_ui.total_panel);
    lv_label_set_text(total_title, "TOT");
    lv_obj_set_style_text_color(total_title, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(total_title, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_opa(total_title, LV_OPA_COVER, LV_PART_MAIN);  // Ensure opaque text
    lv_obj_center(total_title);
    lv_obj_set_pos(total_title, 0, -8);
    
    lv_obj_t *total_value = lv_label_create(g_overview_ui.total_panel);
    char total_text[8];
    snprintf(total_text, sizeof(total_text), "%d", g_espnow_stats.total_nodes);
    lv_label_set_text(total_value, total_text);
    lv_obj_set_style_text_color(total_value, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(total_value, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_opa(total_value, LV_OPA_COVER, LV_PART_MAIN);  // Ensure opaque text
    lv_obj_center(total_value);
    lv_obj_set_pos(total_value, 0, 7);
    
    // Uptime at bottom-left (portrait layout: Y:225)
    g_overview_ui.uptime_label = lv_label_create(scr);
    char uptime_text[16];
    format_uptime_string(uptime_text, sizeof(uptime_text));
    lv_label_set_text(g_overview_ui.uptime_label, uptime_text);
    lv_obj_set_style_text_color(g_overview_ui.uptime_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(g_overview_ui.uptime_label, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_opa(g_overview_ui.uptime_label, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_pos(g_overview_ui.uptime_label, 5, 225);
    
    // Memory at bottom-right (portrait layout: Y:225)
    g_overview_ui.memory_label = lv_label_create(scr);
    char memory_text[16];
    format_free_memory_string(memory_text, sizeof(memory_text));
    lv_label_set_text(g_overview_ui.memory_label, memory_text);
    lv_obj_set_style_text_color(g_overview_ui.memory_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(g_overview_ui.memory_label, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_opa(g_overview_ui.memory_label, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_pos(g_overview_ui.memory_label, 80, 225);  // Right-aligned for 135px width
    
    return ESP_OK;
}

// Internal UI update function
static esp_err_t espnow_overview_update(void)
{
    // Get latest statistics from ESP-NOW manager
    espnow_stats_t latest_stats = {0};
    if (espnow_manager_get_stats(&latest_stats) == ESP_OK) {
        // Update local stats with latest data
        g_espnow_stats = latest_stats;
    }
    
    // Update uptime display
    if (g_overview_ui.uptime_label != NULL) {
        char uptime_text[16];
        format_uptime_string(uptime_text, sizeof(uptime_text));
        lv_label_set_text(g_overview_ui.uptime_label, uptime_text);
    }
    
    // Update memory display
    if (g_overview_ui.memory_label != NULL) {
        char memory_text[16];
        format_free_memory_string(memory_text, sizeof(memory_text));
        lv_label_set_text(g_overview_ui.memory_label, memory_text);
    }
    
    // Update packet statistics (48pt numbers only)
    if (g_overview_ui.sent_label != NULL) {
        char sent_text[16];
        snprintf(sent_text, sizeof(sent_text), "%"PRIu32, g_espnow_stats.packets_sent);
        lv_label_set_text(g_overview_ui.sent_label, sent_text);
    }
    
    if (g_overview_ui.recv_label != NULL) {
        char recv_text[16];
        snprintf(recv_text, sizeof(recv_text), "%"PRIu32, g_espnow_stats.packets_received);
        lv_label_set_text(g_overview_ui.recv_label, recv_text);
    }
    
    // Update Online Node statistics in triple panels (24pt numbers)
    if (g_overview_ui.online_panel != NULL) {
        // Find the value label child of online panel and update it
        lv_obj_t *online_value = lv_obj_get_child(g_overview_ui.online_panel, 1); // Second child (value label)
        if (online_value != NULL) {
            char online_text[8];
            snprintf(online_text, sizeof(online_text), "%d", g_espnow_stats.online_nodes);
            lv_label_set_text(online_value, online_text);
        }
    }
    
    if (g_overview_ui.used_panel != NULL) {
        // Find the value label child of used panel and update it
        lv_obj_t *used_value = lv_obj_get_child(g_overview_ui.used_panel, 1); // Second child (value label)
        if (used_value != NULL) {
            char used_text[8];
            snprintf(used_text, sizeof(used_text), "%d", g_espnow_stats.used_nodes);
            lv_label_set_text(used_value, used_text);
        }
    }
    
    if (g_overview_ui.total_panel != NULL) {
        // Find the value label child of total panel and update it
        lv_obj_t *total_value = lv_obj_get_child(g_overview_ui.total_panel, 1); // Second child (value label)
        if (total_value != NULL) {
            char total_text[8];
            snprintf(total_text, sizeof(total_text), "%d", g_espnow_stats.total_nodes);
            lv_label_set_text(total_value, total_text);
        }
    }
    
    return ESP_OK;
}

// Internal UI destruction function  
static esp_err_t espnow_overview_destroy(void)
{
    // Clear screen will automatically clean up all child objects
    lv_obj_t *scr = lv_scr_act();
    lv_obj_clean(scr);
    
    // Reset object pointers in overview structure
    memset(&g_overview_ui, 0, sizeof(espnow_overview_t));
    
    return ESP_OK;
}

/*=============================================================================
 * 🔍  NODE DETAIL PAGE IMPLEMENTATION (Individual device monitoring)
 *=============================================================================*/

// Node detail page UI creation function
static esp_err_t espnow_node_detail_create(void)
{
    ESP_LOGI(TAG, "Creating ESP-NOW node detail page...");
    
    lv_obj_t *scr = lv_scr_act();
    
    // Clear screen
    lv_obj_clean(scr);
    
    // Set black background to match other pages
    lv_obj_set_style_bg_color(scr, lv_color_black(), LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    
    // Title with page indicator - centered for 135px screen
    g_node_detail_ui.title_label = lv_label_create(scr);
    lv_label_set_text(g_node_detail_ui.title_label, "Node Detail [2/2]");
    lv_obj_set_style_text_color(g_node_detail_ui.title_label, lv_color_hex(0x00FFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(g_node_detail_ui.title_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_pos(g_node_detail_ui.title_label, 15, 5);  // Centered position for 135px screen
    
    // Create UI elements with initial placeholder text - data will be populated by refresh function
    
    // Row 1: Device ID and Network Information 
    g_node_detail_ui.network_row_label = lv_label_create(scr);
    lv_label_set_text(g_node_detail_ui.network_row_label, "0:--- | RSSI: ---");
    lv_obj_set_style_text_color(g_node_detail_ui.network_row_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(g_node_detail_ui.network_row_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_pos(g_node_detail_ui.network_row_label, 5, 25);
    
    // Row 2: Power Information - Main Display (24pt)
    g_node_detail_ui.power_label = lv_label_create(scr);
    lv_label_set_text(g_node_detail_ui.power_label, "-----.-");
    lv_obj_set_style_text_color(g_node_detail_ui.power_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(g_node_detail_ui.power_label, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_pos(g_node_detail_ui.power_label, 10, 60);  // Adjusted for 24pt baseline alignment
    
    // Power Unit Label "W" (18pt, like voltage unit)
    lv_obj_t *power_unit_label = lv_label_create(scr);
    lv_label_set_text(power_unit_label, "W");
    lv_obj_set_style_text_color(power_unit_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(power_unit_label, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_pos(power_unit_label, 115, 65);  // Aligned with 24pt power value baseline
    
    // Row 3: Dual Panel Row for Voltage & Current (Y:95-125, like Dual Panel Row)
    // Left Panel: Voltage
    lv_obj_t *voltage_panel = lv_obj_create(scr);
    lv_obj_set_size(voltage_panel, 63, 30);
    lv_obj_set_pos(voltage_panel, 2, 95);
    lv_obj_set_style_bg_color(voltage_panel, lv_color_hex(0x204080), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(voltage_panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(voltage_panel, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(voltage_panel, 0, LV_PART_MAIN);
    lv_obj_clear_flag(voltage_panel, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_t *voltage_title = lv_label_create(voltage_panel);
    lv_label_set_text(voltage_title, "VOLT");
    lv_obj_set_style_text_color(voltage_title, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(voltage_title, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_center(voltage_title);
    lv_obj_set_pos(voltage_title, 0, -8);
    
    lv_obj_t *voltage_value = lv_label_create(voltage_panel);
    lv_label_set_text(voltage_value, "---.-");
    lv_obj_set_style_text_color(voltage_value, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(voltage_value, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_center(voltage_value);
    lv_obj_set_pos(voltage_value, 0, 7);
    
    // Right Panel: Current
    lv_obj_t *current_panel = lv_obj_create(scr);
    lv_obj_set_size(current_panel, 63, 30);
    lv_obj_set_pos(current_panel, 70, 95);
    lv_obj_set_style_bg_color(current_panel, lv_color_hex(0x204080), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(current_panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(current_panel, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(current_panel, 0, LV_PART_MAIN);
    lv_obj_clear_flag(current_panel, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_t *current_title = lv_label_create(current_panel);
    lv_label_set_text(current_title, "CURR");
    lv_obj_set_style_text_color(current_title, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(current_title, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_center(current_title);
    lv_obj_set_pos(current_title, 0, -8);
    
    lv_obj_t *current_value = lv_label_create(current_panel);
    lv_label_set_text(current_value, "--.-");
    lv_obj_set_style_text_color(current_value, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(current_value, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_center(current_value);
    lv_obj_set_pos(current_value, 0, 7);
    
    // Store references for later updates
    g_node_detail_ui.electrical_row_label = voltage_panel;  // Store voltage panel for updates
    g_node_detail_ui.current_panel = current_panel;         // Store current panel for updates
    
    // Row 4: System Information (moved down after dual panel)
    g_node_detail_ui.system_row_label = lv_label_create(scr);
    lv_label_set_text(g_node_detail_ui.system_row_label, "UP: --:--:-- | --KB | FW: ---");
    lv_obj_set_style_text_color(g_node_detail_ui.system_row_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(g_node_detail_ui.system_row_label, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_pos(g_node_detail_ui.system_row_label, 5, 130);
    
    // Row 5: Compile time information (moved down)
    g_node_detail_ui.compile_label = lv_label_create(scr);
    lv_label_set_text(g_node_detail_ui.compile_label, "Built: ---");
    lv_obj_set_style_text_color(g_node_detail_ui.compile_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(g_node_detail_ui.compile_label, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_pos(g_node_detail_ui.compile_label, 5, 150);
    
    // Uptime at bottom-left (LOCAL DEVICE system uptime, same as overview page)
    g_node_detail_ui.uptime_label = lv_label_create(scr);
    char uptime_text[16];
    format_uptime_string(uptime_text, sizeof(uptime_text));
    lv_label_set_text(g_node_detail_ui.uptime_label, uptime_text);
    lv_obj_set_style_text_color(g_node_detail_ui.uptime_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(g_node_detail_ui.uptime_label, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_opa(g_node_detail_ui.uptime_label, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_pos(g_node_detail_ui.uptime_label, 5, 225);
    
    // Memory at bottom-right (LOCAL DEVICE memory, same as overview page)
    // NOTE: This shows local M5StickC Plus memory, NOT remote device memory
    // Remote device memory is shown in system_row_label above from TLV data
    g_node_detail_ui.memory_label = lv_label_create(scr);
    char memory_text[16];
    format_free_memory_string(memory_text, sizeof(memory_text));
    lv_label_set_text(g_node_detail_ui.memory_label, memory_text);
    lv_obj_set_style_text_color(g_node_detail_ui.memory_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(g_node_detail_ui.memory_label, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_opa(g_node_detail_ui.memory_label, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_pos(g_node_detail_ui.memory_label, 80, 225);  // Right-aligned
    
    // Populate data using common refresh function (replaces placeholder text if data is available)
    esp_err_t ret = espnow_node_detail_refresh_data_and_ui();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to refresh node detail data during creation: %s", esp_err_to_name(ret));
        // Continue anyway - UI is created with placeholders
    }
    
    ESP_LOGI(TAG, "ESP-NOW node detail page created successfully");
    return ESP_OK;
}

// Node detail page UI update function
static esp_err_t espnow_node_detail_update(void)
{
    ESP_LOGD(TAG, "Updating ESP-NOW node detail page...");
    
    // Always update LOCAL DEVICE uptime display (same as overview page)
    if (g_node_detail_ui.uptime_label != NULL) {
        char uptime_text[16];
        format_uptime_string(uptime_text, sizeof(uptime_text));
        lv_label_set_text(g_node_detail_ui.uptime_label, uptime_text);
    }
    
    // Always update LOCAL DEVICE memory display (same as overview page)
    // NOTE: Remote device memory is handled in refresh_data_and_ui function
    if (g_node_detail_ui.memory_label != NULL) {
        char memory_text[16];
        format_free_memory_string(memory_text, sizeof(memory_text));
        lv_label_set_text(g_node_detail_ui.memory_label, memory_text);
    }
    
    // Use common function to refresh device data and UI
    esp_err_t ret = espnow_node_detail_refresh_data_and_ui();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to refresh node detail data and UI: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGD(TAG, "ESP-NOW node detail page updated successfully");
    return ESP_OK;
}

// Internal helper function for updating node detail data and UI
// This function handles device data retrieval and UI updates for all labels
static esp_err_t espnow_node_detail_refresh_data_and_ui(void)
{
    ESP_LOGD(TAG, "Refreshing ESP-NOW node detail data and UI...");
    
    // Get latest device information from ESP-NOW manager (current device index)
    espnow_device_info_t device_info = {0};
    bool have_real_data = false;
    
    if (espnow_manager_get_device_info(g_current_device_index, &device_info) == ESP_OK) {
        // Update global node data with real device information
        memcpy(g_current_node_data.mac_address, device_info.mac_address, 6);
        g_current_node_data.rssi = device_info.rssi;
        g_current_node_data.uptime_seconds = device_info.uptime_seconds;
        g_current_node_data.ac_voltage = device_info.ac_voltage;
        g_current_node_data.ac_current = device_info.ac_current;
        g_current_node_data.ac_power = device_info.ac_power;
        g_current_node_data.power_factor = device_info.ac_power_factor;
        g_current_node_data.temperature = device_info.temperature;
        g_current_node_data.free_memory_kb = device_info.free_memory_kb;
        g_current_node_data.error_code = device_info.error_code;
        
        // Update string fields if available
        if (strlen(device_info.device_id) > 0) {
            strncpy(g_current_node_data.device_id, device_info.device_id, sizeof(g_current_node_data.device_id) - 1);
        }
        if (strlen(device_info.firmware_version) > 0) {
            strncpy(g_current_node_data.firmware_version, device_info.firmware_version, sizeof(g_current_node_data.firmware_version) - 1);
        }
        if (strlen(device_info.compile_time) > 0) {
            strncpy(g_current_node_data.compile_time, device_info.compile_time, sizeof(g_current_node_data.compile_time) - 1);
        }
        
        have_real_data = true;
        ESP_LOGD(TAG, "📊 Using real device data: MAC=" MACSTR ", entries=%d, uptime=%lu", 
                MAC2STR(device_info.mac_address), device_info.entry_count, device_info.uptime_seconds);
    } else {
        ESP_LOGD(TAG, "📊 No real device data available, using placeholders");
    }
    
    // Update Row 1: Network information (Device ID and RSSI)
    if (g_node_detail_ui.network_row_label != NULL) {
        char network_text[80];
        if (have_real_data) {
            snprintf(network_text, sizeof(network_text), 
                     "%d:%s | RSSI:%d",
                     g_current_device_index,
                     g_current_node_data.device_id,
                     g_current_node_data.rssi);
        } else {
            snprintf(network_text, sizeof(network_text), "%d:--- | RSSI: ---", g_current_device_index);
        }
        lv_label_set_text(g_node_detail_ui.network_row_label, network_text);
    }
    
    // Update Row 2: Power information - Main Display (48pt)
    if (g_node_detail_ui.power_label != NULL) {
        char power_text[20];
        if (have_real_data) {
            snprintf(power_text, sizeof(power_text), "%06.1f", g_current_node_data.ac_power);
        } else {
            snprintf(power_text, sizeof(power_text), "-----.-");
        }
        lv_label_set_text(g_node_detail_ui.power_label, power_text);
    }
    
    // Update Row 3: Dual Panel - Voltage (Left Panel)
    if (g_node_detail_ui.electrical_row_label != NULL) {
        // Find the voltage value label (second child of voltage panel)
        lv_obj_t *voltage_value = lv_obj_get_child(g_node_detail_ui.electrical_row_label, 1);
        if (voltage_value != NULL) {
            char voltage_text[16];
            if (have_real_data) {
                snprintf(voltage_text, sizeof(voltage_text), "%.1f", g_current_node_data.ac_voltage);
            } else {
                snprintf(voltage_text, sizeof(voltage_text), "---.-");
            }
            lv_label_set_text(voltage_value, voltage_text);
        }
    }
    
    // Update Row 3: Dual Panel - Current (Right Panel)
    if (g_node_detail_ui.current_panel != NULL) {
        // Find the current value label (second child of current panel)
        lv_obj_t *current_value = lv_obj_get_child(g_node_detail_ui.current_panel, 1);
        if (current_value != NULL) {
            char current_text[16];
            if (have_real_data) {
                snprintf(current_text, sizeof(current_text), "%.2f", g_current_node_data.ac_current);
            } else {
                snprintf(current_text, sizeof(current_text), "--.-");
            }
            lv_label_set_text(current_value, current_text);
        }
    }
    
    // Update Row 4: System information (Uptime, Memory, Firmware)
    if (g_node_detail_ui.system_row_label != NULL) {
        char system_text[80];
        if (have_real_data) {
            uint32_t hours = g_current_node_data.uptime_seconds / 3600;
            uint32_t minutes = (g_current_node_data.uptime_seconds % 3600) / 60;
            uint32_t seconds = g_current_node_data.uptime_seconds % 60;
            
            // Use remote device memory if available, otherwise show placeholder
            if (g_current_node_data.free_memory_kb > 0) {
                snprintf(system_text, sizeof(system_text), 
                         "UP:%02" PRIu32 ":%02" PRIu32 ":%02" PRIu32 " | %" PRIu32 "KB | FW:%s",
                         hours, minutes, seconds,
                         g_current_node_data.free_memory_kb,
                         g_current_node_data.firmware_version);
            } else {
                // No valid remote memory data - show N/A
                snprintf(system_text, sizeof(system_text), 
                         "UP:%02" PRIu32 ":%02" PRIu32 ":%02" PRIu32 " | N/A | FW:%s",
                         hours, minutes, seconds,
                         g_current_node_data.firmware_version);
            }
        } else {
            snprintf(system_text, sizeof(system_text), "UP: --:--:-- | --KB | FW: ---");
        }
        lv_label_set_text(g_node_detail_ui.system_row_label, system_text);
    }
    
    // Update Row 5: Compile time information
    if (g_node_detail_ui.compile_label != NULL) {
        char compile_text[40];
        if (have_real_data) {
            snprintf(compile_text, sizeof(compile_text), "Built: %s", g_current_node_data.compile_time);
        } else {
            snprintf(compile_text, sizeof(compile_text), "Built: ---");
        }
        lv_label_set_text(g_node_detail_ui.compile_label, compile_text);
    }
    
    ESP_LOGD(TAG, "ESP-NOW node detail data and UI refreshed successfully");
    return ESP_OK;
}

// Node detail page UI destruction function
static esp_err_t espnow_node_detail_destroy(void)
{
    ESP_LOGI(TAG, "Destroying ESP-NOW node detail page...");
    
    // Clear screen will automatically clean up all child objects
    lv_obj_t *scr = lv_scr_act();
    lv_obj_clean(scr);
    
    // Reset object pointers in node detail structure
    memset(&g_node_detail_ui, 0, sizeof(espnow_node_detail_t));
    
    ESP_LOGI(TAG, "ESP-NOW node detail page destroyed successfully");
    return ESP_OK;
}

/*=============================================================================
 * ⌨️   KEY EVENT HANDLING (User input processing)
 *=============================================================================*/

// Page-specific key event handler
static bool espnow_page_handle_key_event(uint32_t key)
{
    ESP_LOGI(TAG, "📡 ESP-NOW page received key: %lu", key);
    
    switch (key) {
        case LV_KEY_ENTER:
            // Subpage-specific ENTER actions
            if (g_current_subpage == ESPNOW_SUBPAGE_OVERVIEW) {
                ESP_LOGI(TAG, "📤 ESP-NOW overview ENTER - Send test packet");
                // Send a real test packet through ESP-NOW manager
                espnow_manager_send_test_packet();
                return true;  // We handled this key
            } else if (g_current_subpage == ESPNOW_SUBPAGE_NODE_DETAIL) {
                ESP_LOGI(TAG, "🔄 ESP-NOW node detail ENTER - Switch to next device");
                // Get next valid device index
                int next_device_index = 0;
                esp_err_t ret = espnow_manager_get_next_valid_device_index(g_current_device_index, &next_device_index);
                if (ret == ESP_OK) {
                    ESP_LOGI(TAG, "📱 Switching from device index %d to %d", g_current_device_index, next_device_index);
                    g_current_device_index = next_device_index;
                    // Force data update to refresh UI with new device data
                    atomic_store(&g_espnow_data_updated, true);
                } else {
                    ESP_LOGW(TAG, "⚠️ No valid devices available for switching: %s", esp_err_to_name(ret));
                    // Still force refresh in case device becomes available
                    atomic_store(&g_espnow_data_updated, true);
                }
                return true;  // We handled this key
            }
            ESP_LOGD(TAG, "🔹 ENTER key not handled for subpage %d", g_current_subpage);
            return false;
            
        case LV_KEY_RIGHT:
            // Subpage switching: Overview -> Node Detail
            if (g_current_subpage == ESPNOW_SUBPAGE_OVERVIEW) {
                ESP_LOGI(TAG, "🔄 ESP-NOW RIGHT - Switch to Node Detail subpage");
                esp_err_t ret = espnow_subpage_switch(ESPNOW_SUBPAGE_NODE_DETAIL);
                if (ret == ESP_OK) {
                    ESP_LOGI(TAG, "✅ Successfully switched to node detail subpage");
                } else {
                    ESP_LOGE(TAG, "❌ Failed to switch to node detail subpage: %s", esp_err_to_name(ret));
                }
                return true;  // We handled this key
            } else if (g_current_subpage == ESPNOW_SUBPAGE_NODE_DETAIL) {
                ESP_LOGI(TAG, "📶 ESP-NOW node detail end, should switch to next main page");
                return false;
            }
            return false;
            
        default:
            ESP_LOGD(TAG, "🔹 ESP-NOW page - unhandled key: %lu", key);
            return false;  // Let global handler process this key
    }
}

/*=============================================================================
 * 📑  SUBPAGE MANAGEMENT IMPLEMENTATION (Overview/Node Detail switching)
 *=============================================================================*/

// Subpage management functions implementation

static esp_err_t espnow_subpage_switch(espnow_subpage_id_t subpage_id)
{
    if (subpage_id >= ESPNOW_SUBPAGE_COUNT) {
        ESP_LOGE(TAG, "Invalid subpage ID: %d", subpage_id);
        return ESP_ERR_INVALID_ARG;
    }
    
    if (subpage_id == g_current_subpage) {
        ESP_LOGD(TAG, "Already on subpage %d", subpage_id);
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Switching from subpage %d to subpage %d", g_current_subpage, subpage_id);
    
    // Destroy current subpage
    esp_err_t ret = espnow_subpage_destroy_current();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to destroy current subpage: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Update current subpage
    g_current_subpage = subpage_id;
    
    // Create new subpage
    ret = espnow_subpage_create_current();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create new subpage: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Subpage switch completed successfully");
    return ESP_OK;
}

static esp_err_t espnow_subpage_create_current(void)
{
    switch (g_current_subpage) {
        case ESPNOW_SUBPAGE_OVERVIEW:
            ESP_LOGD(TAG, "Creating overview subpage");
            return espnow_overview_create();
            
        case ESPNOW_SUBPAGE_NODE_DETAIL:
            ESP_LOGD(TAG, "Creating node detail subpage");
            return espnow_node_detail_create();
            
        default:
            ESP_LOGE(TAG, "Unknown subpage ID: %d", g_current_subpage);
            return ESP_ERR_INVALID_STATE;
    }
}

static esp_err_t espnow_subpage_update_current(void)
{
    switch (g_current_subpage) {
        case ESPNOW_SUBPAGE_OVERVIEW:
            return espnow_overview_update();
            
        case ESPNOW_SUBPAGE_NODE_DETAIL:
            return espnow_node_detail_update();
            
        default:
            ESP_LOGE(TAG, "Unknown subpage ID: %d", g_current_subpage);
            return ESP_ERR_INVALID_STATE;
    }
}

static esp_err_t espnow_subpage_destroy_current(void)
{
    switch (g_current_subpage) {
        case ESPNOW_SUBPAGE_OVERVIEW:
            return espnow_overview_destroy();
            
        case ESPNOW_SUBPAGE_NODE_DETAIL:
            return espnow_node_detail_destroy();
            
        default:
            ESP_LOGE(TAG, "Unknown subpage ID: %d", g_current_subpage);
            return ESP_ERR_INVALID_STATE;
    }
}
