/*
 * System Monitor for M5StickC Plus 1.1
 * Collects battery, voltage, temperature and other system data
 */

#ifndef SYSTEM_MONITOR_H
#define SYSTEM_MONITOR_H

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// System data structure
typedef struct {
    // Battery information
    float battery_voltage;      // Battery voltage in V
    uint8_t battery_percentage; // Battery percentage 0-100%
    bool is_charging;          // Charging status
    bool is_usb_connected;     // USB connection status
    float charge_current;      // Charging current in mA
    float discharge_current;   // Discharge current in mA
    
    // Power information  
    float vbus_voltage;        // USB voltage in V
    
    // Temperature
    float internal_temp;       // AXP192 internal temperature in °C
    
    // System information
    uint32_t uptime_seconds;   // System uptime in seconds
    uint32_t free_heap;        // Free heap memory in bytes
    uint32_t min_free_heap;    // Minimum free heap since boot
    
    // Status flags
    bool data_valid;           // True if data is valid
    uint32_t last_update;      // Last update timestamp (ms)
    
} system_data_t;

/**
 * @brief Initialize system monitor
 * @return ESP_OK on success
 */
esp_err_t system_monitor_init(void);

/**
 * @brief Start system monitoring task
 * @return ESP_OK on success
 */
esp_err_t system_monitor_start(void);

/**
 * @brief Stop system monitoring task
 * @return ESP_OK on success
 */
esp_err_t system_monitor_stop(void);

/**
 * @brief Get current system data (thread-safe)
 * @param data Pointer to system_data_t structure to fill
 * @return ESP_OK on success
 */
esp_err_t system_monitor_get_data(system_data_t *data);

/**
 * @brief Get pointer to global system data (read-only, use with caution)
 * @return Pointer to global system data
 */
const system_data_t* system_monitor_get_global_data(void);

/**
 * @brief Force update system data immediately
 * @return ESP_OK on success
 */
esp_err_t system_monitor_update_now(void);

/**
 * @brief Check if system data has been updated since last check
 * @return true if data has been updated, false otherwise
 */
bool system_monitor_is_data_updated(void);

/**
 * @brief Clear the data updated flag
 * This should be called after consuming the updated data
 */
void system_monitor_clear_updated_flag(void);

/**
 * @brief Deinitialize system monitor
 * @return ESP_OK on success
 */
esp_err_t system_monitor_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // SYSTEM_MONITOR_H