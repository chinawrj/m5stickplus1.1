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
#include "st7789_lcd.h"
#include "button.h"
#include "lvgl_init.h"
#include "system_monitor.h"
#include "page_manager.h"
#include "lvgl_button_input.h"
#include "page_manager_lvgl.h"
#include "ux_service.h"

static const char *TAG = "espnow_example";

// Button interrupt callback function
static void button_interrupt_callback(button_id_t button_id, button_event_t event, uint32_t press_duration)
{
    const char* event_names[] = {
        "PRESSED", "RELEASED", "SHORT_PRESS", "LONG_PRESS", "NONE"
    };
    
    ESP_LOGI(TAG, "🔘 %s %s (duration: %lums)", 
             button_get_name(button_id), 
             event_names[event], 
             press_duration);
             
    // Add specific button response logic here
    switch (button_id) {
        case BUTTON_A:
            if (event == BUTTON_EVENT_SHORT_PRESS) {
                ESP_LOGI(TAG, "🔘 Button A short press - Function switch");
            } else if (event == BUTTON_EVENT_LONG_PRESS) {
                ESP_LOGI(TAG, "🔘 Button A long press - Settings mode");
            }
            break;
            
        case BUTTON_B:
            if (event == BUTTON_EVENT_SHORT_PRESS) {
                ESP_LOGI(TAG, "🔘 Button B short press - Confirm action");
            } else if (event == BUTTON_EVENT_LONG_PRESS) {
                ESP_LOGI(TAG, "🔘 Button B long press - Restart function");
            }
            break;
            
        default:
            break;
    }
}

static void axp192_monitor_task(void *pvParameters)
{
    static bool hardware_demo_completed = false;  // Hardware demo completion flag
    static bool button_monitoring_initialized = false;  // Button monitoring initialization flag
    float voltage, current, power, charge_current, discharge_current, temp, vbus_voltage, vbus_current;
    uint8_t battery_level;
    
    while (1) {
        ESP_LOGI(TAG, "=== 🔋 M5StickC Plus Complete System Monitor ===");
        
        // Basic battery information
        if (axp192_get_battery_voltage(&voltage) == ESP_OK &&
            axp192_get_battery_current(&current) == ESP_OK &&
            axp192_get_battery_power(&power) == ESP_OK &&
            axp192_get_battery_level(&battery_level) == ESP_OK) {
            
            ESP_LOGI(TAG, "🔋 Battery: %.3fV | %.1fmA | %.1fmW | %d%%", 
                     voltage, current, power, battery_level);
        }
        
        // Advanced current analysis
        if (axp192_get_battery_charge_current(&charge_current) == ESP_OK &&
            axp192_get_battery_discharge_current(&discharge_current) == ESP_OK) {
            
            ESP_LOGI(TAG, "⚡ Current: Charge%.1fmA | Discharge%.1fmA | Net%.1fmA", 
                     charge_current, discharge_current, charge_current - discharge_current);
            ESP_LOGI(TAG, "🔌 Status: %s", axp192_is_charging() ? "Charging" : "Not charging");
        }
        
        // System status
        if (axp192_get_internal_temperature(&temp) == ESP_OK) {
            ESP_LOGI(TAG, "🌡️ Temperature: %.1f°C", temp);
        }
        
        ESP_LOGI(TAG, "📋 Connection: Battery%s | VBUS%s | 5V Output%s", 
                 axp192_is_battery_present() ? "✅" : "❌",
                 axp192_is_vbus_present() ? "✅" : "❌",
                 axp192_get_exten_status() ? "✅" : "❌");
        
        // USB/VBUS information
        if (axp192_is_vbus_present() && 
            axp192_get_vbus_voltage(&vbus_voltage) == ESP_OK &&
            axp192_get_vbus_current(&vbus_current) == ESP_OK) {
            
            ESP_LOGI(TAG, "🔌 USB: %.3fV | %.1fmA", vbus_voltage, vbus_current);
        }
        
        // === 硬件演示（仅运行一次） ===
        if (!hardware_demo_completed) {
            ESP_LOGI(TAG, "=== 🎛️ 硬件外设控制演示（仅运行一次）===");
                // 1. 屏幕系统演示
            ESP_LOGI(TAG, "📱 演示: 屏幕系统控制");
            axp192_power_tft_display(true);      // 屏幕显示 (LDO3=3.0V)
            axp192_power_tft_backlight(true);    // 屏幕背光 (LDO2=3.3V)
            ESP_LOGI(TAG, "💡 屏幕系统: 完全开启");
            vTaskDelay(pdMS_TO_TICKS(2000));
            
            // 2. 麦克风系统演示
            ESP_LOGI(TAG, "🎤 演示: 麦克风系统控制");
            axp192_power_microphone(true);       // 麦克风 (LDO0=3.3V)
            ESP_LOGI(TAG, "🎤 麦克风: 开启 (安全固定电压)");
            
            // GPIO演示
            ESP_LOGI(TAG, "📌 GPIO演示: GPIO0-4闪烁");
            for (int i = 0; i < 5; i++) {
                axp192_set_gpio0(i % 2);
                axp192_set_gpio1(i % 2);
                axp192_set_gpio2(i % 2);
                axp192_set_gpio3(i % 2);
                axp192_set_gpio4(i % 2);
                vTaskDelay(pdMS_TO_TICKS(200));
            }
            vTaskDelay(pdMS_TO_TICKS(2000));
            
            // 3. 5V GROVE输出演示
            ESP_LOGI(TAG, "🔌 演示: 5V GROVE输出控制");
            axp192_power_grove_5v(true);         // 5V输出 (EXTEN)
            ESP_LOGI(TAG, "🔌 5V GROVE: 开启 (安全控制)");
            vTaskDelay(pdMS_TO_TICKS(2000));
            
            // 4. 关闭所有外设（保留ESP32核心供电）
            // 5. ST7789 TFT显示屏演示
            ESP_LOGI(TAG, "🖥️  开始ST7789 TFT显示屏演示");
            esp_err_t tft_ret = st7789_lcd_init();
            if (tft_ret == ESP_OK) {
                ESP_LOGI(TAG, "🖥️  ST7789 TFT显示屏初始化成功");
                ESP_LOGI(TAG, "🎨 开始显示测试图案...");
                
                // 运行测试图案
                tft_ret = st7789_lcd_test_patterns();
                if (tft_ret == ESP_OK) {
                    ESP_LOGI(TAG, "🎨 测试图案显示完成");
                } else {
                    ESP_LOGE(TAG, "🎨 测试图案显示失败: %s", esp_err_to_name(tft_ret));
                }
                
                // 清理显示屏资源
                ESP_LOGI(TAG, "🧹 清理ST7789资源");
                st7789_lcd_deinit();
                
            } else {
                ESP_LOGE(TAG, "🖥️  ST7789 TFT显示屏初始化失败: %s", esp_err_to_name(tft_ret));
            }
            
            // 6. Hardware power management demonstrations completed
            ESP_LOGI(TAG, "� Hardware demonstrations completed");
            ESP_LOGI(TAG, "ℹ️  LED/Buzzer effects now managed by UX Service");
            
            // 8. 按键驱动演示
            ESP_LOGI(TAG, "🔘 开始按键驱动演示");
            esp_err_t btn_ret = button_init();
            if (btn_ret == ESP_OK) {
                ESP_LOGI(TAG, "🔘 按键驱动初始化成功");
                ESP_LOGI(TAG, "🧪 开始按键测试...");
                
                // 执行按键测试
                btn_ret = button_test_all_functions();
                if (btn_ret == ESP_OK) {
                    ESP_LOGI(TAG, "🧪 按键测试完成");
                } else {
                    ESP_LOGE(TAG, "🔘 按键测试失败: %s", esp_err_to_name(btn_ret));
                }
                
                vTaskDelay(pdMS_TO_TICKS(1000));
                
                // 清理按键资源
                ESP_LOGI(TAG, "🧹 清理按键资源");
                button_deinit();
                
            } else {
                ESP_LOGE(TAG, "🔘 按键驱动初始化失败: %s", esp_err_to_name(btn_ret));
            }
            
            ESP_LOGI(TAG, "💤 关闭所有外设");
            
            axp192_power_tft_backlight(false);   // 关闭屏幕背光
            axp192_power_tft_display(false);     // 关闭屏幕显示
            axp192_power_microphone(false);      // 关闭麦克风
            axp192_power_grove_5v(false);        // 关闭5V输出
            
            // 关闭所有GPIO
            axp192_set_gpio0(false);
            axp192_set_gpio1(false);
            axp192_set_gpio2(false);
            axp192_set_gpio3(false);
            axp192_set_gpio4(false);
            
            ESP_LOGI(TAG, "💤 所有外设已关闭，后续可按需调用API开启");
            ESP_LOGI(TAG, "✅ 硬件演示完成，进入持续监控模式");
            
            hardware_demo_completed = true;  // 标记演示已完成
        }
        
        // 按键监控初始化（仅在硬件演示完成后进行持续监控）
        if (hardware_demo_completed && !button_monitoring_initialized) {
            ESP_LOGI(TAG, "🔘 初始化按键持续监控模式");
            esp_err_t btn_ret = button_init();
            if (btn_ret == ESP_OK) {
                // 设置中断回调
                button_set_interrupt_callback(button_interrupt_callback);
                button_set_interrupt_mode(true);
                button_monitoring_initialized = true;
                ESP_LOGI(TAG, "🔘 按键监控模式已启用 (中断+轮询)");
            } else {
                ESP_LOGE(TAG, "🔘 按键监控初始化失败: %s", esp_err_to_name(btn_ret));
            }
        }
        
        // 按键轮询检测（在电池监控的同时进行）
        if (button_monitoring_initialized) {
            for (int i = 0; i < BUTTON_COUNT; i++) {
                button_event_t event = button_poll_event(i);
                if (event != BUTTON_EVENT_NONE) {
                    button_state_t state;
                    if (button_get_state(i, &state) == ESP_OK) {
                        ESP_LOGI(TAG, "🔘 POLL: %s - Event %d (duration: %lums, count: %lu)", 
                                 button_get_name(i), event, state.press_duration, state.press_count);
                    }
                }
            }
        }
        
        ESP_LOGI(TAG, "========================================");
        vTaskDelay(pdMS_TO_TICKS(5000));  // 5秒更新一次
    }
}

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
