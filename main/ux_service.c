#include "ux_service.h"
#include "axp192.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include <string.h>

static const char *TAG = "UX_SERVICE";

// Hardware definitions for M5StickC Plus
#define RED_LED_PIN     GPIO_NUM_10
#define BUZZER_PIN      GPIO_NUM_2

// UX Service State
static QueueHandle_t ux_queue = NULL;
static TaskHandle_t ux_task_handle = NULL;
static bool ux_service_running = false;
static ux_service_stats_t ux_stats = {0};

// Hardware state
static bool led_initialized = false;
static bool buzzer_initialized = false;
static bool demo_completed = false;

// Forward declarations
static void ux_service_task(void *pvParameters);
static void ux_queue_startup_demo_effects(void);
static esp_err_t ux_execute_led_effect(ux_led_effect_type_t effect_type, uint32_t duration_ms, uint32_t repeat_count, uint32_t parameter);
static esp_err_t ux_execute_buzzer_effect(ux_buzzer_effect_type_t effect_type, uint32_t duration_ms, uint32_t repeat_count, uint32_t parameter);
static const char* ux_effect_to_string(ux_effect_t effect);

// Hardware initialization functions
static esp_err_t ux_init_led(void);
static esp_err_t ux_init_buzzer(void);
static void ux_deinit_led(void);
static void ux_deinit_buzzer(void);

// Direct hardware control functions
static esp_err_t ux_led_set(bool on);
static esp_err_t ux_led_blink(uint32_t on_time_ms, uint32_t off_time_ms, uint32_t cycles);
static esp_err_t ux_buzzer_tone(uint32_t frequency_hz, uint32_t duration_ms);
static esp_err_t ux_buzzer_stop(void);

esp_err_t ux_service_init(void)
{
    ESP_LOGI(TAG, "🎨 Initializing UX Service...");
    
    if (ux_service_running) {
        ESP_LOGW(TAG, "UX Service already running");
        return ESP_OK;
    }
    
    // Create message queue
    ux_queue = xQueueCreate(UX_SERVICE_QUEUE_SIZE, sizeof(ux_message_t));
    if (ux_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create UX message queue");
        return ESP_ERR_NO_MEM;
    }
    
    // Initialize hardware components
    esp_err_t ret;
    
    // Initialize LED
    ret = ux_init_led();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize LED: %s", esp_err_to_name(ret));
        vQueueDelete(ux_queue);
        return ret;
    }
    
    // Initialize buzzer (5V GROVE power already enabled in AXP192 init)
    ESP_LOGI(TAG, "Initializing buzzer (5V GROVE power already enabled)...");
    vTaskDelay(pdMS_TO_TICKS(100)); // Power stabilization delay
    
    ret = ux_init_buzzer();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize buzzer: %s", esp_err_to_name(ret));
        ux_deinit_led();
        vQueueDelete(ux_queue);
        return ret;
    }
    
    // Create UX service task
    BaseType_t task_ret = xTaskCreate(
        ux_service_task,
        UX_SERVICE_TASK_NAME,
        UX_SERVICE_TASK_STACK_SIZE,
        NULL,
        UX_SERVICE_TASK_PRIORITY,
        &ux_task_handle
    );
    
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create UX service task");
        ux_deinit_buzzer();
        ux_deinit_led();
        vQueueDelete(ux_queue);
        return ESP_ERR_NO_MEM;
    }
    
    ux_service_running = true;
    memset(&ux_stats, 0, sizeof(ux_stats));
    
    ESP_LOGI(TAG, "🎨 UX Service initialized successfully");
    return ESP_OK;
}

esp_err_t ux_service_deinit(void)
{
    ESP_LOGI(TAG, "🧹 Deinitializing UX Service...");
    
    if (!ux_service_running) {
        ESP_LOGW(TAG, "UX Service not running");
        return ESP_OK;
    }
    
    ux_service_running = false;
    
    // Delete task
    if (ux_task_handle) {
        vTaskDelete(ux_task_handle);
        ux_task_handle = NULL;
    }
    
    // Delete queue
    if (ux_queue) {
        vQueueDelete(ux_queue);
        ux_queue = NULL;
    }
    
    // Deinitialize hardware
    ux_deinit_buzzer();
    ux_deinit_led();
    
    ESP_LOGI(TAG, "🧹 UX Service deinitialized");
    return ESP_OK;
}

esp_err_t ux_service_send_effect(ux_effect_t effect, 
                                  uint32_t duration_ms,
                                  uint32_t repeat_count,
                                  uint32_t parameter)
{
    if (!ux_service_running || ux_queue == NULL) {
        ESP_LOGE(TAG, "UX Service not running");
        return ESP_ERR_INVALID_STATE;
    }
    
    ux_message_t message = {
        .effect = effect,
        .duration_ms = duration_ms,
        .repeat_count = repeat_count,
        .parameter = parameter
    };
    
    BaseType_t ret = xQueueSend(ux_queue, &message, pdMS_TO_TICKS(100));
    if (ret != pdTRUE) {
        ux_stats.queue_full_errors++;
        ESP_LOGW(TAG, "UX queue full, message dropped");
        return ESP_ERR_TIMEOUT;
    }
    
    return ESP_OK;
}

esp_err_t ux_service_send_simple_effect(ux_effect_t effect)
{
    return ux_service_send_effect(effect, 0, 0, 0);
}

esp_err_t ux_service_get_stats(ux_service_stats_t *stats)
{
    if (stats == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memcpy(stats, &ux_stats, sizeof(ux_service_stats_t));
    return ESP_OK;
}

bool ux_service_is_running(void)
{
    return ux_service_running;
}

// LED Convenience Functions
esp_err_t ux_led_off(void)
{
    return ux_service_send_simple_effect(UX_LED_EFFECT(UX_LED_EFFECT_OFF));
}

esp_err_t ux_led_on(void)
{
    return ux_service_send_simple_effect(UX_LED_EFFECT(UX_LED_EFFECT_ON));
}

esp_err_t ux_led_blink_fast(uint32_t duration_ms)
{
    return ux_service_send_effect(UX_LED_EFFECT(UX_LED_EFFECT_BLINK_FAST), duration_ms, 0, 0);
}

esp_err_t ux_led_breathing(uint32_t cycles)
{
    return ux_service_send_effect(UX_LED_EFFECT(UX_LED_EFFECT_BREATHING), 0, cycles, 0);
}

esp_err_t ux_led_success_pattern(void)
{
    return ux_service_send_simple_effect(UX_LED_EFFECT(UX_LED_EFFECT_SUCCESS_PATTERN));
}

// Buzzer Convenience Functions
esp_err_t ux_buzzer_silence(void)
{
    return ux_service_send_simple_effect(UX_BUZZER_EFFECT(UX_BUZZER_EFFECT_SILENCE));
}

esp_err_t ux_buzzer_startup(void)
{
    return ux_service_send_simple_effect(UX_BUZZER_EFFECT(UX_BUZZER_EFFECT_STARTUP));
}

esp_err_t ux_buzzer_success(void)
{
    return ux_service_send_simple_effect(UX_BUZZER_EFFECT(UX_BUZZER_EFFECT_SUCCESS));
}

esp_err_t ux_buzzer_error(void)
{
    return ux_service_send_simple_effect(UX_BUZZER_EFFECT(UX_BUZZER_EFFECT_ERROR));
}

esp_err_t ux_buzzer_notification(void)
{
    return ux_service_send_simple_effect(UX_BUZZER_EFFECT(UX_BUZZER_EFFECT_NOTIFICATION));
}

// Private Functions

/**
 * @brief Queue startup demo effects for LED and Buzzer
 * 
 * This function queues a series of demo effects to showcase
 * the capabilities of both LED and buzzer components.
 */
static void ux_queue_startup_demo_effects(void)
{
    if (demo_completed || ux_queue == NULL) {
        return;
    }
    
    ESP_LOGI(TAG, "🎨 Sending startup demo effects to queue...");
    
    // Send LED demo effects
    ux_message_t led_effects[] = {
        {UX_LED_EFFECT(UX_LED_EFFECT_ON), 1000, 0, 0},           // LED on for 1s
        {UX_LED_EFFECT(UX_LED_EFFECT_BLINK_FAST), 2000, 0, 0},  // Fast blink for 2s
        {UX_LED_EFFECT(UX_LED_EFFECT_BREATHING), 0, 2, 0},       // Breathing 2 cycles
        {UX_LED_EFFECT(UX_LED_EFFECT_SUCCESS_PATTERN), 0, 0, 0}, // Success pattern
        {UX_LED_EFFECT(UX_LED_EFFECT_OFF), 0, 0, 0}              // LED off
    };
    
    for (int i = 0; i < 5; i++) {
        xQueueSend(ux_queue, &led_effects[i], portMAX_DELAY);
    }
    
    // Send buzzer demo effects
    ux_message_t buzzer_effects[] = {
        {UX_BUZZER_EFFECT(UX_BUZZER_EFFECT_STARTUP), 0, 0, 0},      // Startup sound
        {UX_BUZZER_EFFECT(UX_BUZZER_EFFECT_SUCCESS), 0, 0, 0},      // Success sound
        {UX_BUZZER_EFFECT(UX_BUZZER_EFFECT_ERROR), 0, 0, 0},        // Error sound
        {UX_BUZZER_EFFECT(UX_BUZZER_EFFECT_NOTIFICATION), 0, 0, 0}, // Notification sound
        {UX_BUZZER_EFFECT(UX_BUZZER_EFFECT_SILENCE), 0, 0, 0}       // Silence
    };
    
    for (int i = 0; i < 5; i++) {
        xQueueSend(ux_queue, &buzzer_effects[i], portMAX_DELAY);
    }
    
    demo_completed = true;
    ESP_LOGI(TAG, "🎨 Startup demo effects queued successfully");
}

static void ux_service_task(void *pvParameters)
{
    ESP_LOGI(TAG, "🎨 UX Service task started");
    
    // Wait a bit for hardware to stabilize
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Queue startup demo effects
    ux_queue_startup_demo_effects();
    
    ux_message_t message;
    
    while (ux_service_running) {
        // Wait for messages
        if (xQueueReceive(ux_queue, &message, pdMS_TO_TICKS(1000)) == pdTRUE) {
            ux_stats.messages_processed++;
            
            ESP_LOGI(TAG, "🎬 Processing: %s", ux_effect_to_string(message.effect));
            
            esp_err_t result = ESP_OK;
            
            // Route message based on device type
            if (message.effect.device_type == UX_DEVICE_LED) {
                result = ux_execute_led_effect(message.effect.led_effect, message.duration_ms, 
                                             message.repeat_count, message.parameter);
                if (result == ESP_OK) {
                    ux_stats.led_effects_count++;
                }
            }
            else if (message.effect.device_type == UX_DEVICE_BUZZER) {
                result = ux_execute_buzzer_effect(message.effect.buzzer_effect, message.duration_ms, 
                                                message.repeat_count, message.parameter);
                if (result == ESP_OK) {
                    ux_stats.buzzer_effects_count++;
                }
            }
            else {
                ESP_LOGW(TAG, "Unknown UX device type: %d", message.effect.device_type);
                result = ESP_ERR_INVALID_ARG;
            }
            
            if (result != ESP_OK) {
                ux_stats.execution_errors++;
                ESP_LOGE(TAG, "Failed to execute UX effect %s: %s", 
                         ux_effect_to_string(message.effect), esp_err_to_name(result));
            }
        }
    }
    
    ESP_LOGI(TAG, "🎨 UX Service task ending");
    vTaskDelete(NULL);
}

static esp_err_t ux_execute_led_effect(ux_led_effect_type_t effect_type, uint32_t duration_ms, uint32_t repeat_count, uint32_t parameter)
{
    esp_err_t ret = ESP_OK;
    
    switch (effect_type) {
        case UX_LED_EFFECT_OFF:
            ret = ux_led_set(false);
            break;
            
        case UX_LED_EFFECT_ON:
            ret = ux_led_set(true);
            break;
            
        case UX_LED_EFFECT_BLINK_FAST:
            if (duration_ms > 0) {
                uint32_t cycles = duration_ms / 200; // 100ms on + 100ms off = 200ms per cycle
                ret = ux_led_blink(100, 100, cycles);
            } else {
                ret = ux_led_blink(100, 100, 10); // Default 10 cycles (2 seconds)
            }
            break;
            
        case UX_LED_EFFECT_BLINK_SLOW:
            if (duration_ms > 0) {
                uint32_t cycles = duration_ms / 1000; // 500ms on + 500ms off = 1000ms per cycle
                ret = ux_led_blink(500, 500, cycles);
            } else {
                ret = ux_led_blink(500, 500, 5); // Default 5 cycles (5 seconds)
            }
            break;
            
        case UX_LED_EFFECT_BREATHING:
            {
                uint32_t cycles = repeat_count > 0 ? repeat_count : 3; // Default 3 cycles
                // Simulate breathing effect with slow blink
                ret = ux_led_blink(800, 800, cycles); // 0.8s on, 0.8s off
            }
            break;
            
        case UX_LED_EFFECT_SUCCESS_PATTERN:
            // Success pattern: 3 quick blinks
            ret = ux_led_blink(150, 150, 3);
            break;
            
        case UX_LED_EFFECT_ERROR_PATTERN:
            // Error pattern: rapid blink 5 times
            ret = ux_led_blink(100, 100, 5);
            break;
            
        default:
            ESP_LOGW(TAG, "Unknown LED effect: %d", effect_type);
            ret = ESP_ERR_INVALID_ARG;
            break;
    }
    
    return ret;
}

static esp_err_t ux_execute_buzzer_effect(ux_buzzer_effect_type_t effect_type, uint32_t duration_ms, uint32_t repeat_count, uint32_t parameter)
{
    esp_err_t ret = ESP_OK;
    
    switch (effect_type) {
        case UX_BUZZER_EFFECT_SILENCE:
            ret = ux_buzzer_stop();
            break;
            
        case UX_BUZZER_EFFECT_STARTUP:
            // Startup sound: ascending tones
            ret = ux_buzzer_tone(440, 200); // A4
            vTaskDelay(pdMS_TO_TICKS(250));
            ret = ux_buzzer_tone(554, 200); // C#5
            vTaskDelay(pdMS_TO_TICKS(250));
            ret = ux_buzzer_tone(659, 300); // E5
            break;
            
        case UX_BUZZER_EFFECT_SUCCESS:
            // Success sound: happy melody
            ret = ux_buzzer_tone(523, 150); // C5
            vTaskDelay(pdMS_TO_TICKS(170));
            ret = ux_buzzer_tone(659, 150); // E5
            vTaskDelay(pdMS_TO_TICKS(170));
            ret = ux_buzzer_tone(784, 200); // G5
            break;
            
        case UX_BUZZER_EFFECT_ERROR:
            // Error sound: descending tones
            ret = ux_buzzer_tone(800, 200); // High
            vTaskDelay(pdMS_TO_TICKS(220));
            ret = ux_buzzer_tone(600, 200); // Medium
            vTaskDelay(pdMS_TO_TICKS(220));
            ret = ux_buzzer_tone(400, 300); // Low
            break;
            
        case UX_BUZZER_EFFECT_NOTIFICATION:
            // Notification: double beep
            ret = ux_buzzer_tone(1000, 150);
            vTaskDelay(pdMS_TO_TICKS(200));
            ret = ux_buzzer_tone(1000, 150);
            break;
            
        case UX_BUZZER_EFFECT_WARNING:
            // Warning: alternating tones
            ret = ux_buzzer_tone(500, 200);
            vTaskDelay(pdMS_TO_TICKS(220));
            ret = ux_buzzer_tone(400, 200);
            break;
            
        case UX_BUZZER_EFFECT_CLICK:
            // Click: very short high tone
            ret = ux_buzzer_tone(1000, 50);
            break;
            
        default:
            ESP_LOGW(TAG, "Unknown buzzer effect: %d", effect_type);
            ret = ESP_ERR_INVALID_ARG;
            break;
    }
    
    return ret;
}

// Hardware initialization functions
static esp_err_t ux_init_led(void)
{
    if (led_initialized) {
        return ESP_OK;
    }
    
    // Configure LED GPIO (active LOW on M5StickC Plus)
    gpio_config_t led_conf = {
        .pin_bit_mask = (1ULL << RED_LED_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    
    esp_err_t ret = gpio_config(&led_conf);
    if (ret == ESP_OK) {
        gpio_set_level(RED_LED_PIN, 1); // LED OFF (active LOW)
        led_initialized = true;
        ESP_LOGI(TAG, "LED initialized on GPIO%d (active LOW)", RED_LED_PIN);
    }
    
    return ret;
}

static esp_err_t ux_init_buzzer(void)
{
    if (buzzer_initialized) {
        return ESP_OK;
    }
    
    // Configure LEDC timer for buzzer
    ledc_timer_config_t timer_config = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_1,      // Use timer 1 to avoid conflicts
        .duty_resolution = LEDC_TIMER_13_BIT,
        .freq_hz = 1000,
        .clk_cfg = LEDC_AUTO_CLK
    };
    
    esp_err_t ret = ledc_timer_config(&timer_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LEDC timer: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Configure LEDC channel for buzzer
    ledc_channel_config_t channel_config = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_1,      // Use channel 1 to avoid conflicts
        .timer_sel = LEDC_TIMER_1,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = BUZZER_PIN,
        .duty = 0,
        .hpoint = 0
    };
    
    ret = ledc_channel_config(&channel_config);
    if (ret == ESP_OK) {
        buzzer_initialized = true;
        ESP_LOGI(TAG, "Buzzer initialized on GPIO%d", BUZZER_PIN);
    } else {
        ESP_LOGW(TAG, "Buzzer initialization warning: %s (may still work)", esp_err_to_name(ret));
        buzzer_initialized = true; // Continue anyway as it may still work
        ret = ESP_OK;
    }
    
    return ret;
}

static void ux_deinit_led(void)
{
    if (led_initialized) {
        gpio_set_level(RED_LED_PIN, 1); // LED OFF
        led_initialized = false;
        ESP_LOGI(TAG, "LED deinitialized");
    }
}

static void ux_deinit_buzzer(void)
{
    if (buzzer_initialized) {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 0);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
        buzzer_initialized = false;
        ESP_LOGI(TAG, "Buzzer deinitialized");
    }
}

// Direct hardware control functions
static esp_err_t ux_led_set(bool on)
{
    if (!led_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // M5StickC Plus LED is active LOW
    gpio_set_level(RED_LED_PIN, on ? 0 : 1);
    return ESP_OK;
}

static esp_err_t ux_led_blink(uint32_t on_time_ms, uint32_t off_time_ms, uint32_t cycles)
{
    if (!led_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    for (uint32_t i = 0; i < cycles; i++) {
        ux_led_set(true);
        vTaskDelay(pdMS_TO_TICKS(on_time_ms));
        ux_led_set(false);
        if (i < cycles - 1) { // Don't delay after the last cycle
            vTaskDelay(pdMS_TO_TICKS(off_time_ms));
        }
    }
    
    return ESP_OK;
}

static esp_err_t ux_buzzer_tone(uint32_t frequency_hz, uint32_t duration_ms)
{
    if (!buzzer_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Set frequency
    esp_err_t ret = ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_1, frequency_hz);
    if (ret != ESP_OK) {
        ESP_LOGD(TAG, "LEDC freq set warning: %s", esp_err_to_name(ret));
    }
    
    // Set 50% duty cycle
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 4096); // 50% of 8192 (13-bit)
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
    
    if (duration_ms > 0) {
        vTaskDelay(pdMS_TO_TICKS(duration_ms));
        
        // Stop buzzer
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 0);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
    }
    
    return ESP_OK;
}

static esp_err_t ux_buzzer_stop(void)
{
    if (!buzzer_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
    
    return ESP_OK;
}

// Effect type to string conversion for human-friendly logging
static const char* ux_effect_to_string(ux_effect_t effect)
{
    static char buffer[64];
    
    switch (effect.device_type) {
        case UX_DEVICE_LED:
            switch (effect.led_effect) {
                case UX_LED_EFFECT_OFF:              return "🔴 LED OFF";
                case UX_LED_EFFECT_ON:               return "🔴 LED ON";
                case UX_LED_EFFECT_BLINK_FAST:       return "🔴 LED BLINK FAST";
                case UX_LED_EFFECT_BLINK_SLOW:       return "🔴 LED BLINK SLOW";
                case UX_LED_EFFECT_BREATHING:        return "🔴 LED BREATHING";
                case UX_LED_EFFECT_SUCCESS_PATTERN:  return "🔴 LED SUCCESS";
                case UX_LED_EFFECT_ERROR_PATTERN:    return "🔴 LED ERROR";
                default:                             return "🔴 LED UNKNOWN";
            }
            break;
            
        case UX_DEVICE_BUZZER:
            switch (effect.buzzer_effect) {
                case UX_BUZZER_EFFECT_SILENCE:       return "🔊 BUZZER SILENCE";
                case UX_BUZZER_EFFECT_STARTUP:       return "🔊 BUZZER STARTUP";
                case UX_BUZZER_EFFECT_SUCCESS:       return "🔊 BUZZER SUCCESS";
                case UX_BUZZER_EFFECT_ERROR:         return "🔊 BUZZER ERROR";
                case UX_BUZZER_EFFECT_NOTIFICATION:  return "🔊 BUZZER NOTIFICATION";
                case UX_BUZZER_EFFECT_WARNING:       return "🔊 BUZZER WARNING";
                case UX_BUZZER_EFFECT_CLICK:         return "🔊 BUZZER CLICK";
                default:                             return "🔊 BUZZER UNKNOWN";
            }
            break;
            
        case UX_DEVICE_NONE:
        default:
            snprintf(buffer, sizeof(buffer), "UNKNOWN DEVICE (%d)", effect.device_type);
            return buffer;
    }
}
