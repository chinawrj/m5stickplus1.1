/**
 * ESP-NOW Manager - Official Example Integration
 * 
 * This module integrates the official ESP-IDF ESP-NOW example code
 * into the M5StickC Plus project, maintaining compatibility with
 * the official communication protocol and flow.
 */

#include "espnow_manager.h"
#include "esp_now.h"  // Must be included BEFORE espnow_example.h for ESP_NOW_ETH_ALEN
#include "espnow_example.h"
#include "page_manager_espnow.h"
#include "esphome_tlv_format.h"  // TLV data format for ESP-NOW communication
#include "ux_service.h"  // LED animation support
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_random.h"
#include "esp_crc.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include <string.h>
#include <inttypes.h>

static const char *TAG = "ESPNOW_MGR";

// ESP-NOW configuration matching official example
#define ESPNOW_MAXDELAY 512

// Global state variables (matching official example)
static QueueHandle_t s_espnow_queue = NULL;
static uint8_t s_broadcast_mac[ESP_NOW_ETH_ALEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
// For compatibility with espnow_example.h macro
extern uint8_t s_example_broadcast_mac[ESP_NOW_ETH_ALEN];
uint8_t s_example_broadcast_mac[ESP_NOW_ETH_ALEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
static uint16_t s_espnow_seq[EXAMPLE_ESPNOW_DATA_MAX] = { 0, 0 };
static espnow_stats_t s_stats = {0};
static bool s_espnow_running = false;

// LED animation rate limiting constant
static const uint32_t LED_ANIMATION_INTERVAL_MS = 1000;  // 1 second

// Device Discovery Task Variables
typedef struct {
    uint8_t *buffer;              // Send buffer
    int len;                      // Buffer length
    uint32_t magic;               // Magic number for identification
    uint32_t last_send_time;      // Timestamp of last successful send (in ticks)
    bool send_completed;          // Flag indicating last send completed
} device_discovery_param_t;

static device_discovery_param_t *s_discovery_param = NULL;
static TaskHandle_t s_discovery_task_handle = NULL;

// TLV Device Storage Configuration
#define MAX_TLV_DEVICES 16          // Maximum number of devices to track
#define MAX_TLV_ENTRIES_PER_DEVICE 32  // Maximum TLV entries per device
#define MAX_TLV_ENTRY_VALUE_SIZE 64    // Maximum value size for a single TLV entry

// Individual TLV entry storage
typedef struct {
    uint8_t type;                   // TLV type
    uint8_t length;                 // TLV value length
    uint8_t value[MAX_TLV_ENTRY_VALUE_SIZE];  // TLV value data
    uint32_t last_updated;          // Timestamp of last update (in ticks)
    bool valid;                     // Whether this entry is valid
} stored_tlv_entry_t;

// Device TLV information storage
typedef struct {
    uint8_t mac_address[ESP_NOW_ETH_ALEN];  // Device MAC address
    stored_tlv_entry_t tlv_entries[MAX_TLV_ENTRIES_PER_DEVICE];  // TLV entries for this device
    uint16_t entry_count;           // Number of valid TLV entries
    uint32_t last_seen;             // Last time we received data from this device
    int8_t rssi;                    // Latest RSSI value from ESP-NOW reception
    bool in_use;                    // Whether this device slot is in use
    char device_name[32];           // Device friendly name (optional)
} device_tlv_storage_t;

// Global TLV device storage array
static device_tlv_storage_t g_tlv_devices[MAX_TLV_DEVICES];
static SemaphoreHandle_t g_tlv_mutex = NULL;  // Mutex for thread-safe access

// Forward declarations
static void espnow_wifi_init(void);
static void espnow_send_cb(const esp_now_send_info_t *tx_info, esp_now_send_status_t status);
static void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len);
static int espnow_data_parse(uint8_t *data, uint16_t data_len, uint8_t *state, uint16_t *seq, uint32_t *magic);
static void espnow_trigger_led_animation(void);

// TLV helper functions
static const char* tlv_type_to_string(uint8_t type);

// TLV Device Storage Functions
static esp_err_t tlv_storage_init(void);
static void tlv_storage_deinit(void);
static device_tlv_storage_t* find_device_by_mac(const uint8_t *mac_addr);
static device_tlv_storage_t* get_or_create_device(const uint8_t *mac_addr);
static esp_err_t store_device_tlv_data(const uint8_t *mac_addr, const uint8_t *tlv_data, size_t data_len, int8_t rssi);
static void process_received_tlv_data(const uint8_t *mac_addr, const uint8_t *data, size_t data_len, int8_t rssi);
static void print_device_tlv_info(const device_tlv_storage_t *device);

// Device Discovery Task Functions
static void device_discovery_data_prepare(device_discovery_param_t *param);
static void device_discovery_task(void *pvParameter);
static void device_discovery_cleanup(void);
static void espnow_recv_only_task(void *pvParameter);

esp_err_t espnow_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing ESP-NOW Manager");
    
    // Reset statistics
    memset(&s_stats, 0, sizeof(s_stats));
    s_espnow_running = false;
    
    // Initialize TLV device storage
    esp_err_t ret = tlv_storage_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize TLV storage: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "ESP-NOW Manager initialized");
    return ESP_OK;
}

esp_err_t espnow_manager_start(void)
{
    if (s_espnow_running) {
        ESP_LOGW(TAG, "ESP-NOW already running");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Starting ESP-NOW with Device Discovery Task");
    
    // Initialize WiFi first (required for ESP-NOW)
    espnow_wifi_init();
    
    // Create event queue for receiving events
    s_espnow_queue = xQueueCreate(ESPNOW_QUEUE_SIZE, sizeof(example_espnow_event_t));
    if (s_espnow_queue == NULL) {
        ESP_LOGE(TAG, "Create queue fail");
        return ESP_FAIL;
    }
    
    // Initialize ESP-NOW and register callbacks
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(espnow_send_cb));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));
    
    // Set primary master key (from configuration)
    ESP_ERROR_CHECK(esp_now_set_pmk((uint8_t *)CONFIG_ESPNOW_PMK));
    
    // Add broadcast peer
    esp_now_peer_info_t *peer = malloc(sizeof(esp_now_peer_info_t));
    if (peer == NULL) {
        ESP_LOGE(TAG, "Malloc peer information fail");
        vQueueDelete(s_espnow_queue);
        s_espnow_queue = NULL;
        esp_now_deinit();
        return ESP_FAIL;
    }
    
    memset(peer, 0, sizeof(esp_now_peer_info_t));
    peer->channel = CONFIG_ESPNOW_CHANNEL;
    peer->ifidx = ESPNOW_WIFI_IF;
    peer->encrypt = false;
    memcpy(peer->peer_addr, s_broadcast_mac, ESP_NOW_ETH_ALEN);
    ESP_ERROR_CHECK(esp_now_add_peer(peer));
    free(peer);
    
    // Initialize device discovery parameters
    s_discovery_param = malloc(sizeof(device_discovery_param_t));
    if (s_discovery_param == NULL) {
        ESP_LOGE(TAG, "Malloc discovery parameter fail");
        vQueueDelete(s_espnow_queue);
        s_espnow_queue = NULL;
        esp_now_deinit();
        return ESP_FAIL;
    }
    
    memset(s_discovery_param, 0, sizeof(device_discovery_param_t));
    s_discovery_param->len = CONFIG_ESPNOW_SEND_LEN;
    s_discovery_param->magic = esp_random();
    s_discovery_param->send_completed = true;  // Initially ready to send
    s_discovery_param->last_send_time = 0;
    
    s_discovery_param->buffer = malloc(CONFIG_ESPNOW_SEND_LEN);
    if (s_discovery_param->buffer == NULL) {
        ESP_LOGE(TAG, "Malloc discovery buffer fail");
        free(s_discovery_param);
        s_discovery_param = NULL;
        vQueueDelete(s_espnow_queue);
        s_espnow_queue = NULL;
        esp_now_deinit();
        return ESP_FAIL;
    }
    
    // Store magic number in stats
    s_stats.magic_number = s_discovery_param->magic;
    
    s_espnow_running = true;
    
    // Create device discovery task (NEW - replaces original espnow_task)
    BaseType_t task_created = xTaskCreate(
        device_discovery_task, 
        "device_discovery", 
        4096,  // Stack size - sufficient for discovery logic
        s_discovery_param, 
        4,     // Priority
        &s_discovery_task_handle
    );
    
    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create device discovery task");
        device_discovery_cleanup();
        vQueueDelete(s_espnow_queue);
        s_espnow_queue = NULL;
        esp_now_deinit();
        s_espnow_running = false;
        return ESP_FAIL;
    }
    
    // Also create a receive-only task to process incoming data
    example_espnow_send_param_t *recv_param = malloc(sizeof(example_espnow_send_param_t));
    if (recv_param == NULL) {
        ESP_LOGE(TAG, "Malloc recv parameter fail");
        device_discovery_cleanup();
        vQueueDelete(s_espnow_queue);
        s_espnow_queue = NULL;
        esp_now_deinit();
        s_espnow_running = false;
        return ESP_FAIL;
    }
    
    // Initialize basic recv parameters for receive-only processing
    memset(recv_param, 0, sizeof(example_espnow_send_param_t));
    recv_param->magic = s_discovery_param->magic;
    recv_param->broadcast = false;  // Disable all sending in recv task
    recv_param->unicast = false;
    recv_param->state = 1;  // Always state 1 for discovery
    recv_param->count = 0;  // No sending allowed
    recv_param->len = 0;    // No send buffer needed
    recv_param->buffer = NULL;  // No send buffer
    
    // Create receive-only task
    xTaskCreate(espnow_recv_only_task, "espnow_recv_only", 6144, recv_param, 4, NULL);
    
    ESP_LOGI(TAG, "✅ ESP-NOW started with Device Discovery (Magic: 0x%08lX)", s_discovery_param->magic);
    ESP_LOGI(TAG, "🔍 Device Discovery: Broadcasting every 5 seconds with state=1");
    ESP_LOGI(TAG, "📥 Receive-only task created for processing incoming data");
    
    return ESP_OK;
}

esp_err_t espnow_manager_stop(void)
{
    if (!s_espnow_running) {
        ESP_LOGW(TAG, "ESP-NOW not running");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Stopping ESP-NOW and Device Discovery");
    
    // Signal all tasks to stop
    s_espnow_running = false;
    
    // Give tasks time to cleanup gracefully
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Clean up device discovery resources
    device_discovery_cleanup();
    
    ESP_LOGI(TAG, "ESP-NOW stopped");
    return ESP_OK;
}

esp_err_t espnow_manager_deinit(void)
{
    ESP_LOGI(TAG, "Deinitializing ESP-NOW Manager");
    
    if (s_espnow_running) {
        espnow_manager_stop();
    }
    
    // Clean up remaining resources
    if (s_espnow_queue != NULL) {
        vQueueDelete(s_espnow_queue);
        s_espnow_queue = NULL;
    }
    
    esp_now_deinit();
    
    // Deinitialize TLV storage
    tlv_storage_deinit();
    
    ESP_LOGI(TAG, "ESP-NOW Manager deinitialized");
    return ESP_OK;
}

esp_err_t espnow_manager_get_stats(espnow_stats_t *stats)
{
    if (stats == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memcpy(stats, &s_stats, sizeof(espnow_stats_t));
    return ESP_OK;
}

esp_err_t espnow_manager_get_next_valid_device_index(int current_index, int *next_index)
{
    if (next_index == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (g_tlv_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Take mutex for thread safety
    if (xSemaphoreTake(g_tlv_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    esp_err_t result = ESP_ERR_NOT_FOUND;
    int start_index = (current_index + 1) % MAX_TLV_DEVICES;
    int search_index = start_index;
    
    // Search for next valid device, wrapping around if necessary
    do {
        if (g_tlv_devices[search_index].in_use) {
            *next_index = search_index;
            result = ESP_OK;
            ESP_LOGI(TAG, "📱 Found next valid device at index %d (MAC: " MACSTR ")", 
                     search_index, MAC2STR(g_tlv_devices[search_index].mac_address));
            break;
        }
        search_index = (search_index + 1) % MAX_TLV_DEVICES;
    } while (search_index != start_index);
    
    // If no next device found, try to stay on current device if it's valid
    if (result != ESP_OK && current_index >= 0 && current_index < MAX_TLV_DEVICES) {
        if (g_tlv_devices[current_index].in_use) {
            *next_index = current_index;
            result = ESP_OK;
            ESP_LOGI(TAG, "📱 No next device found, staying on current device at index %d", current_index);
        }
    }
    
    // If still no valid device, default to first available device
    if (result != ESP_OK) {
        for (int i = 0; i < MAX_TLV_DEVICES; i++) {
            if (g_tlv_devices[i].in_use) {
                *next_index = i;
                result = ESP_OK;
                ESP_LOGI(TAG, "📱 No valid device found in sequence, using first available at index %d", i);
                break;
            }
        }
    }
    
    // Release mutex
    xSemaphoreGive(g_tlv_mutex);
    
    if (result != ESP_OK) {
        ESP_LOGW(TAG, "📱 No valid devices found in storage");
        *next_index = 0; // Default fallback
    }
    
    return result;
}

esp_err_t espnow_manager_get_device_info(int device_index, espnow_device_info_t *device_info)
{
    if (device_info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (device_index < 0 || device_index >= MAX_TLV_DEVICES) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (g_tlv_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Take mutex for thread safety
    if (xSemaphoreTake(g_tlv_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    esp_err_t result = ESP_ERR_NOT_FOUND;
    
    // Check if device at index is available and in use
    if (g_tlv_devices[device_index].in_use) {
        const device_tlv_storage_t *device = &g_tlv_devices[device_index];
        
        // Initialize device_info structure
        memset(device_info, 0, sizeof(espnow_device_info_t));
        
        // Copy basic device information
        memcpy(device_info->mac_address, device->mac_address, ESP_NOW_ETH_ALEN);
        strncpy(device_info->device_name, device->device_name, sizeof(device_info->device_name) - 1);
        device_info->is_available = true;
        device_info->last_seen = device->last_seen;
        device_info->entry_count = device->entry_count;
        
        // Initialize with stored RSSI from actual ESP-NOW reception
        device_info->rssi = device->rssi;
        device_info->uptime_seconds = 0;
        device_info->ac_voltage = 0.0f;
        device_info->ac_current = 0.0f;
        device_info->ac_power = 0.0f;
        device_info->ac_power_factor = 0.0f;
        device_info->ac_frequency = 0.0f;
        device_info->status_flags = 0;
        device_info->error_code = 0;
        device_info->temperature = 0.0f;
        device_info->free_memory_kb = 0;
        
        // Parse TLV entries to extract device information
        for (int i = 0; i < MAX_TLV_ENTRIES_PER_DEVICE; i++) {
            const stored_tlv_entry_t *entry = &device->tlv_entries[i];
            if (!entry->valid || entry->length == 0) {
                continue;
            }
            
            switch (entry->type) {
                case TLV_TYPE_UPTIME:
                    if (entry->length == 4) {
                        // Convert from big-endian
                        device_info->uptime_seconds = (entry->value[0] << 24) | 
                                                    (entry->value[1] << 16) | 
                                                    (entry->value[2] << 8) | 
                                                     entry->value[3];
                    }
                    break;
                    
                case TLV_TYPE_DEVICE_ID:
                    if (entry->length > 0 && entry->length < sizeof(device_info->device_id)) {
                        memcpy(device_info->device_id, entry->value, entry->length);
                        device_info->device_id[entry->length] = '\0';
                    }
                    break;
                    
                case TLV_TYPE_FIRMWARE_VER:
                    if (entry->length > 0 && entry->length < sizeof(device_info->firmware_version)) {
                        memcpy(device_info->firmware_version, entry->value, entry->length);
                        device_info->firmware_version[entry->length] = '\0';
                    }
                    break;
                    
                case TLV_TYPE_COMPILE_TIME:
                    if (entry->length > 0 && entry->length < sizeof(device_info->compile_time)) {
                        memcpy(device_info->compile_time, entry->value, entry->length);
                        device_info->compile_time[entry->length] = '\0';
                    }
                    break;
                    
                case TLV_TYPE_AC_VOLTAGE:
                    if (entry->length == 4) {
                        // IEEE 754 float32 in big-endian format
                        uint32_t float_bits = (entry->value[0] << 24) | 
                                            (entry->value[1] << 16) | 
                                            (entry->value[2] << 8) | 
                                             entry->value[3];
                        memcpy(&device_info->ac_voltage, &float_bits, sizeof(float));
                    }
                    break;
                    
                case TLV_TYPE_AC_CURRENT:
                    if (entry->length == 4) {
                        // Convert from milliamperes (int32_t big-endian) to amperes
                        int32_t milliamps = (int32_t)((entry->value[0] << 24) | 
                                                     (entry->value[1] << 16) | 
                                                     (entry->value[2] << 8) | 
                                                      entry->value[3]);
                        device_info->ac_current = milliamps / 1000.0f;
                    }
                    break;
                    
                case TLV_TYPE_AC_POWER:
                    if (entry->length == 4) {
                        // Convert from milliwatts (int32_t big-endian) to watts
                        int32_t milliwatts = (int32_t)((entry->value[0] << 24) | 
                                                      (entry->value[1] << 16) | 
                                                      (entry->value[2] << 8) | 
                                                       entry->value[3]);
                        device_info->ac_power = milliwatts / 1000.0f;
                    }
                    break;
                    
                case TLV_TYPE_AC_POWER_FACTOR:
                    if (entry->length == 4) {
                        // IEEE 754 float32 in big-endian format
                        uint32_t float_bits = (entry->value[0] << 24) | 
                                            (entry->value[1] << 16) | 
                                            (entry->value[2] << 8) | 
                                             entry->value[3];
                        memcpy(&device_info->ac_power_factor, &float_bits, sizeof(float));
                    }
                    break;
                    
                case TLV_TYPE_AC_FREQUENCY:
                    if (entry->length == 4) {
                        // IEEE 754 float32 in big-endian format
                        uint32_t float_bits = (entry->value[0] << 24) | 
                                            (entry->value[1] << 16) | 
                                            (entry->value[2] << 8) | 
                                             entry->value[3];
                        memcpy(&device_info->ac_frequency, &float_bits, sizeof(float));
                    }
                    break;
                    
                case TLV_TYPE_STATUS_FLAGS:
                    if (entry->length == 2) {
                        device_info->status_flags = (entry->value[0] << 8) | entry->value[1];
                    }
                    break;
                    
                case TLV_TYPE_ERROR_CODE:
                    if (entry->length == 2) {
                        device_info->error_code = (entry->value[0] << 8) | entry->value[1];
                    }
                    break;
                    
                case TLV_TYPE_TEMPERATURE:
                    if (entry->length == 4) {
                        // IEEE 754 float32 in big-endian format
                        uint32_t float_bits = (entry->value[0] << 24) | 
                                            (entry->value[1] << 16) | 
                                            (entry->value[2] << 8) | 
                                             entry->value[3];
                        memcpy(&device_info->temperature, &float_bits, sizeof(float));
                    }
                    break;
                    
                case TLV_TYPE_FREE_MEMORY:
                    if (entry->length == 4) {
                        // Free memory in bytes (uint32_t big-endian), convert to KB
                        uint32_t free_memory_bytes = (entry->value[0] << 24) | 
                                                   (entry->value[1] << 16) | 
                                                   (entry->value[2] << 8) | 
                                                    entry->value[3];
                        device_info->free_memory_kb = free_memory_bytes / 1024;
                        ESP_LOGD(TAG, "🧠 Parsed TLV_TYPE_FREE_MEMORY: %u bytes (%u KB)", 
                                free_memory_bytes, device_info->free_memory_kb);
                    }
                    break;
                    
                default:
                    // Unknown TLV type, skip
                    break;
            }
        }
        
        
        result = ESP_OK;
        
        ESP_LOGD(TAG, "📊 Device info retrieved for index %d: MAC=" MACSTR ", entries=%d", 
                device_index, MAC2STR(device_info->mac_address), device_info->entry_count);
    } else {
        ESP_LOGD(TAG, "📊 Device at index %d not available or not in use", device_index);
    }
    
    // Release mutex
    xSemaphoreGive(g_tlv_mutex);
    
    return result;
}

esp_err_t espnow_manager_send_test_packet(void)
{
    if (!s_espnow_running) {
        ESP_LOGW(TAG, "ESP-NOW not running, cannot send test packet");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (s_discovery_task_handle == NULL) {
        ESP_LOGW(TAG, "Device discovery task not running, cannot trigger immediate send");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Notify the device discovery task to send immediately
    ESP_LOGI(TAG, "📤 Triggering immediate device discovery broadcast");
    BaseType_t notify_result = xTaskNotify(s_discovery_task_handle, 0x01, eSetBits);
    
    if (notify_result == pdPASS) {
        ESP_LOGI(TAG, "✅ Discovery task notified successfully");
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "❌ Failed to notify discovery task");
        return ESP_FAIL;
    }
}

// WiFi initialization (matching official example)
static void espnow_wifi_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(ESPNOW_WIFI_MODE));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(CONFIG_ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE));

#if CONFIG_ESPNOW_ENABLE_LONG_RANGE
    ESP_ERROR_CHECK(esp_wifi_set_protocol(ESPNOW_WIFI_IF, WIFI_PROTOCOL_11B|WIFI_PROTOCOL_11G|WIFI_PROTOCOL_11N|WIFI_PROTOCOL_LR));
#endif
    
    ESP_LOGI(TAG, "📶 WiFi initialized for ESP-NOW (Channel: %d)", CONFIG_ESPNOW_CHANNEL);
}

// ESP-NOW send callback (official example)
static void espnow_send_cb(const esp_now_send_info_t *tx_info, esp_now_send_status_t status)
{
    example_espnow_event_t evt;
    example_espnow_event_send_cb_t *send_cb = &evt.info.send_cb;
    
    if (tx_info == NULL) {
        ESP_LOGE(TAG, "Send cb arg error");
        return;
    }
    
    evt.id = EXAMPLE_ESPNOW_SEND_CB;
    memcpy(send_cb->mac_addr, tx_info->des_addr, ESP_NOW_ETH_ALEN);
    send_cb->status = status;
    
    if (s_espnow_queue && xQueueSend(s_espnow_queue, &evt, ESPNOW_MAXDELAY) != pdTRUE) {
        ESP_LOGW(TAG, "Send send queue fail");
    }
    
    // Update statistics
    if (status == ESP_NOW_SEND_SUCCESS) {
        s_stats.send_success++;
    } else {
        s_stats.send_failed++;
    }
}

// ESP-NOW receive callback (official example)
static void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len)
{
    example_espnow_event_t evt;
    example_espnow_event_recv_cb_t *recv_cb = &evt.info.recv_cb;
    uint8_t *mac_addr = recv_info->src_addr;
    uint8_t *des_addr = recv_info->des_addr;
    
    if (mac_addr == NULL || data == NULL || len <= 0) {
        ESP_LOGE(TAG, "Receive cb arg error");
        return;
    }
    // this info is from recv_info->rx_ctrl, upload it through 
    //signed rssi: 8;               /**< Received Signal Strength Indicator(RSSI) of packet. unit: dBm */
    //unsigned rate: 5;             /**< PHY rate encoding of the packet. Only valid for non HT(11bg) packet */
    //unsigned : 1;                 /**< reserved */
    //unsigned sig_mode: 2;         /**< Protocol of the received packet, 0: non HT(11bg) packet; 1: HT(11n) packet; 3: VHT(11ac) packet */
    //unsigned : 16;                /**< reserved */
    //unsigned mcs: 7;              /**< Modulation Coding Scheme. If is HT(11n) packet, shows the modulation, range from 0 to 76(MSC0 ~ MCS76) */
    recv_cb->rssi = recv_info->rx_ctrl->rssi;
    switch (recv_info->rx_ctrl->sig_mode) {
        case 0: // non HT
            // ESP_LOGI(TAG, "Recv non-HT packet, rate=%d, rssi=%d", recv_info->rx_ctrl.rate, recv_info->rx_ctrl.rssi);
            recv_cb->rate_11bg = recv_info->rx_ctrl->rate;
            recv_cb->rate_11n = -1;
            recv_cb->rate_11ac = -1;
            break;
        case 1: // HT
            // ESP_LOGI(TAG, "Recv HT packet, mcs=%d, rssi=%d", recv_info->rx_ctrl.mcs, recv_info->rx_ctrl.rssi);
            recv_cb->rate_11bg = -1;
            recv_cb->rate_11n = recv_info->rx_ctrl->mcs;
            recv_cb->rate_11ac = -1;
            break;
        case 3: // VHT
            recv_cb->rate_11bg = -1;
            recv_cb->rate_11n = -1;
            recv_cb->rate_11ac = recv_info->rx_ctrl->mcs;
            break;
        default:
            // Maybe we should assert here?
            break;
    }
    if (IS_BROADCAST_ADDR(des_addr)) {
        recv_cb->is_broadcast = true;
    } else {
        recv_cb->is_broadcast = false;
    }
    
    evt.id = EXAMPLE_ESPNOW_RECV_CB;
    memcpy(recv_cb->mac_addr, mac_addr, ESP_NOW_ETH_ALEN);
    recv_cb->data = malloc(len);
    if (recv_cb->data == NULL) {
        ESP_LOGE(TAG, "Malloc receive data fail");
        return;
    }
    memcpy(recv_cb->data, data, len);
    recv_cb->data_len = len;
    
    if (s_espnow_queue && xQueueSend(s_espnow_queue, &evt, ESPNOW_MAXDELAY) != pdTRUE) {
        ESP_LOGW(TAG, "Send receive queue fail");
        free(recv_cb->data);
    }
    
    // Update statistics
    s_stats.packets_received++;
    
    // Trigger LED animation on packet reception (rate limited)
    espnow_trigger_led_animation();
    
    // Notify ESP-NOW page of receive statistics update
    espnow_page_notify_data_update();
}

// Data parsing (official example)
// TLV helper functions
static const char* tlv_type_to_string(uint8_t type)
{
    switch (type) {
        // Basic Types (0x00-0x0F)
        case TLV_TYPE_UPTIME:          return "UPTIME";
        case TLV_TYPE_TIMESTAMP:       return "TIMESTAMP";
        case TLV_TYPE_DEVICE_ID:       return "DEVICE_ID";
        case TLV_TYPE_FIRMWARE_VER:    return "FIRMWARE_VER";
        case TLV_TYPE_MAC_ADDRESS:     return "MAC_ADDRESS";
        case TLV_TYPE_COMPILE_TIME:    return "COMPILE_TIME";
        case TLV_TYPE_FREE_MEMORY:     return "FREE_MEMORY";
        
        // Electrical Measurements (0x10-0x2F)
        case TLV_TYPE_AC_VOLTAGE:      return "AC_VOLTAGE";
        case TLV_TYPE_AC_CURRENT:      return "AC_CURRENT";
        case TLV_TYPE_AC_FREQUENCY:    return "AC_FREQUENCY";
        case TLV_TYPE_AC_POWER:        return "AC_POWER";
        case TLV_TYPE_AC_POWER_FACTOR: return "AC_POWER_FACTOR";
        
        // Energy Measurements (0x30-0x4F)
        case TLV_TYPE_ENERGY_TOTAL:    return "ENERGY_TOTAL";
        case TLV_TYPE_ENERGY_TODAY:    return "ENERGY_TODAY";
        
        // Status and Flags (0x50-0x6F)
        case TLV_TYPE_STATUS_FLAGS:    return "STATUS_FLAGS";
        case TLV_TYPE_ERROR_CODE:      return "ERROR_CODE";
        
        // Environmental (0x70-0x8F)
        case TLV_TYPE_TEMPERATURE:     return "TEMPERATURE";
        case TLV_TYPE_HUMIDITY:        return "HUMIDITY";
        
        default:
            if (type >= TLV_TYPE_CUSTOM_START) {
                return "CUSTOM";
            }
            return "UNKNOWN";
    }
}

static int espnow_data_parse(uint8_t *data, uint16_t data_len, uint8_t *state, uint16_t *seq, uint32_t *magic)
{
    if (data == NULL || data_len == 0) {
        ESP_LOGE(TAG, "TLV Parse: Invalid data pointer or length");
        return -1;
    }
    
    ESP_LOGI(TAG, "📊 TLV Data Analysis: Parsing %d bytes", data_len);
    ESP_LOG_BUFFER_HEX_LEVEL(TAG, data, data_len, ESP_LOG_INFO);
    
    // Parse directly as TLV format (no ESP-NOW wrapper)
    ESP_LOGI(TAG, "� Parsing as pure TLV format...");
    
    size_t offset = 0;
    int tlv_count = 0;
    bool valid_tlv_found = false;
    
    while (offset < data_len) {
        // Check if we have at least 2 bytes for type and length
        if (offset + 2 > data_len) {
            ESP_LOGW(TAG, "⚠️ Insufficient data for TLV header at offset %zu", offset);
            break;
        }
        
        uint8_t type = data[offset];
        uint8_t length = data[offset + 1];
        
        // Validate TLV entry bounds
        size_t total_entry_size = TLV_TOTAL_SIZE(length);
        if (offset + total_entry_size > data_len) {
            ESP_LOGE(TAG, "❌ TLV entry exceeds buffer bounds: Entry size: %zu, Remaining buffer: %zu", 
                     total_entry_size, data_len - offset);
            break;
        }
        
        // Parse specific known types and create one-line output
        char value_str[128] = {0};
        if (length > 0) {
            switch (type) {
                case TLV_TYPE_UPTIME:
                    if (length == 4) {
                        uint32_t uptime = TLV_UINT32_FROM_BE(&data[offset + 2]);
                        snprintf(value_str, sizeof(value_str), "Uptime: %" PRIu32 " seconds", uptime);
                    }
                    break;
                    
                case TLV_TYPE_AC_VOLTAGE:
                    if (length == 4) {
                        float voltage;
                        TLV_FLOAT32_FROM_BE(&data[offset + 2], voltage);
                        snprintf(value_str, sizeof(value_str), "AC Voltage: %.1f V", voltage);
                    }
                    break;
                    
                case TLV_TYPE_AC_CURRENT:
                    if (length == 4) {
                        int32_t current_ma = TLV_INT32_FROM_BE(&data[offset + 2]);
                        float current_a = TLV_CURRENT_MA_TO_A(current_ma);
                        snprintf(value_str, sizeof(value_str), "AC Current: %.3f A (%" PRId32 " mA)", current_a, current_ma);
                    }
                    break;
                    
                case TLV_TYPE_AC_FREQUENCY:
                    if (length == 4) {
                        float frequency;
                        TLV_FLOAT32_FROM_BE(&data[offset + 2], frequency);
                        snprintf(value_str, sizeof(value_str), "AC Frequency: %.2f Hz", frequency);
                    }
                    break;
                    
                case TLV_TYPE_AC_POWER:
                    if (length == 4) {
                        int32_t power_mw = TLV_INT32_FROM_BE(&data[offset + 2]);
                        float power_w = TLV_POWER_MW_TO_W(power_mw);
                        snprintf(value_str, sizeof(value_str), "AC Power: %.3f W (%" PRId32 " mW)", power_w, power_mw);
                    }
                    break;
                    
                case TLV_TYPE_DEVICE_ID:
                case TLV_TYPE_FIRMWARE_VER:
                case TLV_TYPE_COMPILE_TIME:
                    // String types - display as text if printable
                    {
                        char temp_str[65] = {0}; // Max 64 chars + null terminator
                        int copy_len = (length < 64) ? length : 64;
                        memcpy(temp_str, &data[offset + 2], copy_len);
                        snprintf(value_str, sizeof(value_str), "Text: \"%s\"", temp_str);
                    }
                    break;
                    
                case TLV_TYPE_MAC_ADDRESS:
                    if (length == 6) {
                        snprintf(value_str, sizeof(value_str), "MAC: " MACSTR, MAC2STR(&data[offset + 2]));
                    }
                    break;
                    
                case TLV_TYPE_STATUS_FLAGS:
                    if (length == 2) {
                        uint16_t flags = TLV_UINT16_FROM_BE(&data[offset + 2]);
                        char flag_details[64] = {0};
                        if (flags & STATUS_FLAG_POWER_ON) strcat(flag_details, "PWR ");
                        if (flags & STATUS_FLAG_WIFI_CONNECTED) strcat(flag_details, "WIFI ");
                        if (flags & STATUS_FLAG_ESP_NOW_ACTIVE) strcat(flag_details, "ESPNOW ");
                        if (flags & STATUS_FLAG_ERROR) strcat(flag_details, "ERR ");
                        snprintf(value_str, sizeof(value_str), "Status Flags: 0x%04X (%s)", flags, flag_details);
                    }
                    break;
                    
                default:
                    snprintf(value_str, sizeof(value_str), "Raw data (%d bytes)", length);
                    break;
            }
        } else {
            snprintf(value_str, sizeof(value_str), "(empty)");
        }
        
        // Single line output for each TLV entry
        ESP_LOGI(TAG, "📋 TLV #%d @%zu: Type=0x%02X (%s), Len=%d, %s", 
                 tlv_count + 1, offset, type, tlv_type_to_string(type), length, value_str);
        
        // Move to next TLV entry
        offset += total_entry_size;
        tlv_count++;
        valid_tlv_found = true;
        
        // Safety check to prevent infinite loops
        if (tlv_count > 100) {
            ESP_LOGW(TAG, "⚠️ Maximum TLV entry limit reached (100), stopping parse");
            break;
        }
    }
    
    if (valid_tlv_found) {
        ESP_LOGI(TAG, "✅ TLV Format: Successfully parsed %d TLV entries", tlv_count);
        
        // For TLV format, we don't have traditional state/seq/magic
        if (state) *state = 0;
        if (seq) *seq = 0;
        if (magic) *magic = 0;
        
        return tlv_count; // Return number of TLV entries found
    } else {
        ESP_LOGW(TAG, "❌ No valid TLV format detected in data");
        ESP_LOGW(TAG, "    Buffer may contain raw data or unknown format");
        return -1;
    }
}

// Data preparation (official example)
// ===== DEVICE DISCOVERY TASK IMPLEMENTATION =====

/**
 * @brief Prepare discovery broadcast data (simplified version)
 * 
 * Always sends broadcast with state=1 and incrementing sequence number
 */
static void device_discovery_data_prepare(device_discovery_param_t *param)
{
    example_espnow_data_t *buf = (example_espnow_data_t *)param->buffer;
    
    if (param->len < sizeof(example_espnow_data_t)) {
        ESP_LOGE(TAG, "Discovery buffer too small");
        return;
    }
    
    buf->type = EXAMPLE_ESPNOW_DATA_BROADCAST;
    buf->state = 1;  // Always 1 for device discovery
    buf->seq_num = s_espnow_seq[EXAMPLE_ESPNOW_DATA_BROADCAST]++;
    buf->crc = 0;
    buf->magic = param->magic;
    
    // Fill payload with random data
    esp_fill_random(buf->payload, param->len - sizeof(example_espnow_data_t));
    
    // Calculate CRC
    buf->crc = esp_crc16_le(UINT16_MAX, (uint8_t const *)buf, param->len);
    
    ESP_LOGD(TAG, "🔧 Discovery data prepared: state=1, seq=%d, magic=0x%08lX", 
             buf->seq_num, buf->magic);
}

/**
 * @brief Device discovery task - sends broadcast every 5 seconds
 * 
 * This task continuously sends ESP-NOW broadcast packets with:
 * - Fixed 5-second interval after last send completion
 * - State always set to 1
 * - Incrementing sequence numbers
 */
static void device_discovery_task(void *pvParameter)
{
    device_discovery_param_t *param = (device_discovery_param_t *)pvParameter;
    const uint32_t DISCOVERY_INTERVAL_MS = 5000;  // 5 seconds
    
    ESP_LOGI(TAG, "🔍 Device Discovery Task started");
    ESP_LOGI(TAG, "🔍 Broadcasting every %lu seconds with state=1", DISCOVERY_INTERVAL_MS / 1000);
    
    // Initial delay before first broadcast
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    while (s_espnow_running) {
        // Prepare discovery data
        device_discovery_data_prepare(param);
        
        // Send broadcast
        ESP_LOGI(TAG, "📡 Sending device discovery broadcast (state=1)...");
        param->send_completed = false;
        
        esp_err_t ret = esp_now_send(s_broadcast_mac, param->buffer, param->len);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "❌ Discovery send failed: %s", esp_err_to_name(ret));
            s_stats.send_failed++;
        } else {
            ESP_LOGD(TAG, "📤 Discovery send initiated successfully");
        }
        
        // Wait for send completion or timeout
        TickType_t send_start_time = xTaskGetTickCount();
        const TickType_t SEND_TIMEOUT_MS = 1000;  // 1 second timeout
        
        while (!param->send_completed && 
               (xTaskGetTickCount() - send_start_time) < pdMS_TO_TICKS(SEND_TIMEOUT_MS)) {
            vTaskDelay(pdMS_TO_TICKS(10));  // Small delay to check completion
        }
        
        if (param->send_completed) {
            ESP_LOGI(TAG, "✅ Discovery broadcast completed successfully");
            param->last_send_time = xTaskGetTickCount();
        } else {
            ESP_LOGW(TAG, "⏰ Discovery send timeout (assuming completed)");
            param->last_send_time = xTaskGetTickCount();
        }
        
        // Wait for next discovery interval or immediate trigger
        ESP_LOGI(TAG, "⏱️ Waiting %lu seconds until next discovery broadcast (or immediate trigger)...", DISCOVERY_INTERVAL_MS / 1000);
        
        // Use task notification with timeout for DISCOVERY_INTERVAL_MS
        uint32_t notification_value;
        BaseType_t notify_result = xTaskNotifyWait(
            0x00,      // Don't clear any notification bits on entry
            0xFFFFFFFF, // Clear all notification bits on exit
            &notification_value,
            pdMS_TO_TICKS(DISCOVERY_INTERVAL_MS)  // Wait timeout
        );
        
        if (notify_result == pdTRUE) {
            ESP_LOGI(TAG, "🚀 Immediate discovery trigger received (notification: 0x%08lX)", notification_value);
        } else {
            ESP_LOGD(TAG, "⏰ Discovery interval timeout - proceeding with next broadcast");
        }
    }
    
    ESP_LOGI(TAG, "🔍 Device Discovery Task ending");
    vTaskDelete(NULL);
}

/**
 * @brief Clean up device discovery resources
 */
static void device_discovery_cleanup(void)
{
    if (s_discovery_param) {
        if (s_discovery_param->buffer) {
            free(s_discovery_param->buffer);
        }
        free(s_discovery_param);
        s_discovery_param = NULL;
    }
    s_discovery_task_handle = NULL;
    ESP_LOGI(TAG, "🧹 Device discovery resources cleaned up");
}

/**
 * @brief Receive-only ESP-NOW task (no sending, only processing received data)
 * 
 * This task only processes ESP-NOW receive events without sending any data.
 * All sending is handled by the device_discovery_task.
 */
static void espnow_recv_only_task(void *pvParameter)
{
    example_espnow_event_t evt;
    uint8_t recv_state = 0;
    uint16_t recv_seq = 0;
    uint32_t recv_magic = 0;
    
    example_espnow_send_param_t *recv_param = (example_espnow_send_param_t *)pvParameter;
    
    ESP_LOGI(TAG, "📥 ESP-NOW Receive-only task started (Magic: 0x%08lX)", recv_param->magic);
    
    while (s_espnow_running && xQueueReceive(s_espnow_queue, &evt, portMAX_DELAY) == pdTRUE) {
        switch (evt.id) {
            case EXAMPLE_ESPNOW_SEND_CB:
            {
                example_espnow_event_send_cb_t *send_cb = &evt.info.send_cb;
                
                // Check if this is a broadcast to our broadcast MAC (device discovery)
                bool is_discovery_broadcast = (memcmp(send_cb->mac_addr, s_broadcast_mac, ESP_NOW_ETH_ALEN) == 0);
                
                if (is_discovery_broadcast && s_discovery_param) {
                    // Notify device discovery task that send completed
                    s_discovery_param->send_completed = true;
                    ESP_LOGD(TAG, "🔍 Discovery send callback: %s", 
                             (send_cb->status == ESP_NOW_SEND_SUCCESS) ? "SUCCESS" : "FAILED");
                }
                
                // Update send statistics
                if (send_cb->status == ESP_NOW_SEND_SUCCESS) {
                    s_stats.packets_sent++;
                    s_stats.send_success++;
                } else {
                    s_stats.send_failed++;
                }
                
                // Notify ESP-NOW page of send statistics update
                espnow_page_notify_data_update();
                
                ESP_LOGD(TAG, "📤 Send callback: "MACSTR", status: %d", 
                         MAC2STR(send_cb->mac_addr), send_cb->status);
                break;
            }
            case EXAMPLE_ESPNOW_RECV_CB:
            {
                example_espnow_event_recv_cb_t *recv_cb = &evt.info.recv_cb;
                
                // Print raw data for debugging
                ESP_LOGI(TAG, "📦 Raw data from "MACSTR" (len=%d):", MAC2STR(recv_cb->mac_addr), recv_cb->data_len);
                ESP_LOGI(TAG, "   Received via %s", recv_cb->is_broadcast ? "BROADCAST" : "UNICAST");
                ESP_LOGI(TAG, "   rssi: %d dBm, 11bg: %d, 11n: %d, 11ac: %d", recv_cb->rssi, recv_cb->rate_11bg, recv_cb->rate_11n, recv_cb->rate_11ac);
                ESP_LOG_BUFFER_HEX(TAG, recv_cb->data, recv_cb->data_len);
                
                int parse_result = espnow_data_parse(recv_cb->data, recv_cb->data_len, &recv_state, &recv_seq, &recv_magic);
                
                // If TLV data was successfully parsed, store it indexed by MAC address
                if (parse_result > 0) {
                    ESP_LOGI(TAG, "✅ TLV data parsed successfully (%d entries), storing for device " MACSTR, 
                             parse_result, MAC2STR(recv_cb->mac_addr));
                    
                    // Process and store the TLV data using MAC address as index (with RSSI)
                    process_received_tlv_data(recv_cb->mac_addr, recv_cb->data, recv_cb->data_len, recv_cb->rssi);
                } else {
                    ESP_LOGW(TAG, "⚠️ TLV parsing failed or no valid TLV data found");
                }
                
                free(recv_cb->data);
                
                // Notify ESP-NOW page of updates
                espnow_page_notify_data_update();
                break;
            }
            default:
                ESP_LOGE(TAG, "Callback type error: %d", evt.id);
                break;
        }
    }
    
    ESP_LOGI(TAG, "📥 ESP-NOW Receive-only task ending");
    if (recv_param) {
        free(recv_param);
    }
    vTaskDelete(NULL);
}

// ===== LED ANIMATION INTEGRATION =====

/**
 * @brief Trigger LED animation with rate limiting
 * 
 * Sends a LED animation request to ux_service when ESP-NOW packets are received.
 * Rate limited to once per second to avoid excessive blinking.
 */
static void espnow_trigger_led_animation(void)
{
    static TickType_t s_last_led_animation_time = 0;  // Function-local static variable
    TickType_t current_time = xTaskGetTickCount();
    TickType_t time_diff = current_time - s_last_led_animation_time;
    
    // Check if enough time has passed since last animation (1 second)
    if (time_diff >= pdMS_TO_TICKS(LED_ANIMATION_INTERVAL_MS)) {
        // Send LED blink animation to UX service
        esp_err_t ret = ux_led_blink_fast(500);  // Fast blink for 500ms
        if (ret == ESP_OK) {
            s_last_led_animation_time = current_time;
            ESP_LOGD(TAG, "🔴 LED animation triggered on ESP-NOW packet reception");
        } else {
            ESP_LOGW(TAG, "Failed to trigger LED animation: %s", esp_err_to_name(ret));
        }
    } else {
        // Rate limited - too soon since last animation
        uint32_t remaining_ms = pdTICKS_TO_MS(pdMS_TO_TICKS(LED_ANIMATION_INTERVAL_MS) - time_diff);
        ESP_LOGD(TAG, "🔴 LED animation rate limited (wait %lu ms)", remaining_ms);
    }
}

// ===== TLV DEVICE STORAGE IMPLEMENTATION =====

/**
 * @brief Initialize TLV device storage system
 */
static esp_err_t tlv_storage_init(void)
{
    ESP_LOGI(TAG, "🗂️ Initializing TLV device storage");
    
    // Create mutex for thread-safe access
    g_tlv_mutex = xSemaphoreCreateMutex();
    if (g_tlv_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create TLV storage mutex");
        return ESP_FAIL;
    }
    
    // Initialize all device storage slots
    for (int i = 0; i < MAX_TLV_DEVICES; i++) {
        memset(&g_tlv_devices[i], 0, sizeof(device_tlv_storage_t));
        g_tlv_devices[i].in_use = false;
        g_tlv_devices[i].entry_count = 0;
        
        // Initialize all TLV entries as invalid
        for (int j = 0; j < MAX_TLV_ENTRIES_PER_DEVICE; j++) {
            g_tlv_devices[i].tlv_entries[j].valid = false;
        }
    }
    
    ESP_LOGI(TAG, "✅ TLV storage initialized (max %d devices, %d entries each)", 
             MAX_TLV_DEVICES, MAX_TLV_ENTRIES_PER_DEVICE);
    return ESP_OK;
}

/**
 * @brief Deinitialize TLV device storage system
 */
static void tlv_storage_deinit(void)
{
    ESP_LOGI(TAG, "🗂️ Deinitializing TLV device storage");
    
    if (g_tlv_mutex != NULL) {
        vSemaphoreDelete(g_tlv_mutex);
        g_tlv_mutex = NULL;
    }
    
    // Clear all storage
    memset(g_tlv_devices, 0, sizeof(g_tlv_devices));
    
    ESP_LOGI(TAG, "✅ TLV storage deinitialized");
}

/**
 * @brief Find device storage by MAC address
 * @param mac_addr MAC address to search for
 * @return Pointer to device storage or NULL if not found
 */
static device_tlv_storage_t* find_device_by_mac(const uint8_t *mac_addr)
{
    if (mac_addr == NULL) {
        return NULL;
    }
    
    for (int i = 0; i < MAX_TLV_DEVICES; i++) {
        if (g_tlv_devices[i].in_use && 
            memcmp(g_tlv_devices[i].mac_address, mac_addr, ESP_NOW_ETH_ALEN) == 0) {
            return &g_tlv_devices[i];
        }
    }
    
    return NULL;
}

/**
 * @brief Get existing device or create new device storage slot
 * @param mac_addr MAC address of the device
 * @return Pointer to device storage or NULL if no space available
 */
static device_tlv_storage_t* get_or_create_device(const uint8_t *mac_addr)
{
    if (mac_addr == NULL) {
        return NULL;
    }
    
    // First try to find existing device
    device_tlv_storage_t *device = find_device_by_mac(mac_addr);
    if (device != NULL) {
        return device;
    }
    
    // Look for an empty slot
    for (int i = 0; i < MAX_TLV_DEVICES; i++) {
        if (!g_tlv_devices[i].in_use) {
            // Initialize new device slot
            memset(&g_tlv_devices[i], 0, sizeof(device_tlv_storage_t));
            memcpy(g_tlv_devices[i].mac_address, mac_addr, ESP_NOW_ETH_ALEN);
            g_tlv_devices[i].in_use = true;
            g_tlv_devices[i].entry_count = 0;
            g_tlv_devices[i].last_seen = xTaskGetTickCount();
            g_tlv_devices[i].rssi = -100; // Initialize with weak signal until actual reception
            
            // Initialize all TLV entries as invalid
            for (int j = 0; j < MAX_TLV_ENTRIES_PER_DEVICE; j++) {
                g_tlv_devices[i].tlv_entries[j].valid = false;
            }
            
            // Generate friendly device name
            snprintf(g_tlv_devices[i].device_name, sizeof(g_tlv_devices[i].device_name),
                     "ESP-" MACSTR, MAC2STR(mac_addr));
            
            ESP_LOGI(TAG, "📋 Created new device storage: %s", g_tlv_devices[i].device_name);
            return &g_tlv_devices[i];
        }
    }
    
    ESP_LOGW(TAG, "⚠️ No space available for new device " MACSTR, MAC2STR(mac_addr));
    return NULL;
}

/**
 * @brief Store TLV data for a specific device
 * @param mac_addr MAC address of the device
 * @param tlv_data TLV data buffer
 * @param data_len Length of TLV data
 * @param rssi RSSI value from ESP-NOW reception
 * @return ESP_OK on success, ESP_FAIL on error
 */
static esp_err_t store_device_tlv_data(const uint8_t *mac_addr, const uint8_t *tlv_data, size_t data_len, int8_t rssi)
{
    if (mac_addr == NULL || tlv_data == NULL || data_len == 0 || g_tlv_mutex == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Take mutex for thread safety
    if (xSemaphoreTake(g_tlv_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take TLV storage mutex");
        return ESP_ERR_TIMEOUT;
    }
    
    esp_err_t result = ESP_OK;
    
    do {
        // Get or create device storage
        device_tlv_storage_t *device = get_or_create_device(mac_addr);
        if (device == NULL) {
            ESP_LOGE(TAG, "Failed to get device storage for " MACSTR, MAC2STR(mac_addr));
            result = ESP_FAIL;
            break;
        }
        
        // Update last seen timestamp and RSSI
        device->last_seen = xTaskGetTickCount();
        device->rssi = rssi;  // Store the actual RSSI from ESP-NOW reception
        
        // Parse TLV data and store individual entries
        size_t offset = 0;
        int stored_entries = 0;
        
        while (offset < data_len && stored_entries < MAX_TLV_ENTRIES_PER_DEVICE) {
            // Check if we have at least 2 bytes for type and length
            if (offset + 2 > data_len) {
                ESP_LOGW(TAG, "Insufficient data for TLV header at offset %zu", offset);
                break;
            }
            
            uint8_t type = tlv_data[offset];
            uint8_t length = tlv_data[offset + 1];
            
            // Validate TLV entry bounds
            size_t total_entry_size = TLV_TOTAL_SIZE(length);
            if (offset + total_entry_size > data_len) {
                ESP_LOGE(TAG, "TLV entry exceeds buffer bounds");
                break;
            }
            
            // Check if value fits in our storage
            if (length > MAX_TLV_ENTRY_VALUE_SIZE) {
                ESP_LOGW(TAG, "TLV value too large (type=0x%02X, len=%d), skipping", type, length);
                offset += total_entry_size;
                continue;
            }
            
            // Find existing entry with same type or get empty slot
            stored_tlv_entry_t *entry = NULL;
            
            // First, look for existing entry with same type (update)
            for (int i = 0; i < MAX_TLV_ENTRIES_PER_DEVICE; i++) {
                if (device->tlv_entries[i].valid && device->tlv_entries[i].type == type) {
                    entry = &device->tlv_entries[i];
                    break;
                }
            }
            
            // If not found, look for empty slot (new entry)
            if (entry == NULL) {
                for (int i = 0; i < MAX_TLV_ENTRIES_PER_DEVICE; i++) {
                    if (!device->tlv_entries[i].valid) {
                        entry = &device->tlv_entries[i];
                        device->entry_count++;
                        break;
                    }
                }
            }
            
            if (entry == NULL) {
                ESP_LOGW(TAG, "No space for TLV type 0x%02X", type);
                offset += total_entry_size;
                continue;
            }
            
            // Store TLV entry
            entry->type = type;
            entry->length = length;
            if (length > 0) {
                memcpy(entry->value, &tlv_data[offset + 2], length);
            }
            entry->last_updated = xTaskGetTickCount();
            entry->valid = true;
            
            stored_entries++;
            offset += total_entry_size;
        }
        
        ESP_LOGI(TAG, "📊 Stored %d TLV entries for device %s (total: %d)", 
                 stored_entries, device->device_name, device->entry_count);
        
    } while (0);
    
    // Release mutex
    xSemaphoreGive(g_tlv_mutex);
    
    return result;
}

/**
 * @brief Print detailed TLV information for a device
 * @param device Pointer to device storage structure
 */
static void print_device_tlv_info(const device_tlv_storage_t *device)
{
    if (device == NULL || !device->in_use) {
        return;
    }
    
    ESP_LOGI(TAG, "🔍 Device TLV Info: %s (" MACSTR ")", 
             device->device_name, MAC2STR(device->mac_address));
    ESP_LOGI(TAG, "   Last seen: %lu ticks ago", xTaskGetTickCount() - device->last_seen);
    ESP_LOGI(TAG, "   TLV entries: %d/%d", device->entry_count, MAX_TLV_ENTRIES_PER_DEVICE);
    
    int valid_entries = 0;
    for (int i = 0; i < MAX_TLV_ENTRIES_PER_DEVICE; i++) {
        if (device->tlv_entries[i].valid) {
            const stored_tlv_entry_t *entry = &device->tlv_entries[i];
            
            // Create one-line value description
            char value_str[128] = {0};
            if (entry->length > 0) {
                switch (entry->type) {
                    case TLV_TYPE_UPTIME:
                        if (entry->length == 4) {
                            uint32_t uptime = TLV_UINT32_FROM_BE(entry->value);
                            snprintf(value_str, sizeof(value_str), "Uptime: %" PRIu32 " seconds", uptime);
                        }
                        break;
                        
                    case TLV_TYPE_AC_VOLTAGE:
                        if (entry->length == 4) {
                            float voltage;
                            TLV_FLOAT32_FROM_BE(entry->value, voltage);
                            snprintf(value_str, sizeof(value_str), "AC Voltage: %.1f V", voltage);
                        }
                        break;
                        
                    case TLV_TYPE_AC_CURRENT:
                        if (entry->length == 4) {
                            int32_t current_ma = TLV_INT32_FROM_BE(entry->value);
                            float current_a = TLV_CURRENT_MA_TO_A(current_ma);
                            snprintf(value_str, sizeof(value_str), "AC Current: %.3f A (%" PRId32 " mA)", current_a, current_ma);
                        }
                        break;
                        
                    case TLV_TYPE_AC_FREQUENCY:
                        if (entry->length == 4) {
                            float frequency;
                            TLV_FLOAT32_FROM_BE(entry->value, frequency);
                            snprintf(value_str, sizeof(value_str), "AC Frequency: %.2f Hz", frequency);
                        }
                        break;
                        
                    case TLV_TYPE_AC_POWER:
                        if (entry->length == 4) {
                            int32_t power_mw = TLV_INT32_FROM_BE(entry->value);
                            float power_w = TLV_POWER_MW_TO_W(power_mw);
                            snprintf(value_str, sizeof(value_str), "AC Power: %.3f W (%" PRId32 " mW)", power_w, power_mw);
                        }
                        break;
                        
                    case TLV_TYPE_DEVICE_ID:
                    case TLV_TYPE_FIRMWARE_VER:
                    case TLV_TYPE_COMPILE_TIME:
                        // String types
                        {
                            char temp_str[65] = {0};
                            int copy_len = (entry->length < 64) ? entry->length : 64;
                            memcpy(temp_str, entry->value, copy_len);
                            snprintf(value_str, sizeof(value_str), "Text: \"%s\"", temp_str);
                        }
                        break;
                        
                    case TLV_TYPE_MAC_ADDRESS:
                        if (entry->length == 6) {
                            snprintf(value_str, sizeof(value_str), "MAC: " MACSTR, MAC2STR(entry->value));
                        }
                        break;
                        
                    case TLV_TYPE_STATUS_FLAGS:
                        if (entry->length == 2) {
                            uint16_t flags = TLV_UINT16_FROM_BE(entry->value);
                            char flag_details[64] = {0};
                            if (flags & STATUS_FLAG_POWER_ON) strcat(flag_details, "PWR ");
                            if (flags & STATUS_FLAG_WIFI_CONNECTED) strcat(flag_details, "WIFI ");
                            if (flags & STATUS_FLAG_ESP_NOW_ACTIVE) strcat(flag_details, "ESPNOW ");
                            if (flags & STATUS_FLAG_ERROR) strcat(flag_details, "ERR ");
                            snprintf(value_str, sizeof(value_str), "Status Flags: 0x%04X (%s)", flags, flag_details);
                        }
                        break;
                        
                    default:
                        snprintf(value_str, sizeof(value_str), "Raw data (%d bytes)", entry->length);
                        break;
                }
            } else {
                snprintf(value_str, sizeof(value_str), "(empty)");
            }
            
            // Single line output for each stored TLV entry
            ESP_LOGI(TAG, "   [%d] Type=0x%02X (%s), Len=%d, %s", 
                     valid_entries++,
                     entry->type, 
                     tlv_type_to_string(entry->type), 
                     entry->length,
                     value_str);
        }
    }
}

/**
 * @brief Process received TLV data and store it indexed by MAC address
 * @param mac_addr MAC address of sender
 * @param data Raw received data
 * @param data_len Length of received data
 */
static void process_received_tlv_data(const uint8_t *mac_addr, const uint8_t *data, size_t data_len, int8_t rssi)
{
    if (mac_addr == NULL || data == NULL || data_len == 0) {
        return;
    }
    
    ESP_LOGI(TAG, "🗂️ Processing TLV data from " MACSTR " (%zu bytes)", 
             MAC2STR(mac_addr), data_len);
    
    // Store the TLV data for this device (including RSSI)
    esp_err_t ret = store_device_tlv_data(mac_addr, data, data_len, rssi);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ TLV data stored successfully");
        
        // Print device information
        if (g_tlv_mutex != NULL && 
            xSemaphoreTake(g_tlv_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
            
            device_tlv_storage_t *device = find_device_by_mac(mac_addr);
            if (device != NULL) {
                print_device_tlv_info(device);
            }
            
            xSemaphoreGive(g_tlv_mutex);
        }
    } else {
        ESP_LOGE(TAG, "❌ Failed to store TLV data: %s", esp_err_to_name(ret));
    }
}