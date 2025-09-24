#ifndef ESPNOW_MANAGER_H
#define ESPNOW_MANAGER_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief ESP-NOW statistics structure
 */
typedef struct {
    uint32_t packets_sent;
    uint32_t packets_received;
    uint32_t send_success;
    uint32_t send_failed;
    uint32_t magic_number;
    uint8_t peer_mac[6];
    bool is_sender;
    bool is_connected;
    uint32_t last_recv_time;    // Timestamp of last received packet
    uint32_t last_seq_num;      // Last received sequence number for restart detection
} espnow_stats_t;

/**
 * @brief ESP-NOW device information structure (extracted from TLV data)
 */
typedef struct {
    // Device identification
    uint8_t mac_address[6];         // Device MAC address
    char device_name[32];           // Device friendly name
    bool is_available;              // Whether device data is available
    uint32_t last_seen;             // Last time device was seen (in ticks)
    uint16_t entry_count;           // Number of TLV entries for this device
    
    // Network information
    int rssi;                       // Signal strength (from ESP-NOW rx_ctrl)
    
    // System information (parsed from TLV entries)
    uint32_t uptime_seconds;        // TLV_TYPE_UPTIME
    char device_id[32];             // TLV_TYPE_DEVICE_ID (max 32 + null)
    char firmware_version[17];      // TLV_TYPE_FIRMWARE_VER (max 16 + null)
    char compile_time[33];          // TLV_TYPE_COMPILE_TIME (max 32 + null)
    
    // Electrical measurements (parsed from TLV entries)
    float ac_voltage;               // TLV_TYPE_AC_VOLTAGE (volts)
    float ac_current;               // TLV_TYPE_AC_CURRENT (amperes, converted from milliamperes)
    float ac_power;                 // TLV_TYPE_AC_POWER (watts, converted from milliwatts)
    float ac_power_factor;          // TLV_TYPE_AC_POWER_FACTOR
    float ac_frequency;             // TLV_TYPE_AC_FREQUENCY (hertz)
    
    // Status and error information
    uint16_t status_flags;          // TLV_TYPE_STATUS_FLAGS
    uint16_t error_code;            // TLV_TYPE_ERROR_CODE
    
    // Environmental data
    float temperature;              // TLV_TYPE_TEMPERATURE (celsius)
    
    // Memory information (derived/calculated)
    uint32_t free_memory_kb;        // Free memory in KB (calculated or estimated)
} espnow_device_info_t;

/**
 * @brief Initialize ESP-NOW manager (follows official example pattern)
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t espnow_manager_init(void);

/**
 * @brief Start ESP-NOW communication (official example logic)
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t espnow_manager_start(void);

/**
 * @brief Stop ESP-NOW communication
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t espnow_manager_stop(void);

/**
 * @brief Deinitialize ESP-NOW manager
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t espnow_manager_deinit(void);

/**
 * @brief Get ESP-NOW statistics
 * 
 * @param stats Pointer to statistics structure to fill
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t espnow_manager_get_stats(espnow_stats_t *stats);

/**
 * @brief Get ESP-NOW device information by index
 * 
 * @param device_index Index of device to retrieve (0-based, typically 0 for first device)
 * @param device_info Pointer to device information structure to fill
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if device not available, error code otherwise
 */
esp_err_t espnow_manager_get_device_info(int device_index, espnow_device_info_t *device_info);

/**
 * @brief Get the next valid device index in circular fashion
 * 
 * @param current_index Current device index
 * @param next_index Pointer to store the next valid device index
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if no valid devices available
 */
esp_err_t espnow_manager_get_next_valid_device_index(int current_index, int *next_index);

/**
 * @brief Send a test packet manually
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t espnow_manager_send_test_packet(void);

#ifdef __cplusplus
}
#endif

#endif // ESPNOW_MANAGER_H