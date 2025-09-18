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
#include <time.h>
#include <string.h>
#include <assert.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "nvs_flash.h"
#include "esp_random.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_now.h"
#include "esp_crc.h"
#include "espnow_example.h"
#include "axp192.h"
#include "st7789_lcd.h"
#include "buzzer.h"
#include "red_led.h"
#include "button.h"
#include "lvgl_init.h"
#include "system_monitor.h"
#include "page_manager.h"
#include "lvgl_button_input.h"
#include "page_manager_lvgl.h"

#define ESPNOW_MAXDELAY 512

static const char *TAG = "espnow_example";

static QueueHandle_t s_example_espnow_queue = NULL;

static uint8_t s_example_broadcast_mac[ESP_NOW_ETH_ALEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
static uint16_t s_example_espnow_seq[EXAMPLE_ESPNOW_DATA_MAX] = { 0, 0 };

static void example_espnow_deinit(example_espnow_send_param_t *send_param);

/* WiFi should start before using ESPNOW */
static void example_wifi_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    ESP_ERROR_CHECK( esp_wifi_set_mode(ESPNOW_WIFI_MODE) );
    ESP_ERROR_CHECK( esp_wifi_start());
    ESP_ERROR_CHECK( esp_wifi_set_channel(CONFIG_ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE));

#if CONFIG_ESPNOW_ENABLE_LONG_RANGE
    ESP_ERROR_CHECK( esp_wifi_set_protocol(ESPNOW_WIFI_IF, WIFI_PROTOCOL_11B|WIFI_PROTOCOL_11G|WIFI_PROTOCOL_11N|WIFI_PROTOCOL_LR) );
#endif
}

/* ESPNOW sending or receiving callback function is called in WiFi task.
 * Users should not do lengthy operations from this task. Instead, post
 * necessary data to a queue and handle it from a lower priority task. */
static void example_espnow_send_cb(const esp_now_send_info_t *tx_info, esp_now_send_status_t status)
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
    if (xQueueSend(s_example_espnow_queue, &evt, ESPNOW_MAXDELAY) != pdTRUE) {
        ESP_LOGW(TAG, "Send send queue fail");
    }
}

static void example_espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len)
{
    example_espnow_event_t evt;
    example_espnow_event_recv_cb_t *recv_cb = &evt.info.recv_cb;
    uint8_t * mac_addr = recv_info->src_addr;
    uint8_t * des_addr = recv_info->des_addr;

    if (mac_addr == NULL || data == NULL || len <= 0) {
        ESP_LOGE(TAG, "Receive cb arg error");
        return;
    }

    if (IS_BROADCAST_ADDR(des_addr)) {
        /* If added a peer with encryption before, the receive packets may be
         * encrypted as peer-to-peer message or unencrypted over the broadcast channel.
         * Users can check the destination address to distinguish it.
         */
        ESP_LOGD(TAG, "Receive broadcast ESPNOW data");
    } else {
        ESP_LOGD(TAG, "Receive unicast ESPNOW data");
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
    if (xQueueSend(s_example_espnow_queue, &evt, ESPNOW_MAXDELAY) != pdTRUE) {
        ESP_LOGW(TAG, "Send receive queue fail");
        free(recv_cb->data);
    }
}

/* Parse received ESPNOW data. */
int example_espnow_data_parse(uint8_t *data, uint16_t data_len, uint8_t *state, uint16_t *seq, uint32_t *magic)
{
    example_espnow_data_t *buf = (example_espnow_data_t *)data;
    uint16_t crc, crc_cal = 0;

    if (data_len < sizeof(example_espnow_data_t)) {
        ESP_LOGE(TAG, "Receive ESPNOW data too short, len:%d", data_len);
        return -1;
    }

    *state = buf->state;
    *seq = buf->seq_num;
    *magic = buf->magic;
    crc = buf->crc;
    buf->crc = 0;
    crc_cal = esp_crc16_le(UINT16_MAX, (uint8_t const *)buf, data_len);

    if (crc_cal == crc) {
        return buf->type;
    }

    return -1;
}

/* Prepare ESPNOW data to be sent. */
void example_espnow_data_prepare(example_espnow_send_param_t *send_param)
{
    example_espnow_data_t *buf = (example_espnow_data_t *)send_param->buffer;

    assert(send_param->len >= sizeof(example_espnow_data_t));

    buf->type = IS_BROADCAST_ADDR(send_param->dest_mac) ? EXAMPLE_ESPNOW_DATA_BROADCAST : EXAMPLE_ESPNOW_DATA_UNICAST;
    buf->state = send_param->state;
    buf->seq_num = s_example_espnow_seq[buf->type]++;
    buf->crc = 0;
    buf->magic = send_param->magic;
    /* Fill all remaining bytes after the data with random values */
    esp_fill_random(buf->payload, send_param->len - sizeof(example_espnow_data_t));
    buf->crc = esp_crc16_le(UINT16_MAX, (uint8_t const *)buf, send_param->len);
}

static void example_espnow_task(void *pvParameter)
{
    example_espnow_event_t evt;
    uint8_t recv_state = 0;
    uint16_t recv_seq = 0;
    uint32_t recv_magic = 0;
    bool is_broadcast = false;
    int ret;

    vTaskDelay(5000 / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "Start sending broadcast data");

    /* Start sending broadcast ESPNOW data. */
    example_espnow_send_param_t *send_param = (example_espnow_send_param_t *)pvParameter;
    if (esp_now_send(send_param->dest_mac, send_param->buffer, send_param->len) != ESP_OK) {
        ESP_LOGE(TAG, "Send error");
        example_espnow_deinit(send_param);
        vTaskDelete(NULL);
    }

    while (xQueueReceive(s_example_espnow_queue, &evt, portMAX_DELAY) == pdTRUE) {
        switch (evt.id) {
            case EXAMPLE_ESPNOW_SEND_CB:
            {
                example_espnow_event_send_cb_t *send_cb = &evt.info.send_cb;
                is_broadcast = IS_BROADCAST_ADDR(send_cb->mac_addr);

                ESP_LOGD(TAG, "Send data to "MACSTR", status1: %d", MAC2STR(send_cb->mac_addr), send_cb->status);

                if (is_broadcast && (send_param->broadcast == false)) {
                    break;
                }

                if (!is_broadcast) {
                    send_param->count--;
                    if (send_param->count == 0) {
                        ESP_LOGI(TAG, "Send done");
                        example_espnow_deinit(send_param);
                        vTaskDelete(NULL);
                    }
                }

                /* Delay a while before sending the next data. */
                if (send_param->delay > 0) {
                    vTaskDelay(send_param->delay/portTICK_PERIOD_MS);
                }

                ESP_LOGI(TAG, "send data to "MACSTR"", MAC2STR(send_cb->mac_addr));

                memcpy(send_param->dest_mac, send_cb->mac_addr, ESP_NOW_ETH_ALEN);
                example_espnow_data_prepare(send_param);

                /* Send the next data after the previous data is sent. */
                if (esp_now_send(send_param->dest_mac, send_param->buffer, send_param->len) != ESP_OK) {
                    ESP_LOGE(TAG, "Send error");
                    example_espnow_deinit(send_param);
                    vTaskDelete(NULL);
                }
                break;
            }
            case EXAMPLE_ESPNOW_RECV_CB:
            {
                example_espnow_event_recv_cb_t *recv_cb = &evt.info.recv_cb;

                ret = example_espnow_data_parse(recv_cb->data, recv_cb->data_len, &recv_state, &recv_seq, &recv_magic);
                free(recv_cb->data);
                if (ret == EXAMPLE_ESPNOW_DATA_BROADCAST) {
                    ESP_LOGI(TAG, "Receive %dth broadcast data from: "MACSTR", len: %d", recv_seq, MAC2STR(recv_cb->mac_addr), recv_cb->data_len);

                    /* If MAC address does not exist in peer list, add it to peer list. */
                    if (esp_now_is_peer_exist(recv_cb->mac_addr) == false) {
                        esp_now_peer_info_t *peer = malloc(sizeof(esp_now_peer_info_t));
                        if (peer == NULL) {
                            ESP_LOGE(TAG, "Malloc peer information fail");
                            example_espnow_deinit(send_param);
                            vTaskDelete(NULL);
                        }
                        memset(peer, 0, sizeof(esp_now_peer_info_t));
                        peer->channel = CONFIG_ESPNOW_CHANNEL;
                        peer->ifidx = ESPNOW_WIFI_IF;
                        peer->encrypt = true;
                        memcpy(peer->lmk, CONFIG_ESPNOW_LMK, ESP_NOW_KEY_LEN);
                        memcpy(peer->peer_addr, recv_cb->mac_addr, ESP_NOW_ETH_ALEN);
                        ESP_ERROR_CHECK( esp_now_add_peer(peer) );
                        free(peer);
                    }

                    /* Indicates that the device has received broadcast ESPNOW data. */
                    if (send_param->state == 0) {
                        send_param->state = 1;
                    }

                    /* If receive broadcast ESPNOW data which indicates that the other device has received
                     * broadcast ESPNOW data and the local magic number is bigger than that in the received
                     * broadcast ESPNOW data, stop sending broadcast ESPNOW data and start sending unicast
                     * ESPNOW data.
                     */
                    if (recv_state == 1) {
                        /* The device which has the bigger magic number sends ESPNOW data, the other one
                         * receives ESPNOW data.
                         */
                        if (send_param->unicast == false && send_param->magic >= recv_magic) {
                    	    ESP_LOGI(TAG, "Start sending unicast data");
                    	    ESP_LOGI(TAG, "send data to "MACSTR"", MAC2STR(recv_cb->mac_addr));

                    	    /* Start sending unicast ESPNOW data. */
                            memcpy(send_param->dest_mac, recv_cb->mac_addr, ESP_NOW_ETH_ALEN);
                            example_espnow_data_prepare(send_param);
                            if (esp_now_send(send_param->dest_mac, send_param->buffer, send_param->len) != ESP_OK) {
                                ESP_LOGE(TAG, "Send error");
                                example_espnow_deinit(send_param);
                                vTaskDelete(NULL);
                            }
                            else {
                                send_param->broadcast = false;
                                send_param->unicast = true;
                            }
                        }
                    }
                }
                else if (ret == EXAMPLE_ESPNOW_DATA_UNICAST) {
                    ESP_LOGI(TAG, "Receive %dth unicast data from: "MACSTR", len: %d", recv_seq, MAC2STR(recv_cb->mac_addr), recv_cb->data_len);

                    /* If receive unicast ESPNOW data, also stop sending broadcast ESPNOW data. */
                    send_param->broadcast = false;
                }
                else {
                    ESP_LOGI(TAG, "Receive error data from: "MACSTR"", MAC2STR(recv_cb->mac_addr));
                }
                break;
            }
            default:
                ESP_LOGE(TAG, "Callback type error: %d", evt.id);
                break;
        }
    }
}

static esp_err_t example_espnow_init(void)
{
    example_espnow_send_param_t *send_param;

    s_example_espnow_queue = xQueueCreate(ESPNOW_QUEUE_SIZE, sizeof(example_espnow_event_t));
    if (s_example_espnow_queue == NULL) {
        ESP_LOGE(TAG, "Create queue fail");
        return ESP_FAIL;
    }

    /* Initialize ESPNOW and register sending and receiving callback function. */
    ESP_ERROR_CHECK( esp_now_init() );
    ESP_ERROR_CHECK( esp_now_register_send_cb(example_espnow_send_cb) );
    ESP_ERROR_CHECK( esp_now_register_recv_cb(example_espnow_recv_cb) );
#if CONFIG_ESPNOW_ENABLE_POWER_SAVE
    ESP_ERROR_CHECK( esp_now_set_wake_window(CONFIG_ESPNOW_WAKE_WINDOW) );
    ESP_ERROR_CHECK( esp_wifi_connectionless_module_set_wake_interval(CONFIG_ESPNOW_WAKE_INTERVAL) );
#endif
    /* Set primary master key. */
    ESP_ERROR_CHECK( esp_now_set_pmk((uint8_t *)CONFIG_ESPNOW_PMK) );

    /* Add broadcast peer information to peer list. */
    esp_now_peer_info_t *peer = malloc(sizeof(esp_now_peer_info_t));
    if (peer == NULL) {
        ESP_LOGE(TAG, "Malloc peer information fail");
        vQueueDelete(s_example_espnow_queue);
        s_example_espnow_queue = NULL;
        esp_now_deinit();
        return ESP_FAIL;
    }
    memset(peer, 0, sizeof(esp_now_peer_info_t));
    peer->channel = CONFIG_ESPNOW_CHANNEL;
    peer->ifidx = ESPNOW_WIFI_IF;
    peer->encrypt = false;
    memcpy(peer->peer_addr, s_example_broadcast_mac, ESP_NOW_ETH_ALEN);
    ESP_ERROR_CHECK( esp_now_add_peer(peer) );
    free(peer);

    /* Initialize sending parameters. */
    send_param = malloc(sizeof(example_espnow_send_param_t));
    if (send_param == NULL) {
        ESP_LOGE(TAG, "Malloc send parameter fail");
        vQueueDelete(s_example_espnow_queue);
        s_example_espnow_queue = NULL;
        esp_now_deinit();
        return ESP_FAIL;
    }
    memset(send_param, 0, sizeof(example_espnow_send_param_t));
    send_param->unicast = false;
    send_param->broadcast = true;
    send_param->state = 0;
    send_param->magic = esp_random();
    send_param->count = CONFIG_ESPNOW_SEND_COUNT;
    send_param->delay = CONFIG_ESPNOW_SEND_DELAY;
    send_param->len = CONFIG_ESPNOW_SEND_LEN;
    send_param->buffer = malloc(CONFIG_ESPNOW_SEND_LEN);
    if (send_param->buffer == NULL) {
        ESP_LOGE(TAG, "Malloc send buffer fail");
        free(send_param);
        vQueueDelete(s_example_espnow_queue);
        s_example_espnow_queue = NULL;
        esp_now_deinit();
        return ESP_FAIL;
    }
    memcpy(send_param->dest_mac, s_example_broadcast_mac, ESP_NOW_ETH_ALEN);
    example_espnow_data_prepare(send_param);

    xTaskCreate(example_espnow_task, "example_espnow_task", 2048, send_param, 4, NULL);

    return ESP_OK;
}

static void example_espnow_deinit(example_espnow_send_param_t *send_param)
{
    free(send_param->buffer);
    free(send_param);
    vQueueDelete(s_example_espnow_queue);
    s_example_espnow_queue = NULL;
    esp_now_deinit();
}

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
            
            // 6. 蜂鸣器演示
            ESP_LOGI(TAG, "🔊 开始无源蜂鸣器演示");
            esp_err_t buzzer_ret = buzzer_init();
            if (buzzer_ret == ESP_OK) {
                ESP_LOGI(TAG, "🔊 蜂鸣器初始化成功");
                ESP_LOGI(TAG, "🎵 开始蜂鸣器测试...");
                
                // 播放启动音效
                buzzer_play_startup();
                vTaskDelay(pdMS_TO_TICKS(1000));
                
                // 运行完整测试模式
                buzzer_ret = buzzer_test_patterns();
                if (buzzer_ret == ESP_OK) {
                    ESP_LOGI(TAG, "🎵 蜂鸣器测试完成");
                    // 播放成功音效
                    buzzer_play_success();
                } else {
                    ESP_LOGE(TAG, "🎵 蜂鸣器测试失败: %s", esp_err_to_name(buzzer_ret));
                    buzzer_play_error();
                }
                
                vTaskDelay(pdMS_TO_TICKS(1000));
                
                // 清理蜂鸣器资源
                ESP_LOGI(TAG, "🧹 清理蜂鸣器资源");
                buzzer_deinit();
                
            } else {
                ESP_LOGE(TAG, "🔊 蜂鸣器初始化失败: %s", esp_err_to_name(buzzer_ret));
            }
            
            // 7. 红色LED演示
            ESP_LOGI(TAG, "🔴 开始红色LED演示");
            esp_err_t led_ret = red_led_init();
            if (led_ret == ESP_OK) {
                ESP_LOGI(TAG, "🔴 红色LED初始化成功");
                ESP_LOGI(TAG, "💡 开始LED测试...");
                
                // 播放LED启动指示
                red_led_indicate_boot();
                vTaskDelay(pdMS_TO_TICKS(1000));
                
                // 运行完整LED测试模式
                led_ret = red_led_test_patterns();
                if (led_ret == ESP_OK) {
                    ESP_LOGI(TAG, "💡 LED测试完成");
                    // 播放成功指示
                    red_led_indicate_success();
                } else {
                    ESP_LOGE(TAG, "💡 LED测试失败: %s", esp_err_to_name(led_ret));
                    red_led_indicate_error();
                }
                
                vTaskDelay(pdMS_TO_TICKS(1000));
                
                // 清理LED资源
                ESP_LOGI(TAG, "🧹 清理LED资源");
                red_led_deinit();

            } else {
                ESP_LOGE(TAG, "🔴 红色LED初始化失败: %s", esp_err_to_name(led_ret));
            }
            
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
    
    // Power on display
    axp192_power_tft_display(true);      // Enable TFT display
    axp192_power_tft_backlight(true);    // Enable TFT backlight
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

    // 可选：启动WiFi和ESP-NOW（如果需要网络功能）
    // example_wifi_init();
    // example_espnow_init();
}
