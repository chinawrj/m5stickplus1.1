#include "page_manager_espnow.h"
#include "core/lv_group.h"
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
    lv_obj_t *sent_label;       // Sent packets counter
    lv_obj_t *recv_label;       // Received packets counter
    lv_obj_t *title_label;      // Page title
    lv_obj_t *mac_label;        // MAC address display
} espnow_overview_t;

// ESP-NOW node detail page UI objects structure (TLV data display)
typedef struct {
    lv_obj_t *title_label;      // Page title
    
    // Row 1: Device ID and Network Information (2 items)
    lv_obj_t *network_row_label; // "ID: M5NanoC6-123 | RSSI: -XX"
    
    // Row 2: Electrical Measurements (3 items)
    lv_obj_t *electrical_row_label; // "220.5V | 2.35A | 520W"
    
    // Row 3: System Information with Firmware (3 items)  
    lv_obj_t *system_row_label;   // "UP: 12:34:56 | 45KB | FW: v1.2.3"
    
    // Row 4: Version Information (not used anymore - FW moved to system_row)
    lv_obj_t *version_row_label;  // Unused, kept for structure compatibility
    
    // Note: Compile time is displayed as a separate local label (not stored in this struct)
} espnow_node_detail_t;

// Global UI overview structure
static espnow_overview_t g_espnow_overview = {0};

// Global UI node detail structure
static espnow_node_detail_t g_espnow_node_detail = {0};

// ESP-NOW subpage management
static espnow_subpage_id_t g_current_subpage = ESPNOW_SUBPAGE_OVERVIEW;

// ESP-NOW data state management
static atomic_bool g_espnow_data_updated = ATOMIC_VAR_INIT(false);

// Real ESP-NOW statistics from manager
static espnow_stats_t g_espnow_stats = {0};

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

// Static node data (for demonstration - in real implementation, this would come from ESP-NOW messages)
static espnow_node_data_t g_node_data = {
    .mac_address = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC},
    .rssi = -45,
    .wifi_connected = true,
    .espnow_active = true,
    .ac_voltage = 220.5f,
    .ac_current = 2.35f,
    .ac_power = 520.0f,
    .power_factor = 0.95f,
    .uptime_seconds = 45296,  // 12:34:56
    .temperature = 25.5f,
    .free_memory_kb = 45,
    .error_code = 0,
    .device_id = "M5NanoC6-123456",
    .firmware_version = "v1.2.3",
    .compile_time = "Sep 23 2024 10:30"
};

// Previous state for change detection
static uint32_t g_prev_uptime_sec = 0;      // Previous uptime in seconds
static uint32_t g_prev_free_heap_kb = 0;    // Previous free heap in KB
static uint32_t g_prev_packets_sent = 0;    // Previous sent packets count
static uint32_t g_prev_packets_received = 0; // Previous received packets count

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
static esp_err_t espnow_overview_create(void);
static esp_err_t espnow_overview_update(void);
static esp_err_t espnow_overview_destroy(void);
static esp_err_t espnow_node_detail_create(void);
static esp_err_t espnow_node_detail_update(void);
static esp_err_t espnow_node_detail_destroy(void);
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

static esp_err_t espnow_page_init(void)
{
    ESP_LOGI(TAG, "Initializing ESP-NOW page module");
    
    // Reset UI object pointers in overview structure
    memset(&g_espnow_overview, 0, sizeof(espnow_overview_t));
    
    // Reset UI object pointers in node detail structure
    memset(&g_espnow_node_detail, 0, sizeof(espnow_node_detail_t));
    
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
    
    // Title with page indicator
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "ESP-NOW [2/2]");
    // Match Monitor page title color (cyan)
    lv_obj_set_style_text_color(title, lv_color_hex(0x00FFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_pos(title, 60, 5);
    // Store in overview structure for debugging
    g_espnow_overview.title_label = title;

    // MAC Address display (moved up to replace status)
    lv_obj_t *mac_label = lv_label_create(scr);
    
    // Get real MAC address
    char mac_str[32];
    get_wifi_mac_string(mac_str, sizeof(mac_str));
    lv_label_set_text(mac_label, mac_str);
    
    lv_obj_set_style_text_color(mac_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(mac_label, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_pos(mac_label, 10, 30);
    // Store in overview structure for debugging
    g_espnow_overview.mac_label = mac_label;
    
    // Send counter (moved up)
    g_espnow_overview.sent_label = lv_label_create(scr);
    char sent_text[32];
    snprintf(sent_text, sizeof(sent_text), "Sent: %"PRIu32" packets", g_espnow_stats.packets_sent);
    lv_label_set_text(g_espnow_overview.sent_label, sent_text);
    lv_obj_set_style_text_color(g_espnow_overview.sent_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(g_espnow_overview.sent_label, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_pos(g_espnow_overview.sent_label, 10, 52);
    
    // Receive counter (moved up)
    g_espnow_overview.recv_label = lv_label_create(scr);
    char recv_text[32];
    snprintf(recv_text, sizeof(recv_text), "Received: %"PRIu32" packets", g_espnow_stats.packets_received);
    lv_label_set_text(g_espnow_overview.recv_label, recv_text);
    lv_obj_set_style_text_color(g_espnow_overview.recv_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(g_espnow_overview.recv_label, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_pos(g_espnow_overview.recv_label, 10, 74);
    
    // Uptime at bottom-left
    g_espnow_overview.uptime_label = lv_label_create(scr);
    char uptime_text[16];
    format_uptime_string(uptime_text, sizeof(uptime_text));
    lv_label_set_text(g_espnow_overview.uptime_label, uptime_text);
    lv_obj_set_style_text_color(g_espnow_overview.uptime_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(g_espnow_overview.uptime_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_opa(g_espnow_overview.uptime_label, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_pos(g_espnow_overview.uptime_label, 5, 120);
    
    // Memory at bottom-right
    g_espnow_overview.memory_label = lv_label_create(scr);
    char memory_text[16];
    format_free_memory_string(memory_text, sizeof(memory_text));
    lv_label_set_text(g_espnow_overview.memory_label, memory_text);
    lv_obj_set_style_text_color(g_espnow_overview.memory_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(g_espnow_overview.memory_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_opa(g_espnow_overview.memory_label, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_pos(g_espnow_overview.memory_label, 170, 120);  // Right-aligned
    
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
    if (g_espnow_overview.uptime_label != NULL) {
        char uptime_text[16];
        format_uptime_string(uptime_text, sizeof(uptime_text));
        lv_label_set_text(g_espnow_overview.uptime_label, uptime_text);
    }
    
    // Update memory display
    if (g_espnow_overview.memory_label != NULL) {
        char memory_text[16];
        format_free_memory_string(memory_text, sizeof(memory_text));
        lv_label_set_text(g_espnow_overview.memory_label, memory_text);
    }
    
    // Update packet statistics
    if (g_espnow_overview.sent_label != NULL) {
        char sent_text[32];
        snprintf(sent_text, sizeof(sent_text), "Sent: %"PRIu32" packets", g_espnow_stats.packets_sent);
        lv_label_set_text(g_espnow_overview.sent_label, sent_text);
    }
    
    if (g_espnow_overview.recv_label != NULL) {
        char recv_text[32];
        snprintf(recv_text, sizeof(recv_text), "Received: %"PRIu32" packets", g_espnow_stats.packets_received);
        lv_label_set_text(g_espnow_overview.recv_label, recv_text);
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
    memset(&g_espnow_overview, 0, sizeof(espnow_overview_t));
    
    return ESP_OK;
}

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
    
    // Title with page indicator
    g_espnow_node_detail.title_label = lv_label_create(scr);
    lv_label_set_text(g_espnow_node_detail.title_label, "Node Detail [2/2]");
    lv_obj_set_style_text_color(g_espnow_node_detail.title_label, lv_color_hex(0x00FFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(g_espnow_node_detail.title_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_pos(g_espnow_node_detail.title_label, 50, 5);
    
    // Row 1: Device ID and Network Information
    g_espnow_node_detail.network_row_label = lv_label_create(scr);
    char network_text[80];
    snprintf(network_text, sizeof(network_text), 
             "ID:%s | RSSI:%d",
             g_node_data.device_id,
             g_node_data.rssi);
    lv_label_set_text(g_espnow_node_detail.network_row_label, network_text);
    lv_obj_set_style_text_color(g_espnow_node_detail.network_row_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(g_espnow_node_detail.network_row_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_pos(g_espnow_node_detail.network_row_label, 5, 25);
    
    // Row 2: Electrical Measurements (Voltage, Current, Power)
    g_espnow_node_detail.electrical_row_label = lv_label_create(scr);
    char electrical_text[80];
    snprintf(electrical_text, sizeof(electrical_text), 
             "%.1fV | %.2fA | %.0fW",
             g_node_data.ac_voltage,
             g_node_data.ac_current,
             g_node_data.ac_power);
    lv_label_set_text(g_espnow_node_detail.electrical_row_label, electrical_text);
    lv_obj_set_style_text_color(g_espnow_node_detail.electrical_row_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(g_espnow_node_detail.electrical_row_label, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_pos(g_espnow_node_detail.electrical_row_label, 5, 45);
    
    // Row 3: System Information (Uptime, Memory, Firmware)
    g_espnow_node_detail.system_row_label = lv_label_create(scr);
    char system_text[80];
    uint32_t hours = g_node_data.uptime_seconds / 3600;
    uint32_t minutes = (g_node_data.uptime_seconds % 3600) / 60;
    uint32_t seconds = g_node_data.uptime_seconds % 60;
    snprintf(system_text, sizeof(system_text), 
             "UP:%02" PRIu32 ":%02" PRIu32 ":%02" PRIu32 " | %" PRIu32 "KB | FW:%s",
             hours, minutes, seconds,
             g_node_data.free_memory_kb,
             g_node_data.firmware_version);
    lv_label_set_text(g_espnow_node_detail.system_row_label, system_text);
    lv_obj_set_style_text_color(g_espnow_node_detail.system_row_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(g_espnow_node_detail.system_row_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_pos(g_espnow_node_detail.system_row_label, 5, 65);
    
    // Additional compile time information (gray color for 4th row)
    lv_obj_t *compile_label = lv_label_create(scr);
    char compile_text[40];
    snprintf(compile_text, sizeof(compile_text), "Built: %s", g_node_data.compile_time);
    lv_label_set_text(compile_label, compile_text);
    lv_obj_set_style_text_color(compile_label, lv_color_hex(0x808080), LV_PART_MAIN); // Gray color
    lv_obj_set_style_text_font(compile_label, &lv_font_montserrat_14, LV_PART_MAIN);   // Same font size
    lv_obj_set_pos(compile_label, 5, 85);
    
    ESP_LOGI(TAG, "ESP-NOW node detail page created successfully");
    return ESP_OK;
}

// Node detail page UI update function
static esp_err_t espnow_node_detail_update(void)
{
    ESP_LOGD(TAG, "Updating ESP-NOW node detail page...");
    
    // Get latest device information from ESP-NOW manager (first device at index 0)
    espnow_device_info_t device_info = {0};
    bool have_real_data = false;
    
    if (espnow_manager_get_device_info(0, &device_info) == ESP_OK) {
        // Update global node data with real device information
        memcpy(g_node_data.mac_address, device_info.mac_address, 6);
        g_node_data.rssi = device_info.rssi;
        g_node_data.uptime_seconds = device_info.uptime_seconds;
        g_node_data.ac_voltage = device_info.ac_voltage;
        g_node_data.ac_current = device_info.ac_current;
        g_node_data.ac_power = device_info.ac_power;
        g_node_data.power_factor = device_info.ac_power_factor;
        g_node_data.temperature = device_info.temperature;
        g_node_data.free_memory_kb = device_info.free_memory_kb;
        g_node_data.error_code = device_info.error_code;
        
        // Update string fields if available
        if (strlen(device_info.device_id) > 0) {
            strncpy(g_node_data.device_id, device_info.device_id, sizeof(g_node_data.device_id) - 1);
        }
        if (strlen(device_info.firmware_version) > 0) {
            strncpy(g_node_data.firmware_version, device_info.firmware_version, sizeof(g_node_data.firmware_version) - 1);
        }
        if (strlen(device_info.compile_time) > 0) {
            strncpy(g_node_data.compile_time, device_info.compile_time, sizeof(g_node_data.compile_time) - 1);
        }
        
        have_real_data = true;
        ESP_LOGD(TAG, "📊 Using real device data: MAC=" MACSTR ", entries=%d, uptime=%lu", 
                MAC2STR(device_info.mac_address), device_info.entry_count, device_info.uptime_seconds);
    } else {
        ESP_LOGD(TAG, "📊 No real device data available, using demo data");
    }
    
    // Update network information (Device ID and RSSI)
    if (g_espnow_node_detail.network_row_label != NULL) {
        char network_text[80];
        snprintf(network_text, sizeof(network_text), 
                 "%s%s | RSSI:%d%s",
                 have_real_data ? "🔗" : "📍",  // Icon indicates real vs demo data
                 g_node_data.device_id,
                 g_node_data.rssi,
                 have_real_data ? "" : " (demo)");
        lv_label_set_text(g_espnow_node_detail.network_row_label, network_text);
    }
    
    // Update electrical measurements (simplified to match create function)
    if (g_espnow_node_detail.electrical_row_label != NULL) {
        char electrical_text[80];
        snprintf(electrical_text, sizeof(electrical_text), 
                 "%.1fV | %.2fA | %.0fW%s",
                 g_node_data.ac_voltage,
                 g_node_data.ac_current,
                 g_node_data.ac_power,
                 have_real_data ? "" : " (demo)");
        lv_label_set_text(g_espnow_node_detail.electrical_row_label, electrical_text);
    }
    
    // Update system information (Uptime, Memory, Firmware)
    if (g_espnow_node_detail.system_row_label != NULL) {
        char system_text[80];
        uint32_t hours = g_node_data.uptime_seconds / 3600;
        uint32_t minutes = (g_node_data.uptime_seconds % 3600) / 60;
        uint32_t seconds = g_node_data.uptime_seconds % 60;
        snprintf(system_text, sizeof(system_text), 
                 "UP:%02" PRIu32 ":%02" PRIu32 ":%02" PRIu32 " | %" PRIu32 "KB | FW:%s",
                 hours, minutes, seconds,
                 g_node_data.free_memory_kb,
                 g_node_data.firmware_version);
        lv_label_set_text(g_espnow_node_detail.system_row_label, system_text);
    }
    
    ESP_LOGD(TAG, "ESP-NOW node detail page updated successfully (%s)", 
             have_real_data ? "real data" : "demo data");
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
    memset(&g_espnow_node_detail, 0, sizeof(espnow_node_detail_t));
    
    ESP_LOGI(TAG, "ESP-NOW node detail page destroyed successfully");
    return ESP_OK;
}

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
                ESP_LOGI(TAG, "🔄 ESP-NOW node detail ENTER - Refresh data");
                // Force data update for node detail
                atomic_store(&g_espnow_data_updated, true);
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
