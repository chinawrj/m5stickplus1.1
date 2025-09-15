#include "red_led.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include <string.h>

static const char *TAG = "RED_LED";

// LED state variables
static bool led_initialized = false;
static bool led_current_state = false;  // false = OFF, true = ON

// Morse code lookup table
static const char* morse_code[] = {
    ".-",    // A
    "-...",  // B
    "-.-.",  // C
    "-..",   // D
    ".",     // E
    "..-.",  // F
    "--.",   // G
    "....",  // H
    "..",    // I
    ".---",  // J
    "-.-",   // K
    ".-..",  // L
    "--",    // M
    "-.",    // N
    "---",   // O
    ".--.",  // P
    "--.-",  // Q
    ".-.",   // R
    "...",   // S
    "-",     // T
    "..-",   // U
    "...-",  // V
    ".--",   // W
    "-..-",  // X
    "-.--",  // Y
    "--..",  // Z
    "-----", // 0
    ".----", // 1
    "..---", // 2
    "...--", // 3
    "....-", // 4
    ".....", // 5
    "-....", // 6
    "--...", // 7
    "---..", // 8
    "----."  // 9
};

/**
 * @brief Initialize the red LED
 */
esp_err_t red_led_init(void)
{
    ESP_LOGI(TAG, "Initializing red LED on GPIO%d", RED_LED_PIN);
    
    // Configure GPIO pin
    gpio_config_t gpio_conf = {
        .pin_bit_mask = (1ULL << RED_LED_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    
    esp_err_t ret = gpio_config(&gpio_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GPIO%d: %s", RED_LED_PIN, esp_err_to_name(ret));
        return ret;
    }
    
    // Set initial state (OFF) directly via GPIO
    ret = gpio_set_level(RED_LED_PIN, RED_LED_OFF_LEVEL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set initial LED state: %s", esp_err_to_name(ret));
        return ret;
    }
    
    led_initialized = true;
    led_current_state = false;  // LED is OFF
    ESP_LOGI(TAG, "Red LED initialized successfully (active LOW)");
    
    return ESP_OK;
}

/**
 * @brief Deinitialize the red LED
 */
esp_err_t red_led_deinit(void)
{
    if (!led_initialized) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Deinitializing red LED");
    
    // Turn off LED
    red_led_off();
    
    // Reset GPIO to input mode to reduce power consumption
    gpio_reset_pin(RED_LED_PIN);
    
    led_initialized = false;
    ESP_LOGI(TAG, "Red LED deinitialized");
    
    return ESP_OK;
}

/**
 * @brief Turn on the red LED
 */
esp_err_t red_led_on(void)
{
    if (!led_initialized) {
        ESP_LOGE(TAG, "LED not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t ret = gpio_set_level(RED_LED_PIN, RED_LED_ON_LEVEL);
    if (ret == ESP_OK) {
        led_current_state = true;
        ESP_LOGD(TAG, "LED turned ON");
    }
    
    return ret;
}

/**
 * @brief Turn off the red LED
 */
esp_err_t red_led_off(void)
{
    if (!led_initialized) {
        ESP_LOGE(TAG, "LED not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t ret = gpio_set_level(RED_LED_PIN, RED_LED_OFF_LEVEL);
    if (ret == ESP_OK) {
        led_current_state = false;
        ESP_LOGD(TAG, "LED turned OFF");
    }
    
    return ret;
}

/**
 * @brief Toggle the red LED state
 */
esp_err_t red_led_toggle(void)
{
    if (!led_initialized) {
        ESP_LOGE(TAG, "LED not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (led_current_state) {
        return red_led_off();
    } else {
        return red_led_on();
    }
}

/**
 * @brief Set LED state
 */
esp_err_t red_led_set_state(bool state)
{
    if (state) {
        return red_led_on();
    } else {
        return red_led_off();
    }
}

/**
 * @brief Get current LED state
 */
bool red_led_get_state(void)
{
    return led_current_state;
}

/**
 * @brief Blink LED with custom pattern
 */
esp_err_t red_led_blink_pattern(uint32_t on_time_ms, uint32_t off_time_ms, uint32_t repeat_count)
{
    if (!led_initialized) {
        ESP_LOGE(TAG, "LED not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGD(TAG, "Blinking: ON %lums, OFF %lums, repeat %lu times", on_time_ms, off_time_ms, repeat_count);
    
    uint32_t cycles = (repeat_count == 0) ? UINT32_MAX : repeat_count;
    
    for (uint32_t i = 0; i < cycles; i++) {
        // LED ON
        esp_err_t ret = red_led_on();
        if (ret != ESP_OK) return ret;
        vTaskDelay(pdMS_TO_TICKS(on_time_ms));
        
        // LED OFF
        ret = red_led_off();
        if (ret != ESP_OK) return ret;
        vTaskDelay(pdMS_TO_TICKS(off_time_ms));
        
        // For infinite loop, check if we should break (this is a simple implementation)
        if (repeat_count == 0 && i > 1000) {
            ESP_LOGW(TAG, "Breaking infinite blink loop after 1000 cycles");
            break;
        }
    }
    
    return ESP_OK;
}

/**
 * @brief Simple blink function
 */
esp_err_t red_led_blink(uint32_t count, uint32_t interval_ms)
{
    return red_led_blink_pattern(interval_ms, interval_ms, count);
}

/**
 * @brief Set LED blinking state with preset patterns
 */
esp_err_t red_led_set_blink_state(led_state_t state)
{
    if (!led_initialized) {
        ESP_LOGE(TAG, "LED not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    switch (state) {
        case LED_STATE_OFF:
            return red_led_off();
            
        case LED_STATE_ON:
            return red_led_on();
            
        case LED_STATE_BLINK_FAST:
            return red_led_blink_pattern(BLINK_FAST_MS, BLINK_FAST_MS, 5);
            
        case LED_STATE_BLINK_NORMAL:
            return red_led_blink_pattern(BLINK_NORMAL_MS, BLINK_NORMAL_MS, 3);
            
        case LED_STATE_BLINK_SLOW:
            return red_led_blink_pattern(BLINK_SLOW_MS, BLINK_SLOW_MS, 2);
            
        case LED_STATE_BLINK_VERY_SLOW:
            return red_led_blink_pattern(BLINK_VERY_SLOW_MS, BLINK_VERY_SLOW_MS, 1);
            
        default:
            ESP_LOGE(TAG, "Invalid LED state: %d", state);
            return ESP_ERR_INVALID_ARG;
    }
}

/**
 * @brief Stop LED operations
 */
esp_err_t red_led_stop(void)
{
    return red_led_off();
}

/**
 * @brief LED breathing effect
 */
esp_err_t red_led_breathing(uint32_t cycles, uint32_t cycle_duration_ms)
{
    if (!led_initialized) {
        ESP_LOGE(TAG, "LED not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Starting breathing effect: %lu cycles, %lums per cycle", cycles, cycle_duration_ms);
    
    for (uint32_t cycle = 0; cycle < cycles; cycle++) {
        uint32_t steps = 10;  // Reduced number of steps
        uint32_t half_cycle = cycle_duration_ms / 2;  // Time for fade in or fade out
        uint32_t step_duration = half_cycle / steps;
        
        ESP_LOGD(TAG, "Cycle %lu: %lu steps, %lums per step", cycle + 1, steps, step_duration);
        
        // Fade in (increasing on time)
        for (uint32_t step = 1; step <= steps; step++) {
            uint32_t on_time = (step * step_duration) / steps;
            uint32_t off_time = step_duration - on_time;
            
            if (on_time > 0) {
                red_led_on();
                vTaskDelay(pdMS_TO_TICKS(on_time));
            }
            if (off_time > 0) {
                red_led_off();
                vTaskDelay(pdMS_TO_TICKS(off_time));
            }
        }
        
        // Fade out (decreasing on time)
        for (uint32_t step = steps; step > 0; step--) {
            uint32_t on_time = (step * step_duration) / steps;
            uint32_t off_time = step_duration - on_time;
            
            if (on_time > 0) {
                red_led_on();
                vTaskDelay(pdMS_TO_TICKS(on_time));
            }
            if (off_time > 0) {
                red_led_off();
                vTaskDelay(pdMS_TO_TICKS(off_time));
            }
        }
        
        // Brief pause between cycles
        red_led_off();
        vTaskDelay(pdMS_TO_TICKS(200));
        ESP_LOGD(TAG, "Completed cycle %lu", cycle + 1);
    }
    
    ESP_LOGI(TAG, "Breathing effect completed");
    
    return ESP_OK;
}

/**
 * @brief Morse code transmission
 */
esp_err_t red_led_morse_code(const char* message, uint32_t dot_duration_ms)
{
    if (!led_initialized) {
        ESP_LOGE(TAG, "LED not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (message == NULL) {
        ESP_LOGE(TAG, "Message is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Transmitting Morse code: \"%s\"", message);
    
    uint32_t dash_duration = dot_duration_ms * 3;
    uint32_t inter_element_gap = dot_duration_ms;
    uint32_t inter_letter_gap = dot_duration_ms * 3;
    uint32_t inter_word_gap = dot_duration_ms * 7;
    
    for (int i = 0; message[i] != '\0'; i++) {
        char c = message[i];
        
        if (c == ' ') {
            // Word gap
            vTaskDelay(pdMS_TO_TICKS(inter_word_gap));
            continue;
        }
        
        const char* code = NULL;
        
        // Convert to uppercase and get morse code
        if (c >= 'a' && c <= 'z') {
            code = morse_code[c - 'a'];
        } else if (c >= 'A' && c <= 'Z') {
            code = morse_code[c - 'A'];
        } else if (c >= '0' && c <= '9') {
            code = morse_code[26 + (c - '0')];
        }
        
        if (code == NULL) {
            ESP_LOGW(TAG, "Unsupported character: '%c'", c);
            continue;
        }
        
        ESP_LOGD(TAG, "Character '%c' -> %s", c, code);
        
        // Transmit morse code for this character
        for (int j = 0; code[j] != '\0'; j++) {
            if (code[j] == '.') {
                // Dot
                red_led_on();
                vTaskDelay(pdMS_TO_TICKS(dot_duration_ms));
                red_led_off();
            } else if (code[j] == '-') {
                // Dash
                red_led_on();
                vTaskDelay(pdMS_TO_TICKS(dash_duration));
                red_led_off();
            }
            
            // Inter-element gap (except after last element)
            if (code[j + 1] != '\0') {
                vTaskDelay(pdMS_TO_TICKS(inter_element_gap));
            }
        }
        
        // Inter-letter gap (except after last letter)
        if (message[i + 1] != '\0' && message[i + 1] != ' ') {
            vTaskDelay(pdMS_TO_TICKS(inter_letter_gap));
        }
    }
    
    return ESP_OK;
}

/**
 * @brief Status indication patterns
 */
esp_err_t red_led_indicate_boot(void)
{
    ESP_LOGI(TAG, "Boot sequence indication");
    // Fast blinks followed by slow blinks
    red_led_blink(5, 100);
    vTaskDelay(pdMS_TO_TICKS(300));
    red_led_blink(3, 300);
    return ESP_OK;
}

esp_err_t red_led_indicate_success(void)
{
    ESP_LOGI(TAG, "Success indication");
    // Three quick blinks
    red_led_blink(3, 150);
    return ESP_OK;
}

esp_err_t red_led_indicate_error(void)
{
    ESP_LOGI(TAG, "Error indication");
    // Long blinks
    red_led_blink(5, 500);
    return ESP_OK;
}

esp_err_t red_led_indicate_warning(void)
{
    ESP_LOGI(TAG, "Warning indication");
    // Double blink pattern
    for (int i = 0; i < 3; i++) {
        red_led_blink(2, 100);
        vTaskDelay(pdMS_TO_TICKS(300));
    }
    return ESP_OK;
}

esp_err_t red_led_indicate_processing(void)
{
    ESP_LOGI(TAG, "Processing indication");
    // Breathing effect
    red_led_breathing(2, 1000);
    return ESP_OK;
}

/**
 * @brief Comprehensive LED test patterns
 */
esp_err_t red_led_test_patterns(void)
{
    if (!led_initialized) {
        ESP_LOGE(TAG, "LED not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Starting comprehensive LED test patterns");
    
    // Test 1: Basic on/off
    ESP_LOGI(TAG, "Test 1: Basic ON/OFF control");
    red_led_on();
    vTaskDelay(pdMS_TO_TICKS(500));
    red_led_off();
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Test 2: Toggle functionality
    ESP_LOGI(TAG, "Test 2: Toggle functionality");
    for (int i = 0; i < 5; i++) {
        red_led_toggle();
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    red_led_off();
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Test 3: Different blink patterns
    ESP_LOGI(TAG, "Test 3: Blink patterns");
    ESP_LOGI(TAG, "  Fast blink");
    red_led_set_blink_state(LED_STATE_BLINK_FAST);
    vTaskDelay(pdMS_TO_TICKS(500));
    
    ESP_LOGI(TAG, "  Normal blink");
    red_led_set_blink_state(LED_STATE_BLINK_NORMAL);
    vTaskDelay(pdMS_TO_TICKS(800));
    
    ESP_LOGI(TAG, "  Slow blink");
    red_led_set_blink_state(LED_STATE_BLINK_SLOW);
    vTaskDelay(pdMS_TO_TICKS(1200));
    
    // Test 4: Status indications
    ESP_LOGI(TAG, "Test 4: Status indication patterns");
    ESP_LOGI(TAG, "  Boot indication");
    red_led_indicate_boot();
    vTaskDelay(pdMS_TO_TICKS(500));
    
    ESP_LOGI(TAG, "  Success indication");
    red_led_indicate_success();
    vTaskDelay(pdMS_TO_TICKS(500));
    
    ESP_LOGI(TAG, "  Warning indication");
    red_led_indicate_warning();
    vTaskDelay(pdMS_TO_TICKS(500));
    
    ESP_LOGI(TAG, "  Processing indication (breathing)");
    red_led_indicate_processing();
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Test 5: Morse code demo
    ESP_LOGI(TAG, "Test 5: Morse code transmission");
    ESP_LOGI(TAG, "  Transmitting 'SOS'");
    red_led_morse_code("SOS", 200);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    ESP_LOGI(TAG, "  Transmitting 'M5'");
    red_led_morse_code("M5", 150);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Test 6: Final success indication
    ESP_LOGI(TAG, "Test 6: Final success indication");
    red_led_indicate_success();
    
    ESP_LOGI(TAG, "All LED test patterns completed successfully");
    
    return ESP_OK;
}