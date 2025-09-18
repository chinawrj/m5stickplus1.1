/*
 * System Monitor Implementation for M5StickC Plus 1.1
 */

#include "system_monitor.h"
#include "axp192.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdlib.h>  // for abs() function

static const char *TAG = "system_monitor";

// Global system data
static system_data_t g_system_data = {0};
static SemaphoreHandle_t g_data_mutex = NULL;
static TaskHandle_t g_monitor_task_handle = NULL;
static bool g_monitor_running = false;
static bool g_data_updated = false;  // Data update flag

// Configuration
#define MONITOR_TASK_STACK_SIZE     4096
#define MONITOR_TASK_PRIORITY       3
#define MONITOR_UPDATE_INTERVAL_MS  1000  // Update every 1 second

// Function to safely read AXP192 data with error handling
static float safe_read_float(esp_err_t (*read_func)(float*), const char* param_name) {
    float value = 0.0;
    esp_err_t ret = read_func(&value);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read %s: %s", param_name, esp_err_to_name(ret));
        return 0.0;
    }
    return value;
}

static uint8_t safe_read_uint8(esp_err_t (*read_func)(uint8_t*), const char* param_name) {
    uint8_t value = 0;
    esp_err_t ret = read_func(&value);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read %s: %s", param_name, esp_err_to_name(ret));
        return 0;
    }
    return value;
}

static bool safe_read_bool(bool (*read_func)(void), const char* param_name) {
    return read_func();
}

// Update system data
static void update_system_data(void) {
    system_data_t new_data = {0};
    
    // Get current timestamp
    new_data.last_update = esp_timer_get_time() / 1000; // Convert to ms
    
    // Battery information
    new_data.battery_voltage = safe_read_float(axp192_get_battery_voltage, "battery_voltage");
    new_data.battery_percentage = safe_read_uint8(axp192_get_battery_level, "battery_level");
    new_data.is_charging = safe_read_bool(axp192_is_charging, "is_charging");
    new_data.is_usb_connected = safe_read_bool(axp192_is_vbus_present, "is_usb_connected");
    
    // Power information
    new_data.vbus_voltage = safe_read_float(axp192_get_vbus_voltage, "vbus_voltage");
    new_data.charge_current = safe_read_float(axp192_get_battery_charge_current, "charge_current");
    new_data.discharge_current = safe_read_float(axp192_get_battery_discharge_current, "discharge_current");
    
    // Temperature
    new_data.internal_temp = safe_read_float(axp192_get_internal_temperature, "internal_temperature");
    
    // System information
    new_data.uptime_seconds = new_data.last_update / 1000;
    new_data.free_heap = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    new_data.min_free_heap = heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT);
    
    // Mark data as valid
    new_data.data_valid = true;
    
    // Update global data with mutex protection
    if (xSemaphoreTake(g_data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // Compare with existing data to detect actual changes
        bool data_changed = false;
        
        // Check if this is the first valid data or if key values have changed
        if (!g_system_data.data_valid) {
            // First time getting valid data
            data_changed = true;
        } else {
            // Compare key values that matter for UI display
            if (g_system_data.battery_voltage != new_data.battery_voltage ||
                g_system_data.battery_percentage != new_data.battery_percentage ||
                g_system_data.is_charging != new_data.is_charging ||
                g_system_data.is_usb_connected != new_data.is_usb_connected ||
                g_system_data.internal_temp != new_data.internal_temp ||
                abs((int32_t)g_system_data.free_heap - (int32_t)new_data.free_heap) > 1024) {  // Heap change > 1KB
                data_changed = true;
            }
        }
        
        // Update data and set flag only if data actually changed
        memcpy(&g_system_data, &new_data, sizeof(system_data_t));
        if (data_changed) {
            g_data_updated = true;  // Set flag only when data actually changed
            ESP_LOGD(TAG, "System data changed: Bat=%.2fV (%d%%), Temp=%.1f°C, Heap=%"PRIu32"KB", 
                    new_data.battery_voltage, new_data.battery_percentage, 
                    new_data.internal_temp, new_data.free_heap / 1024);
        } else {
            ESP_LOGV(TAG, "System data unchanged, skipping UI update flag");
        }
        
        xSemaphoreGive(g_data_mutex);
    } else {
        ESP_LOGW(TAG, "Failed to acquire mutex for data update");
    }
}

// System monitoring task
static void system_monitor_task(void *pvParameters) {
    ESP_LOGI(TAG, "System monitor task started");
    
    TickType_t last_wake_time = xTaskGetTickCount();
    
    while (g_monitor_running) {
        // Update system data
        update_system_data();
        
        // Wait for next update interval
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(MONITOR_UPDATE_INTERVAL_MS));
    }
    
    ESP_LOGI(TAG, "System monitor task stopped");
    g_monitor_task_handle = NULL;
    vTaskDelete(NULL);
}

// Public functions
esp_err_t system_monitor_init(void) {
    ESP_LOGI(TAG, "Initializing system monitor");
    
    // Create mutex for data protection
    g_data_mutex = xSemaphoreCreateMutex();
    if (g_data_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create data mutex");
        return ESP_ERR_NO_MEM;
    }
    
    // Initialize system data
    memset(&g_system_data, 0, sizeof(system_data_t));
    g_data_updated = false;  // Initialize data updated flag to false
    
    // Perform initial data update
    update_system_data();
    
    ESP_LOGI(TAG, "System monitor initialized successfully");
    return ESP_OK;
}

esp_err_t system_monitor_start(void) {
    if (g_monitor_running) {
        ESP_LOGW(TAG, "System monitor already running");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Starting system monitor task");
    
    g_monitor_running = true;
    
    BaseType_t ret = xTaskCreate(
        system_monitor_task,
        "sys_monitor",
        MONITOR_TASK_STACK_SIZE,
        NULL,
        MONITOR_TASK_PRIORITY,
        &g_monitor_task_handle
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create system monitor task");
        g_monitor_running = false;
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "System monitor task started successfully");
    return ESP_OK;
}

esp_err_t system_monitor_stop(void) {
    if (!g_monitor_running) {
        ESP_LOGW(TAG, "System monitor not running");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Stopping system monitor task");
    
    g_monitor_running = false;
    
    // Wait for task to finish
    if (g_monitor_task_handle != NULL) {
        while (g_monitor_task_handle != NULL) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
    
    ESP_LOGI(TAG, "System monitor task stopped");
    return ESP_OK;
}

esp_err_t system_monitor_get_data(system_data_t *data) {
    if (data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(g_data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        memcpy(data, &g_system_data, sizeof(system_data_t));
        xSemaphoreGive(g_data_mutex);
        return ESP_OK;
    } else {
        ESP_LOGW(TAG, "Failed to acquire mutex for data read");
        return ESP_ERR_TIMEOUT;
    }
}

const system_data_t* system_monitor_get_global_data(void) {
    return &g_system_data;
}

esp_err_t system_monitor_update_now(void) {
    update_system_data();
    return ESP_OK;
}

bool system_monitor_is_data_updated(void) {
    bool updated = false;
    if (xSemaphoreTake(g_data_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        updated = g_data_updated;
        xSemaphoreGive(g_data_mutex);
    }
    return updated;
}

void system_monitor_clear_updated_flag(void) {
    if (xSemaphoreTake(g_data_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        g_data_updated = false;
        xSemaphoreGive(g_data_mutex);
    }
}

esp_err_t system_monitor_deinit(void) {
    ESP_LOGI(TAG, "Deinitializing system monitor");
    
    // Stop monitoring task if running
    system_monitor_stop();
    
    // Clean up mutex
    if (g_data_mutex != NULL) {
        vSemaphoreDelete(g_data_mutex);
        g_data_mutex = NULL;
    }
    
    // Clear global data
    memset(&g_system_data, 0, sizeof(system_data_t));
    g_data_updated = false;  // Reset data updated flag
    
    ESP_LOGI(TAG, "System monitor deinitialized");
    return ESP_OK;
}