#include "page_manager_espnow.h"
#include "page_manager.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_mac.h"
#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include <stdatomic.h>
#include <inttypes.h>

static const char *TAG = "ESPNOW_PAGE";

// ESP-NOW page UI objects - stored as static variables for updates
static lv_obj_t *g_espnow_uptime_label = NULL;
static lv_obj_t *g_espnow_memory_label = NULL;
static lv_obj_t *g_espnow_status_label = NULL;
static lv_obj_t *g_espnow_sent_label = NULL;
static lv_obj_t *g_espnow_recv_label = NULL;
static lv_obj_t *g_espnow_signal_bar = NULL;

// ESP-NOW data state management
static atomic_bool g_espnow_data_updated = ATOMIC_VAR_INIT(false);

// ESP-NOW statistics (simulated for now)
static uint32_t g_packets_sent = 0;
static uint32_t g_packets_received = 0;
static uint8_t g_signal_strength = 75; // 0-100

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
static esp_err_t create_espnow_page_ui(void);
static esp_err_t update_espnow_page_ui(void);
static esp_err_t destroy_espnow_page_ui(void);
static esp_err_t espnow_page_init(void);
static esp_err_t espnow_page_create(void);
static esp_err_t espnow_page_update(void);
static esp_err_t espnow_page_destroy(void);
static bool espnow_page_is_data_updated(void);

// Page controller interface implementation
static const page_controller_t espnow_controller = {
    .init = espnow_page_init,
    .create = espnow_page_create,
    .update = espnow_page_update,
    .destroy = espnow_page_destroy,
    .is_data_updated = espnow_page_is_data_updated,
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
    
    // Reset UI object pointers
    g_espnow_uptime_label = NULL;
    g_espnow_memory_label = NULL;
    g_espnow_status_label = NULL;
    g_espnow_sent_label = NULL;
    g_espnow_recv_label = NULL;
    g_espnow_signal_bar = NULL;
    
    // Reset data state - atomic variable doesn't need explicit initialization
    atomic_store(&g_espnow_data_updated, false);
    
    ESP_LOGI(TAG, "ESP-NOW page module initialized");
    return ESP_OK;
}

static esp_err_t espnow_page_create(void)
{
    ESP_LOGI(TAG, "Creating ESP-NOW page UI...");
    
    esp_err_t ret = create_espnow_page_ui();
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
    
    esp_err_t ret = update_espnow_page_ui();
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
    
    esp_err_t ret = destroy_espnow_page_ui();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to destroy ESP-NOW page: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "ESP-NOW page destroyed successfully");
    return ESP_OK;
}

static bool espnow_page_is_data_updated(void)
{
    // Atomic exchange: get current value and set to false in one operation
    return atomic_exchange(&g_espnow_data_updated, false);
}

void espnow_page_notify_data_update(void)
{
    // Atomic store: thread-safe assignment
    atomic_store(&g_espnow_data_updated, true);
}

// Internal UI creation function
static esp_err_t create_espnow_page_ui(void)
{
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
    g_espnow_sent_label = lv_label_create(scr);
    char sent_text[32];
    snprintf(sent_text, sizeof(sent_text), "Sent: %"PRIu32" packets", g_packets_sent);
    lv_label_set_text(g_espnow_sent_label, sent_text);
    lv_obj_set_style_text_color(g_espnow_sent_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(g_espnow_sent_label, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_pos(g_espnow_sent_label, 10, 52);
    
    // Receive counter (moved up)
    g_espnow_recv_label = lv_label_create(scr);
    char recv_text[32];
    snprintf(recv_text, sizeof(recv_text), "Received: %"PRIu32" packets", g_packets_received);
    lv_label_set_text(g_espnow_recv_label, recv_text);
    lv_obj_set_style_text_color(g_espnow_recv_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(g_espnow_recv_label, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_pos(g_espnow_recv_label, 10, 74);
    
    // Signal strength indicator
    g_espnow_signal_bar = lv_bar_create(scr);
    lv_obj_set_size(g_espnow_signal_bar, 80, 8);
    lv_obj_set_pos(g_espnow_signal_bar, 150, 75);
    lv_obj_set_style_bg_color(g_espnow_signal_bar, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_bg_color(g_espnow_signal_bar, lv_color_white(), LV_PART_INDICATOR);
    lv_bar_set_value(g_espnow_signal_bar, g_signal_strength, LV_ANIM_OFF);
    
    // Uptime at bottom-left
    g_espnow_uptime_label = lv_label_create(scr);
    char uptime_text[16];
    format_uptime_string(uptime_text, sizeof(uptime_text));
    lv_label_set_text(g_espnow_uptime_label, uptime_text);
    lv_obj_set_style_text_color(g_espnow_uptime_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(g_espnow_uptime_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_opa(g_espnow_uptime_label, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_pos(g_espnow_uptime_label, 5, 120);
    
    // Memory at bottom-right
    g_espnow_memory_label = lv_label_create(scr);
    char memory_text[16];
    format_free_memory_string(memory_text, sizeof(memory_text));
    lv_label_set_text(g_espnow_memory_label, memory_text);
    lv_obj_set_style_text_color(g_espnow_memory_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(g_espnow_memory_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_opa(g_espnow_memory_label, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_pos(g_espnow_memory_label, 170, 120);  // Right-aligned
    
    return ESP_OK;
}

// Internal UI update function
static esp_err_t update_espnow_page_ui(void)
{
    // Update uptime display
    if (g_espnow_uptime_label != NULL) {
        char uptime_text[16];
        format_uptime_string(uptime_text, sizeof(uptime_text));
        lv_label_set_text(g_espnow_uptime_label, uptime_text);
    }
    
    // Update memory display
    if (g_espnow_memory_label != NULL) {
        char memory_text[16];
        format_free_memory_string(memory_text, sizeof(memory_text));
        lv_label_set_text(g_espnow_memory_label, memory_text);
    }
    
    // Update packet statistics
    if (g_espnow_sent_label != NULL) {
        char sent_text[32];
        snprintf(sent_text, sizeof(sent_text), "Sent: %"PRIu32" packets", g_packets_sent);
        lv_label_set_text(g_espnow_sent_label, sent_text);
    }
    
    if (g_espnow_recv_label != NULL) {
        char recv_text[32];
        snprintf(recv_text, sizeof(recv_text), "Received: %"PRIu32" packets", g_packets_received);
        lv_label_set_text(g_espnow_recv_label, recv_text);
    }
    
    // Update signal strength bar
    if (g_espnow_signal_bar != NULL) {
        lv_bar_set_value(g_espnow_signal_bar, g_signal_strength, LV_ANIM_OFF);
    }
    
    // Simulate some activity (increment counters occasionally)
    static uint32_t update_counter = 0;
    update_counter++;
    if (update_counter % 10 == 0) {  // Every 10th update (5 seconds)
        g_packets_sent++;
        if (update_counter % 20 == 0) {  // Every 20th update (10 seconds)
            g_packets_received++;
        }
    }
    
    return ESP_OK;
}

// Internal UI destruction function  
static esp_err_t destroy_espnow_page_ui(void)
{
    // Clear screen will automatically clean up all child objects
    lv_obj_t *scr = lv_scr_act();
    lv_obj_clean(scr);
    
    // Reset object pointers
    g_espnow_uptime_label = NULL;
    g_espnow_memory_label = NULL;
    g_espnow_status_label = NULL;
    g_espnow_sent_label = NULL;
    g_espnow_recv_label = NULL;
    g_espnow_signal_bar = NULL;
    
    return ESP_OK;
}