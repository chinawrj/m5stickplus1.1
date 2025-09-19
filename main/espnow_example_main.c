/* ESPNOW Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

/*
   This example shows how to use ESPNOW.
   Prepare two device, one for sending ESPNOW data and another for receiving
   ESPNOW data.
*/
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"

// M5StickC Plus project includes
#include "espnow_manager.h"
#include "axp192.h"
#include "button.h"
#include "lvgl_init.h"
#include "system_monitor.h"
#include "lvgl_button_input.h"
#include "page_manager_lvgl.h"
#include "ux_service.h"

static const char *TAG = "espnow_example";

// Task monitoring function to help diagnose watchdog timeouts
static void task_monitor_debug(void *pvParameters)
{
    ESP_LOGI(TAG, "Task monitor started for watchdog debugging");
    
    while (1) {
        // Simplified task monitoring without requiring runtime stats
        UBaseType_t task_count = uxTaskGetNumberOfTasks();
        ESP_LOGI(TAG, "=== Task Monitor Report ===");
        ESP_LOGI(TAG, "Total tasks: %lu", task_count);
        ESP_LOGI(TAG, "Free heap: %lu bytes", esp_get_free_heap_size());
        ESP_LOGI(TAG, "Minimum free heap: %lu bytes", esp_get_minimum_free_heap_size());
        
        // List currently running tasks on each core
        TaskHandle_t task_handle_cpu0 = xTaskGetCurrentTaskHandleForCPU(0);
        TaskHandle_t task_handle_cpu1 = xTaskGetCurrentTaskHandleForCPU(1);
        
        if (task_handle_cpu0) {
            const char *task_name_cpu0 = pcTaskGetName(task_handle_cpu0);
            ESP_LOGI(TAG, "CPU 0 current task: %s", task_name_cpu0 ? task_name_cpu0 : "Unknown");
        }
        
        if (task_handle_cpu1) {
            const char *task_name_cpu1 = pcTaskGetName(task_handle_cpu1);
            ESP_LOGI(TAG, "CPU 1 current task: %s", task_name_cpu1 ? task_name_cpu1 : "Unknown");
        }
        
        ESP_LOGI(TAG, "==========================");
        
        // Monitor every 10 seconds
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

void app_main(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK( nvs_flash_erase() );
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

    // 初始化AXP192电源管理芯片
    ESP_LOGI(TAG, "Initializing AXP192...");
    ret = axp192_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "AXP192 initialization failed: %s", esp_err_to_name(ret));
        return;
    } else {
        ESP_LOGI(TAG, "AXP192 initialized successfully");
    }

    // Initialize UX Service (LED/Buzzer Effects) - PRIORITY STARTUP
    ESP_LOGI(TAG, "🎨 Initializing UX Service...");
    ret = ux_service_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UX Service initialization failed: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "🎨 UX Service initialized successfully");
    ESP_LOGI(TAG, "🎨 UX Service will automatically run demo effects");

    // Initialize system monitor
    ESP_LOGI(TAG, "🔍 Initializing system monitor");
    ret = system_monitor_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "System monitor initialization failed: %s", esp_err_to_name(ret));
        return;
    }
    
    // Start system monitoring task
    ret = system_monitor_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start system monitor: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "System monitor started successfully");

    // Initialize button driver
    ESP_LOGI(TAG, "🔘 Initializing button driver");
    ret = button_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Button driver initialization failed: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "Button driver initialized successfully");

    // Initialize multi-page LVGL system
    ESP_LOGI(TAG, "🖥️  Initializing LVGL multi-page system");
    ESP_LOGI(TAG, "🖥️  LCD and backlight power already enabled by AXP192 init");
    
    // Power stabilization delay (LCD power already enabled)
    vTaskDelay(pdMS_TO_TICKS(500));       // Wait for power stabilization
    
    // Initialize LVGL base system without demo UI for multi-page application
    esp_err_t lvgl_ret = lvgl_init_base();
    if (lvgl_ret != ESP_OK) {
        ESP_LOGE(TAG, "🎨 LVGL initialization failed: %s", esp_err_to_name(lvgl_ret));
        return;
    }
    ESP_LOGI(TAG, "🖥️  LVGL base system initialized successfully");
    
    // Get default display for page manager
    lv_display_t *disp = lv_display_get_default();
    if (disp == NULL) {
        ESP_LOGE(TAG, "Failed to get LVGL display handle");
        return;
    }
    
    // Initialize LVGL button input device (NEW SYSTEM)
    ESP_LOGI(TAG, "🔘 Initializing LVGL button input device...");
    ret = lvgl_button_input_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LVGL button input initialization failed: %s", esp_err_to_name(ret));
        return;
    }
    
    lv_indev_t *input_device = lvgl_button_input_get_device();
    if (input_device == NULL) {
        ESP_LOGE(TAG, "Failed to get LVGL input device handle");
        return;
    }
    ESP_LOGI(TAG, "✅ LVGL button input device initialized (A=OK, B=NEXT)");
    
    // Initialize LVGL-integrated page manager (NEW SYSTEM)
    ESP_LOGI(TAG, "📄 Initializing LVGL-integrated page manager...");
    ret = page_manager_lvgl_init(disp, input_device);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LVGL page manager initialization failed: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "✅ LVGL page manager initialized with key navigation");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "🎨 LVGL Button System Ready!");
    ESP_LOGI(TAG, "   • Button A (GPIO37): OK/ENTER action");
    ESP_LOGI(TAG, "   • Button B (GPIO39): NEXT page navigation");
    ESP_LOGI(TAG, "");

    // Create task monitoring to help diagnose watchdog issues
    ESP_LOGI(TAG, "🔍 Starting task monitor for watchdog debugging");
    xTaskCreate(task_monitor_debug, "task_monitor", 2048, NULL, 1, NULL);

    // Initialize ESP-NOW Manager for network functionality
    ESP_LOGI(TAG, "🌐 Initializing ESP-NOW Manager...");
    esp_err_t espnow_ret = espnow_manager_init();
    if (espnow_ret == ESP_OK) {
        espnow_ret = espnow_manager_start();
        if (espnow_ret == ESP_OK) {
            ESP_LOGI(TAG, "✅ ESP-NOW Manager started successfully");
        } else {
            ESP_LOGE(TAG, "❌ Failed to start ESP-NOW Manager: %s", esp_err_to_name(espnow_ret));
        }
    } else {
        ESP_LOGE(TAG, "❌ Failed to initialize ESP-NOW Manager: %s", esp_err_to_name(espnow_ret));
    }
}
