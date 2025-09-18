/**
 * @file lvgl_button_input.c
 * @brief LVGL Input Device Driver Implementation for M5StickC Plus 1.1 GPIO Buttons
 * 
 * This driver b    // Update button state (ISR-safe via message buffer)
    send_button_event_from_isr(key, state);
    
    // 🔥 事件驱动模式：主动触发LVGL立即读取（基于官方SDL示例）
    if (g_indev && g_input_enabled) {
        lv_indev_read(g_indev);
        ESP_LOGD(TAG, "🚀 Event-driven: Triggered immediate LVGL read for key=%d, state=%d", 
                 (int)key, (int)state);
    }e GPIO button system with LVGL's input device framework,
 * providing thread-safe button press detection and key event generation.
 */

#include "lvgl_button_input.h"
#include "button.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/message_buffer.h"
#include <string.h>

static const char *TAG = "LVGL_BTN_INPUT";

// Input device state
static bool g_input_initialized = false;
static bool g_input_enabled = true;
static lv_indev_t *g_indev = NULL;

// Button event structure for message buffer
typedef struct {
    lvgl_key_t key;
    lv_indev_state_t state;
} button_event_data_t;

// Message buffer for ISR-safe button events (single producer/consumer)
static MessageBufferHandle_t g_button_message_buffer = NULL;
#define BUTTON_MESSAGE_BUFFER_SIZE (sizeof(button_event_data_t) * 2 + 8)  // Max 2 messages + header overhead
#define BUTTON_EVENT_SIZE sizeof(button_event_data_t)

// Fallback state for when stream buffer is empty
static volatile lvgl_key_t g_last_key = LVGL_KEY_NONE;

// Statistics tracking
static uint32_t g_button_a_count = 0;
static uint32_t g_button_b_count = 0;

/**
 * @brief ISR-safe button event sender using Message Buffer
 * 
 * Sends button events from ISR to LVGL task using single-producer/single-consumer
 * message buffer. This ensures message integrity and is interrupt-safe.
 */
static void send_button_event_from_isr(lvgl_key_t key, lv_indev_state_t state)
{
    if (g_button_message_buffer == NULL) {
        return;
    }
    
    button_event_data_t event = {
        .key = key,
        .state = state
    };
    
    // Send from ISR - non-blocking, ensures message integrity
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    size_t bytes_sent = xMessageBufferSendFromISR(g_button_message_buffer, 
                                                   &event, 
                                                   BUTTON_EVENT_SIZE, 
                                                   &xHigherPriorityTaskWoken);
    
    if (bytes_sent == BUTTON_EVENT_SIZE) {
        g_last_key = key;  // Update fallback state
        if (state == LV_INDEV_STATE_PRESSED) {
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
    } else {
        ESP_LOGW(TAG, "Message buffer full, button event dropped");
    }
    
    // Wake higher priority task if needed
    if (xHigherPriorityTaskWoken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

/**
 * @brief GPIO button callback - converts button events to LVGL keys (ISR CONTEXT)
 * 
 * This callback is called from GPIO interrupt context and must be ISR-safe.
 * It converts the hardware button events into LVGL key events and sends them
 * via message buffer to the LVGL task.
 * 
 * @param button_id Which button triggered the event
 * @param event Type of button event (press/release/long press)
 * @param press_duration Duration of press if applicable
 */
static void button_to_lvgl_callback_isr(button_id_t button_id, button_event_t event, uint32_t press_duration)
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
    
    // Update button state (ISR-safe via message buffer)
    send_button_event_from_isr(key, state);
    
    // 🔥 事件驱动模式：主动触发LVGL立即读取（基于官方SDL示例）
    if (g_indev && g_input_enabled) {
        lv_indev_read(g_indev);
        ESP_LOGD(TAG, "🚀 Event-driven: Triggered immediate LVGL read for key=%d, state=%d", 
                 (int)key, (int)state);
    }
}

/**
 * @brief LVGL input device read callback for keypad (EVENT-DRIVEN MODE)
 * 
 * This function is called by LVGL when lv_indev_read() is triggered from
 * button interrupt handlers. In event-driven mode, this is only called
 * when there are actual hardware events to process.
 */
static void lvgl_keypad_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;  // Unused parameter
    
    button_event_data_t event;
    
    // Try to receive complete message from message buffer (non-blocking)
    size_t bytes_received = xMessageBufferReceive(g_button_message_buffer, 
                                                   &event, 
                                                   BUTTON_EVENT_SIZE, 
                                                   0);  // Non-blocking
    
    if (bytes_received == BUTTON_EVENT_SIZE) {
        // Valid complete message received from message buffer
        data->state = event.state;
        
        // Convert LVGL keys to LVGL key codes
        if (event.key == LVGL_KEY_OK) {
            data->key = LV_KEY_ENTER;
        } else if (event.key == LVGL_KEY_NEXT) {
            data->key = LV_KEY_RIGHT;  // Button B maps to RIGHT arrow
        } else {
            data->key = 0;
        }
        
        // Log event-driven behavior
        if (event.state == LV_INDEV_STATE_PRESSED && data->key != 0) {
            ESP_LOGI(TAG, "🔑 EVENT-DRIVEN read: key=%"PRIu32", state=PRESSED", data->key);
        }
        
        ESP_LOGD(TAG, "Message buffer: Read complete message key=%d, state=%d", 
                 (int)event.key, (int)event.state);
    } else {
        // No complete messages in buffer, return released state with last key
        data->state = LV_INDEV_STATE_RELEASED;
        data->key = (g_last_key == LVGL_KEY_OK) ? LV_KEY_ENTER : 
                    (g_last_key == LVGL_KEY_NEXT) ? LV_KEY_RIGHT : 0;
        
        ESP_LOGD(TAG, "Message buffer empty, using fallback state");
    }
}

// Public API implementation

esp_err_t lvgl_button_input_init(void)
{
    ESP_LOGI(TAG, "Initializing LVGL button input device with Message Buffer...");
    
    if (g_input_initialized) {
        ESP_LOGW(TAG, "LVGL button input already initialized");
        return ESP_OK;
    }
    
    // Create message buffer for ISR-safe button events (prevents message fragmentation)
    g_button_message_buffer = xMessageBufferCreate(BUTTON_MESSAGE_BUFFER_SIZE);
    if (g_button_message_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to create button message buffer");
        return ESP_ERR_NO_MEM;
    }
    
    // Initialize GPIO button system if not already done
    esp_err_t ret = button_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize button system: %s", esp_err_to_name(ret));
        vMessageBufferDelete(g_button_message_buffer);
        g_button_message_buffer = NULL;
        return ret;
    }
    
    // Set up button callback for LVGL key events
    ret = button_set_interrupt_callback(button_to_lvgl_callback_isr);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set button callback: %s", esp_err_to_name(ret));
        button_deinit();
        vMessageBufferDelete(g_button_message_buffer);
        g_button_message_buffer = NULL;
        return ret;
    }
    
    // Enable button interrupt mode
    ret = button_set_interrupt_mode(true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable button interrupts: %s", esp_err_to_name(ret));
        button_set_interrupt_callback(NULL);
        button_deinit();
        vMessageBufferDelete(g_button_message_buffer);
        g_button_message_buffer = NULL;
        return ret;
    }
    
    // Create and register LVGL input device (LVGL 9.x API)
    g_indev = lv_indev_create();
    if (g_indev == NULL) {
        ESP_LOGE(TAG, "Failed to create LVGL input device");
        button_set_interrupt_mode(false);
        button_set_interrupt_callback(NULL);
        button_deinit();
        vMessageBufferDelete(g_button_message_buffer);
        g_button_message_buffer = NULL;
        return ESP_FAIL;
    }
    
    lv_indev_set_type(g_indev, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(g_indev, lvgl_keypad_read_cb);
    
    // 🔥 设置为事件驱动模式（基于官方SDL示例）
    lv_indev_set_mode(g_indev, LV_INDEV_MODE_EVENT);
    ESP_LOGI(TAG, "Input device set to EVENT-DRIVEN mode");
    
    // Initialize state
    g_last_key = LVGL_KEY_NONE;
    g_input_enabled = true;
    g_button_a_count = 0;
    g_button_b_count = 0;
    
    g_input_initialized = true;
    
    ESP_LOGI(TAG, "LVGL button input device initialized successfully");
    ESP_LOGI(TAG, "Mode: EVENT-DRIVEN (immediate response on button press)");
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
    
    // Clear message buffer when disabling to remove pending events
    if (!enabled && g_button_message_buffer) {
        xMessageBufferReset(g_button_message_buffer);
        ESP_LOGD(TAG, "Message buffer cleared on disable");
    }
}

bool lvgl_button_input_is_enabled(void)
{
    return g_input_enabled;
}

bool lvgl_button_input_is_event_driven(void)
{
    if (g_indev) {
        return lv_indev_get_mode(g_indev) == LV_INDEV_MODE_EVENT;
    }
    return false;
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
    
    // Clean up message buffer
    if (g_button_message_buffer) {
        vMessageBufferDelete(g_button_message_buffer);
        g_button_message_buffer = NULL;
    }
    
    g_input_initialized = false;
    
    ESP_LOGI(TAG, "LVGL button input device deinitialized");
    return ESP_OK;
}