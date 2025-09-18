#include "button.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_timer.h"
#include <string.h>

static const char *TAG = "BUTTON";

// Button driver state
static bool button_initialized = false;
static button_callback_t interrupt_callback = NULL;
static bool interrupt_mode_enabled = false;

// Button states
static button_state_t button_states[BUTTON_COUNT];

// Interrupt queue for button events
static QueueHandle_t button_interrupt_queue = NULL;

// Button interrupt event structure
typedef struct {
    button_id_t button_id;
    uint32_t timestamp;
    bool pressed;
} button_interrupt_event_t;

// GPIO pin mapping
static const gpio_num_t button_pins[BUTTON_COUNT] = {
    BUTTON_A_PIN,  // BUTTON_A = GPIO37
    BUTTON_B_PIN   // BUTTON_B = GPIO39
};

// Button names for debugging
static const char* button_names[BUTTON_COUNT] = {
    "Button A",
    "Button B"
};

/**
 * @brief Get current timestamp in milliseconds
 */
static uint32_t get_timestamp_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

/**
 * @brief GPIO interrupt handler
 */
static void IRAM_ATTR button_gpio_isr_handler(void* arg)
{
    button_id_t button_id = (button_id_t)(uintptr_t)arg;
    button_interrupt_event_t event = {
        .button_id = button_id,
        .timestamp = get_timestamp_ms(),
        .pressed = (gpio_get_level(button_pins[button_id]) == BUTTON_PRESSED_LEVEL)
    };
    
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(button_interrupt_queue, &event, &xHigherPriorityTaskWoken);
    
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

/**
 * @brief Button interrupt processing task
 */
static void button_interrupt_task(void *pvParameters)
{
    button_interrupt_event_t event;
    
    while (1) {
        if (xQueueReceive(button_interrupt_queue, &event, portMAX_DELAY)) {
            if (!interrupt_mode_enabled || !button_initialized) {
                continue;
            }
            
            button_state_t *state = &button_states[event.button_id];
            uint32_t current_time = event.timestamp;
            
            // Debouncing
            if (state->current_state != event.pressed) {
                static uint32_t last_change_time[BUTTON_COUNT] = {0};
                
                if (current_time - last_change_time[event.button_id] < BUTTON_DEBOUNCE_MS) {
                    continue; // Ignore bouncing
                }
                
                last_change_time[event.button_id] = current_time;
                state->previous_state = state->current_state;
                state->current_state = event.pressed;
                
                if (event.pressed) {
                    // Button pressed
                    state->press_start_time = current_time;
                    state->press_count++;
                    state->long_press_triggered = false;
                    
                    ESP_LOGD(TAG, "%s pressed (interrupt)", button_get_name(event.button_id));
                    
                    if (interrupt_callback) {
                        interrupt_callback(event.button_id, BUTTON_EVENT_PRESSED, 0);
                    }
                } else {
                    // Button released
                    state->press_duration = current_time - state->press_start_time;
                    
                    ESP_LOGD(TAG, "%s released after %lums (interrupt)", 
                             button_get_name(event.button_id), state->press_duration);
                    
                    if (interrupt_callback) {
                        interrupt_callback(event.button_id, BUTTON_EVENT_RELEASED, state->press_duration);
                        
                        // Determine if it was a short or long press
                        if (state->press_duration >= BUTTON_LONG_PRESS_MS) {
                            if (!state->long_press_triggered) {
                                interrupt_callback(event.button_id, BUTTON_EVENT_LONG_PRESS, state->press_duration);
                                state->long_press_triggered = true;
                            }
                        } else {
                            interrupt_callback(event.button_id, BUTTON_EVENT_SHORT_PRESS, state->press_duration);
                        }
                    }
                }
            }
        }
    }
}

esp_err_t button_init(void)
{
    if (button_initialized) {
        ESP_LOGW(TAG, "Button driver already initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing button driver");
    
    // Initialize button states
    memset(button_states, 0, sizeof(button_states));
    
    // Configure GPIO pins (GPIO37/39 are input-only and don't have internal pull-up)
    gpio_config_t gpio_conf = {
        .pin_bit_mask = (1ULL << BUTTON_A_PIN) | (1ULL << BUTTON_B_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,   // Disable internal pull-up for input-only pins
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE,     // Interrupt on both edges
    };
    
    esp_err_t ret = gpio_config(&gpio_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GPIO: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Create interrupt queue
    button_interrupt_queue = xQueueCreate(10, sizeof(button_interrupt_event_t));
    if (button_interrupt_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create interrupt queue");
        return ESP_ERR_NO_MEM;
    }
    
    // Install GPIO ISR service
    ret = gpio_install_isr_service(ESP_INTR_FLAG_EDGE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install ISR service: %s", esp_err_to_name(ret));
        vQueueDelete(button_interrupt_queue);
        return ret;
    }
    
    // Add ISR handlers for each button
    for (int i = 0; i < BUTTON_COUNT; i++) {
        ret = gpio_isr_handler_add(button_pins[i], button_gpio_isr_handler, (void*)(uintptr_t)i);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to add ISR handler for %s: %s", 
                     button_get_name(i), esp_err_to_name(ret));
            button_deinit();
            return ret;
        }
    }
    
    // Create interrupt processing task with increased stack size for LVGL callbacks and logging
    BaseType_t task_ret = xTaskCreate(button_interrupt_task, "button_intr", 4096, NULL, 5, NULL);
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create interrupt task");
        button_deinit();
        return ESP_ERR_NO_MEM;
    }
    
    // Initialize button states with current GPIO levels
    for (int i = 0; i < BUTTON_COUNT; i++) {
        button_states[i].current_state = (gpio_get_level(button_pins[i]) == BUTTON_PRESSED_LEVEL);
        button_states[i].previous_state = button_states[i].current_state;
    }
    
    button_initialized = true;
    ESP_LOGI(TAG, "Button driver initialized successfully");
    ESP_LOGI(TAG, "Button A: GPIO%d, Button B: GPIO%d", BUTTON_A_PIN, BUTTON_B_PIN);
    ESP_LOGI(TAG, "Current states - A: %s, B: %s", 
             button_states[BUTTON_A].current_state ? "PRESSED" : "RELEASED",
             button_states[BUTTON_B].current_state ? "PRESSED" : "RELEASED");
    
    return ESP_OK;
}

esp_err_t button_deinit(void)
{
    if (!button_initialized) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Deinitializing button driver");
    
    // Disable interrupt mode
    interrupt_mode_enabled = false;
    interrupt_callback = NULL;
    
    // Remove ISR handlers
    for (int i = 0; i < BUTTON_COUNT; i++) {
        gpio_isr_handler_remove(button_pins[i]);
    }
    
    // Uninstall GPIO ISR service
    gpio_uninstall_isr_service();
    
    // Clean up queue
    if (button_interrupt_queue) {
        vQueueDelete(button_interrupt_queue);
        button_interrupt_queue = NULL;
    }
    
    button_initialized = false;
    ESP_LOGI(TAG, "Button driver deinitialized");
    
    return ESP_OK;
}

esp_err_t button_set_interrupt_callback(button_callback_t callback)
{
    if (!button_initialized) {
        ESP_LOGE(TAG, "Button driver not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    interrupt_callback = callback;
    ESP_LOGI(TAG, "Interrupt callback %s", callback ? "set" : "cleared");
    
    return ESP_OK;
}

esp_err_t button_set_interrupt_mode(bool enable)
{
    if (!button_initialized) {
        ESP_LOGE(TAG, "Button driver not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    interrupt_mode_enabled = enable;
    ESP_LOGI(TAG, "Interrupt mode %s", enable ? "enabled" : "disabled");
    
    return ESP_OK;
}

bool button_is_pressed(button_id_t button_id)
{
    if (!button_initialized || button_id >= BUTTON_COUNT) {
        return false;
    }
    
    return (gpio_get_level(button_pins[button_id]) == BUTTON_PRESSED_LEVEL);
}

button_event_t button_poll_event(button_id_t button_id)
{
    if (!button_initialized || button_id >= BUTTON_COUNT) {
        return BUTTON_EVENT_NONE;
    }
    
    button_state_t *state = &button_states[button_id];
    bool current_pressed = button_is_pressed(button_id);
    uint32_t current_time = get_timestamp_ms();
    
    // Debouncing for polling mode
    static uint32_t last_poll_time[BUTTON_COUNT] = {0};
    if (current_time - last_poll_time[button_id] < BUTTON_DEBOUNCE_MS) {
        return BUTTON_EVENT_NONE;
    }
    
    button_event_t event = BUTTON_EVENT_NONE;
    
    if (current_pressed != state->current_state) {
        last_poll_time[button_id] = current_time;
        state->previous_state = state->current_state;
        state->current_state = current_pressed;
        
        if (current_pressed) {
            // Button just pressed
            state->press_start_time = current_time;
            state->press_count++;
            state->long_press_triggered = false;
            event = BUTTON_EVENT_PRESSED;
            
            ESP_LOGI(TAG, "%s pressed (poll)", button_get_name(button_id));
        } else {
            // Button just released
            state->press_duration = current_time - state->press_start_time;
            event = BUTTON_EVENT_RELEASED;
            
            ESP_LOGI(TAG, "%s released after %lums (poll)", 
                     button_get_name(button_id), state->press_duration);
        }
    } else if (current_pressed && !state->long_press_triggered) {
        // Check for long press
        uint32_t press_duration = current_time - state->press_start_time;
        if (press_duration >= BUTTON_LONG_PRESS_MS) {
            state->long_press_triggered = true;
            event = BUTTON_EVENT_LONG_PRESS;
            
            ESP_LOGI(TAG, "%s long press detected (poll)", button_get_name(button_id));
        }
    }
    
    return event;
}

esp_err_t button_get_state(button_id_t button_id, button_state_t *state)
{
    if (!button_initialized || button_id >= BUTTON_COUNT || !state) {
        return ESP_ERR_INVALID_ARG;
    }
    
    *state = button_states[button_id];
    return ESP_OK;
}

uint32_t button_get_press_count(button_id_t button_id)
{
    if (!button_initialized || button_id >= BUTTON_COUNT) {
        return 0;
    }
    
    return button_states[button_id].press_count;
}

esp_err_t button_reset_press_count(button_id_t button_id)
{
    if (!button_initialized || button_id >= BUTTON_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    
    uint32_t old_count = button_states[button_id].press_count;
    button_states[button_id].press_count = 0;
    
    ESP_LOGI(TAG, "%s press count reset from %lu to 0", button_get_name(button_id), old_count);
    
    return ESP_OK;
}

const char* button_get_name(button_id_t button_id)
{
    if (button_id >= BUTTON_COUNT) {
        return "Unknown";
    }
    
    return button_names[button_id];
}

/**
 * @brief Button test callback for interrupt mode
 */
static void button_test_callback(button_id_t button_id, button_event_t event, uint32_t press_duration)
{
    const char* event_names[] = {
        "PRESSED", "RELEASED", "SHORT_PRESS", "LONG_PRESS", "NONE"
    };
    
    ESP_LOGI(TAG, "TEST CALLBACK: %s - %s (duration: %lums)", 
             button_get_name(button_id), 
             event_names[event], 
             press_duration);
}

esp_err_t button_test_all_functions(void)
{
    if (!button_initialized) {
        ESP_LOGE(TAG, "Button driver not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Starting comprehensive button test");
    
    // Test 1: Basic state reading
    ESP_LOGI(TAG, "Test 1: Basic button state reading");
    for (int i = 0; i < BUTTON_COUNT; i++) {
        bool pressed = button_is_pressed(i);
        uint32_t count = button_get_press_count(i);
        ESP_LOGI(TAG, "  %s: %s (press count: %lu)", 
                 button_get_name(i), 
                 pressed ? "PRESSED" : "RELEASED", 
                 count);
    }
    
    // Test 2: Interrupt mode
    ESP_LOGI(TAG, "Test 2: Interrupt mode (10 seconds)");
    button_set_interrupt_callback(button_test_callback);
    button_set_interrupt_mode(true);
    ESP_LOGI(TAG, "  Press any button to test interrupts...");
    vTaskDelay(pdMS_TO_TICKS(10000));
    button_set_interrupt_mode(false);
    
    // Test 3: Polling mode
    ESP_LOGI(TAG, "Test 3: Polling mode (10 seconds)");
    ESP_LOGI(TAG, "  Press any button to test polling...");
    
    uint32_t start_time = get_timestamp_ms();
    while (get_timestamp_ms() - start_time < 10000) {
        for (int i = 0; i < BUTTON_COUNT; i++) {
            button_event_t event = button_poll_event(i);
            if (event != BUTTON_EVENT_NONE) {
                button_state_t state;
                button_get_state(i, &state);
                ESP_LOGI(TAG, "  POLL EVENT: %s - Event %d (duration: %lums)", 
                         button_get_name(i), event, state.press_duration);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50)); // Poll every 50ms
    }
    
    // Test 4: Final state report
    ESP_LOGI(TAG, "Test 4: Final state report");
    for (int i = 0; i < BUTTON_COUNT; i++) {
        button_state_t state;
        button_get_state(i, &state);
        ESP_LOGI(TAG, "  %s: pressed=%s, count=%lu, last_duration=%lums", 
                 button_get_name(i),
                 state.current_state ? "true" : "false",
                 state.press_count,
                 state.press_duration);
    }
    
    ESP_LOGI(TAG, "Button test completed successfully");
    return ESP_OK;
}