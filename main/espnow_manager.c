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

// Forward declarations
static void espnow_wifi_init(void);
static void espnow_send_cb(const esp_now_send_info_t *tx_info, esp_now_send_status_t status);
static void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len);
static int espnow_data_parse(uint8_t *data, uint16_t data_len, uint8_t *state, uint16_t *seq, uint32_t *magic);

// TLV helper functions
static const char* tlv_type_to_string(uint8_t type);

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
        
        ESP_LOGI(TAG, "📋 TLV Entry #%d at offset %zu:", tlv_count + 1, offset);
        ESP_LOGI(TAG, "   Type: 0x%02X (%s)", type, tlv_type_to_string(type));
        ESP_LOGI(TAG, "   Length: %d bytes", length);
        
        // Validate TLV entry bounds
        size_t total_entry_size = TLV_TOTAL_SIZE(length);
        if (offset + total_entry_size > data_len) {
            ESP_LOGE(TAG, "❌ TLV entry exceeds buffer bounds:");
            ESP_LOGE(TAG, "   Entry size: %zu, Remaining buffer: %zu", 
                     total_entry_size, data_len - offset);
            break;
        }
        
        // Display value content if length > 0
        if (length > 0) {
            ESP_LOGI(TAG, "   Value (%d bytes):", length);
            ESP_LOG_BUFFER_HEX_LEVEL(TAG, &data[offset + 2], length, ESP_LOG_INFO);
            
            // Parse specific known types
            switch (type) {
                case TLV_TYPE_UPTIME:
                    if (length == 4) {
                        uint32_t uptime = TLV_UINT32_FROM_BE(&data[offset + 2]);
                        ESP_LOGI(TAG, "   -> Uptime: %lu seconds", uptime);
                    }
                    break;
                    
                case TLV_TYPE_AC_VOLTAGE:
                    if (length == 4) {
                        float voltage;
                        TLV_FLOAT32_FROM_BE(&data[offset + 2], voltage);
                        ESP_LOGI(TAG, "   -> AC Voltage: %.1f V", voltage);
                    }
                    break;
                    
                case TLV_TYPE_AC_CURRENT:
                    if (length == 4) {
                        int32_t current_ma = TLV_INT32_FROM_BE(&data[offset + 2]);
                        float current_a = TLV_CURRENT_MA_TO_A(current_ma);
                        ESP_LOGI(TAG, "   -> AC Current: %.3f A (%ld mA)", current_a, current_ma);
                    }
                    break;
                    
                case TLV_TYPE_AC_FREQUENCY:
                    if (length == 4) {
                        float frequency;
                        TLV_FLOAT32_FROM_BE(&data[offset + 2], frequency);
                        ESP_LOGI(TAG, "   -> AC Frequency: %.2f Hz", frequency);
                    }
                    break;
                    
                case TLV_TYPE_AC_POWER:
                    if (length == 4) {
                        int32_t power_mw = TLV_INT32_FROM_BE(&data[offset + 2]);
                        float power_w = TLV_POWER_MW_TO_W(power_mw);
                        ESP_LOGI(TAG, "   -> AC Power: %.3f W (%ld mW)", power_w, power_mw);
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
                        ESP_LOGI(TAG, "   -> Text: \"%s\"", temp_str);
                    }
                    break;
                    
                case TLV_TYPE_MAC_ADDRESS:
                    if (length == 6) {
                        ESP_LOGI(TAG, "   -> MAC: " MACSTR, MAC2STR(&data[offset + 2]));
                    }
                    break;
                    
                case TLV_TYPE_STATUS_FLAGS:
                    if (length == 2) {
                        uint16_t flags = TLV_UINT16_FROM_BE(&data[offset + 2]);
                        ESP_LOGI(TAG, "   -> Status Flags: 0x%04X", flags);
                        if (flags & STATUS_FLAG_POWER_ON) ESP_LOGI(TAG, "      - POWER_ON");
                        if (flags & STATUS_FLAG_WIFI_CONNECTED) ESP_LOGI(TAG, "      - WIFI_CONNECTED");
                        if (flags & STATUS_FLAG_ESP_NOW_ACTIVE) ESP_LOGI(TAG, "      - ESP_NOW_ACTIVE");
                        if (flags & STATUS_FLAG_ERROR) ESP_LOGI(TAG, "      - ERROR");
                    }
                    break;
                    
                default:
                    ESP_LOGI(TAG, "   -> Raw data (type not specifically parsed)");
                    break;
            }
        } else {
            ESP_LOGI(TAG, "   Value: (empty)");
        }
        
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
    int ret;
    
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
                
                ret = espnow_data_parse(recv_cb->data, recv_cb->data_len, &recv_state, &recv_seq, &recv_magic);
                
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