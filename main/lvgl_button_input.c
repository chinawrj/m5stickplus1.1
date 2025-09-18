/**
 * @file lvgl_button_input.c
 * @brief LVGL Input Device Driver Implementation for M5StickC Plus 1.1 GPIO Buttons
 * 
 * This driver bridges the GPIO button system with LVGL's input device framework,
 * providing thread-safe button press detection and key event generation.
 */

#include "lvgl_button_input.h"
#include "button.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "LVGL_BTN_INPUT";

// Input device state
static bool g_input_initialized = false;
static bool g_input_enabled = true;
static lv_indev_t *g_indev = NULL;

// Button state tracking (thread-safe)
static SemaphoreHandle_t g_button_mutex = NULL;
static volatile lvgl_key_t g_current_key = LVGL_KEY_NONE;
static volatile lvgl_key_t g_last_key = LVGL_KEY_NONE;
static volatile lv_indev_state_t g_key_state = LV_INDEV_STATE_RELEASED;

// Statistics tracking
static uint32_t g_button_a_count = 0;
static uint32_t g_button_b_count = 0;

/**
 * @brief Thread-safe button state update
 */
static void update_button_state(lvgl_key_t key, lv_indev_state_t state)
{
    if (g_button_mutex && xSemaphoreTake(g_button_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        g_current_key = key;
        g_key_state = state;
        
        if (state == LV_INDEV_STATE_PRESSED) {
            g_last_key = key;
            
            // Update statistics
            if (key == LVGL_KEY_OK) {
                g_button_a_count++;
            } else if (key == LVGL_KEY_NEXT) {
                g_button_b_count++;
            }
            
            ESP_LOGI(TAG, "Button pressed: %s (total A:%lu B:%lu)", 
                     (key == LVGL_KEY_OK) ? "OK/ENTER" : "RIGHT",
                     g_button_a_count, g_button_b_count);
        } else {
            ESP_LOGI(TAG, "Button released: %s", 
                     (key == LVGL_KEY_OK) ? "OK/ENTER" : "RIGHT");
        }
        
        xSemaphoreGive(g_button_mutex);
    }
}

/**
 * @brief GPIO button callback - converts button events to LVGL keys
 * 
 * This callback is called from GPIO interrupt context and must be ISR-safe.
 * It converts the hardware button events into LVGL key events.
 * 
 * @param button_id Which button triggered the event
 * @param event Type of button event (press/release/long press)
 * @param press_duration Duration of press if applicable
 */
static void button_to_lvgl_callback(button_id_t button_id, button_event_t event, uint32_t press_duration)
{
    if (!g_input_enabled) {
        return;
    }
    
    lvgl_key_t key = LVGL_KEY_NONE;
    lv_indev_state_t state = LV_INDEV_STATE_RELEASED;
    
    // Map hardware buttons to LVGL keys
    switch (button_id) {
        case BUTTON_A:
            key = LVGL_KEY_OK;  // Button A = OK/Enter
            break;
        case BUTTON_B:
            key = LVGL_KEY_NEXT;  // Button B = Next
            break;
        default:
            ESP_LOGW(TAG, "Unknown button ID: %d", button_id);
            return;
    }
    
    // Map button events to LVGL states
    switch (event) {
        case BUTTON_EVENT_PRESSED:
            state = LV_INDEV_STATE_PRESSED;
            break;
        case BUTTON_EVENT_RELEASED:
        case BUTTON_EVENT_SHORT_PRESS:
        case BUTTON_EVENT_LONG_PRESS:
            state = LV_INDEV_STATE_RELEASED;
            break;
        default:
            return;  // Ignore other events
    }
    
    // Update button state (thread-safe)
    update_button_state(key, state);
}

/**
 * @brief LVGL input device read callback for keypad
 * 
 * This function is called by LVGL to read key states from our button system.
 * It converts GPIO button states to LVGL key events.
 */
static void lvgl_keypad_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;  // Unused parameter
    
    // Thread-safe state reading
    if (xSemaphoreTake(g_button_mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        // If mutex not available, return previous state
        data->state = g_key_state;
        data->key = (g_last_key == LVGL_KEY_OK) ? LV_KEY_ENTER : 
                    (g_last_key == LVGL_KEY_NEXT) ? LV_KEY_NEXT : 0;
        return;
    }
    
    uint32_t current_key = 0;
    lv_indev_state_t current_state = g_key_state;
    
    // Convert our internal key representation to LVGL keys
    if (g_current_key == LVGL_KEY_OK) {
        current_key = LV_KEY_ENTER;
    } else if (g_current_key == LVGL_KEY_NEXT) {
        current_key = LV_KEY_RIGHT;  // Button B now maps to RIGHT arrow
    }
    
    xSemaphoreGive(g_button_mutex);
    
    // Set LVGL data
    data->key = current_key;
    data->state = current_state;
    
    // Smart logging - only log meaningful state changes
    static lv_indev_state_t last_logged_state = LV_INDEV_STATE_RELEASED;
    static uint32_t last_logged_key = 0;
    static bool meaningful_change = false;
    
    // Detect meaningful state changes (ignore no-key polling)
    if (current_key != 0 || last_logged_key != 0) {
        if (current_state != last_logged_state || current_key != last_logged_key) {
            meaningful_change = true;
            last_logged_state = current_state;
            last_logged_key = current_key;
        }
    }
    
    // Only log meaningful events
    if (meaningful_change) {
        if (current_key != 0) {
            ESP_LOGI(TAG, "🔑 LVGL read: key=%lu, state=%s [STATE CHANGE]", 
                     current_key, 
                     current_state == LV_INDEV_STATE_PRESSED ? "PRESSED" : "RELEASED");
        } else {
            ESP_LOGI(TAG, "🔑 LVGL read: key=0, state=RELEASED [CLEARED]");
        }
        meaningful_change = false; // Reset flag after logging
    }
    
    // Optional: Very occasional heartbeat for debugging (every ~30 seconds)
    static uint32_t heartbeat_counter = 0;
    if (current_key == 0 && (heartbeat_counter++ % 1000) == 0) {
        ESP_LOGI(TAG, "LVGL heartbeat: no keys pressed");
    }
    
    // Critical: Clear key state after RELEASED to prevent repeated reads
    if (current_state == LV_INDEV_STATE_RELEASED && current_key != 0) {
        ESP_LOGI(TAG, "Key cleared - resetting to no-key state");
        // Take mutex again to clear state
        if (xSemaphoreTake(g_button_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            g_current_key = LVGL_KEY_NONE;
            g_key_state = LV_INDEV_STATE_RELEASED;
            xSemaphoreGive(g_button_mutex);
        }
    }
}

// Public API implementation

esp_err_t lvgl_button_input_init(void)
{
    ESP_LOGI(TAG, "Initializing LVGL button input device...");
    
    if (g_input_initialized) {
        ESP_LOGW(TAG, "LVGL button input already initialized");
        return ESP_OK;
    }
    
    // Create mutex for thread-safe access
    g_button_mutex = xSemaphoreCreateMutex();
    if (g_button_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create button mutex");
        return ESP_ERR_NO_MEM;
    }
    
    // Initialize GPIO button system if not already done
    esp_err_t ret = button_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize button system: %s", esp_err_to_name(ret));
        vSemaphoreDelete(g_button_mutex);
        g_button_mutex = NULL;
        return ret;
    }
    
    // Set up button callback for LVGL key events
    ret = button_set_interrupt_callback(button_to_lvgl_callback);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set button callback: %s", esp_err_to_name(ret));
        button_deinit();
        vSemaphoreDelete(g_button_mutex);
        g_button_mutex = NULL;
        return ret;
    }
    
    // Enable button interrupt mode
    ret = button_set_interrupt_mode(true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable button interrupts: %s", esp_err_to_name(ret));
        button_set_interrupt_callback(NULL);
        button_deinit();
        vSemaphoreDelete(g_button_mutex);
        g_button_mutex = NULL;
        return ret;
    }
    
    // Create and register LVGL input device (LVGL 9.x API)
    g_indev = lv_indev_create();
    if (g_indev == NULL) {
        ESP_LOGE(TAG, "Failed to create LVGL input device");
        button_set_interrupt_mode(false);
        button_set_interrupt_callback(NULL);
        button_deinit();
        vSemaphoreDelete(g_button_mutex);
        g_button_mutex = NULL;
        return ESP_FAIL;
    }
    
    lv_indev_set_type(g_indev, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(g_indev, lvgl_keypad_read_cb);
    
    // Initialize state
    g_current_key = LVGL_KEY_NONE;
    g_last_key = LVGL_KEY_NONE;
    g_key_state = LV_INDEV_STATE_RELEASED;
    g_input_enabled = true;
    g_button_a_count = 0;
    g_button_b_count = 0;
    
    g_input_initialized = true;
    
    ESP_LOGI(TAG, "LVGL button input device initialized successfully");
        ESP_LOGI(TAG, "Button mapping: A=OK/ENTER, B=RIGHT");
    
    return ESP_OK;
}

lv_indev_t* lvgl_button_input_get_device(void)
{
    return g_indev;
}

void lvgl_button_input_set_enabled(bool enabled)
{
    g_input_enabled = enabled;
    ESP_LOGI(TAG, "LVGL button input %s", enabled ? "enabled" : "disabled");
    
    // Clear current state when disabling
    if (!enabled && g_button_mutex) {
        if (xSemaphoreTake(g_button_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            g_current_key = LVGL_KEY_NONE;
            g_key_state = LV_INDEV_STATE_RELEASED;
            xSemaphoreGive(g_button_mutex);
        }
    }
}

bool lvgl_button_input_is_enabled(void)
{
    return g_input_enabled;
}

lvgl_key_t lvgl_button_input_get_last_key(void)
{
    return g_last_key;
}

void lvgl_button_input_get_stats(uint32_t *button_a_count, uint32_t *button_b_count)
{
    if (button_a_count) {
        *button_a_count = g_button_a_count;
    }
    if (button_b_count) {
        *button_b_count = g_button_b_count;
    }
}

esp_err_t lvgl_button_input_deinit(void)
{
    ESP_LOGI(TAG, "Deinitializing LVGL button input device...");
    
    if (!g_input_initialized) {
        return ESP_OK;
    }
    
    g_input_enabled = false;
    
    // Delete LVGL input device
    if (g_indev) {
        lv_indev_delete(g_indev);
        g_indev = NULL;
    }
    
    // Disable button interrupts and callbacks
    button_set_interrupt_mode(false);
    button_set_interrupt_callback(NULL);
    
    // Clean up mutex
    if (g_button_mutex) {
        vSemaphoreDelete(g_button_mutex);
        g_button_mutex = NULL;
    }
    
    g_input_initialized = false;
    
    ESP_LOGI(TAG, "LVGL button input device deinitialized");
    return ESP_OK;
}