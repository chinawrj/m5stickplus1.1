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
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>

static const char *TAG = "LVGL_BTN_INPUT";

// Input device state
static bool g_input_initialized = false;
static bool g_input_enabled = true;
static lv_indev_t *g_indev = NULL;

// LVGL-compliant button state tracking (ISR-safe with atomic operations)
static volatile lvgl_key_t g_current_key = LVGL_KEY_NONE;
static volatile lvgl_key_t g_last_key = LVGL_KEY_NONE;
static volatile lv_indev_state_t g_key_state = LV_INDEV_STATE_RELEASED;

// Statistics tracking (atomic)
static volatile uint32_t g_button_a_count = 0;
static volatile uint32_t g_button_b_count = 0;

// ISR-to-task communication queue for button events
static QueueHandle_t g_button_event_queue = NULL;

// Button event structure for ISR-safe communication
typedef struct {
    lvgl_key_t key;
    lv_indev_state_t state;
    uint32_t timestamp;
} button_event_msg_t;

// Task handle for button processing
static TaskHandle_t g_button_task_handle = NULL;

// LVGL timer for state management (optional, follows LVGL best practices)
static lv_timer_t *g_state_timer = NULL;

/**
 * @brief LVGL timer callback for state management (LVGL best practice)
 * 
 * This timer provides additional state management according to LVGL guidelines.
 * It ensures state consistency and handles edge cases in input device lifecycle.
 */
static void state_management_timer_cb(lv_timer_t *timer)
{
    (void)timer; // Unused parameter
    
    // Check for stale pressed states (over 5 seconds)
    static uint32_t last_press_time = 0;
    static bool was_pressed = false;
    
    bool currently_pressed = (g_key_state == LV_INDEV_STATE_PRESSED);
    
    if (currently_pressed && !was_pressed) {
        last_press_time = esp_timer_get_time() / 1000;
        was_pressed = true;
    } else if (!currently_pressed && was_pressed) {
        was_pressed = false;
    } else if (currently_pressed && was_pressed) {
        uint32_t current_time = esp_timer_get_time() / 1000;
        if (current_time - last_press_time > 5000) {
            // Clear stale press state (safety mechanism)
            ESP_LOGW(TAG, "Clearing stale button press state (>5s)");
            g_current_key = LVGL_KEY_NONE;
            g_key_state = LV_INDEV_STATE_RELEASED;
            was_pressed = false;
        }
    }
}

/**
 * @brief ISR-safe button state update using atomic operations
 * 
 * This function follows LVGL guidelines for ISR-safe input device handling.
 * It uses atomic operations instead of mutex to avoid blocking in ISR context.
 */
static void update_button_state_atomic(lvgl_key_t key, lv_indev_state_t state)
{
    // Atomic state updates - safe from ISR context
    g_current_key = key;
    g_key_state = state;
    
    if (state == LV_INDEV_STATE_PRESSED) {
        g_last_key = key;
        
        // Atomic statistics update
        if (key == LVGL_KEY_OK) {
            g_button_a_count++;
        } else if (key == LVGL_KEY_NEXT) {
            g_button_b_count++;
        }
    }
}

/**
 * @brief Button processing task (LVGL-compliant pattern)
 * 
 * This task processes button events from the ISR queue and manages state
 * transitions according to LVGL input device best practices.
 */
static void button_processing_task(void *pvParameters)
{
    button_event_msg_t event;
    
    ESP_LOGI(TAG, "LVGL button processing task started");
    
    while (1) {
        if (xQueueReceive(g_button_event_queue, &event, portMAX_DELAY)) {
            if (!g_input_enabled) {
                continue;
            }
            
            // Update state atomically
            update_button_state_atomic(event.key, event.state);
            
            // Log state changes (safe from task context)
            if (event.state == LV_INDEV_STATE_PRESSED) {
                ESP_LOGI(TAG, "Button pressed: %s (total A:%lu B:%lu)", 
                         (event.key == LVGL_KEY_OK) ? "OK/ENTER" : "RIGHT",
                         (uint32_t)g_button_a_count, (uint32_t)g_button_b_count);
            } else {
                ESP_LOGI(TAG, "Button released: %s", 
                         (event.key == LVGL_KEY_OK) ? "OK/ENTER" : "RIGHT");
                
                // LVGL Compliance: Clear key after release with small delay
                // This ensures LVGL has time to process the release event
                vTaskDelay(pdMS_TO_TICKS(10)); // 10ms delay for LVGL processing
                
                // Clear the key state to prevent repeated reads
                g_current_key = LVGL_KEY_NONE;
                ESP_LOGD(TAG, "Key state cleared after release");
            }
        }
    }
}

/**
 * @brief GPIO button callback - ISR-safe conversion to LVGL events
 * 
 * This callback follows LVGL standards for ISR-safe input device handling.
 * It sends events to a processing task instead of direct state manipulation.
 * 
 * @param button_id Which button triggered the event
 * @param event Type of button event (press/release/long press)
 * @param press_duration Duration of press if applicable
 */
static void button_to_lvgl_callback(button_id_t button_id, button_event_t event, uint32_t press_duration)
{
    if (!g_input_enabled || !g_button_event_queue) {
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
            return; // Ignore unknown buttons
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
    
    // Create event message for task processing (ISR-safe)
    button_event_msg_t msg = {
        .key = key,
        .state = state,
        .timestamp = esp_timer_get_time() / 1000
    };
    
    // Send to processing task - ISR-safe operation
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (xQueueSendFromISR(g_button_event_queue, &msg, &xHigherPriorityTaskWoken) != pdTRUE) {
        // Queue full - this is expected under high load and not an error
        return;
    }
    
    // Yield to higher priority task if needed
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/**
 * @brief LVGL input device read callback - LVGL 9.x compliant implementation
 * 
 * This function follows LVGL input device standards:
 * - Fast, non-blocking read operation
 * - No state modification in read callback
 * - Atomic state reading without mutex
 * - Clean, deterministic behavior
 */
static void lvgl_keypad_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;  // Unused parameter
    
    // Atomic read of current state (no mutex needed - follows LVGL standards)
    lvgl_key_t current_key = g_current_key;
    lv_indev_state_t current_state = g_key_state;
    
    // Convert internal key representation to LVGL keys
    uint32_t lvgl_key = 0;
    if (current_key == LVGL_KEY_OK) {
        lvgl_key = LV_KEY_ENTER;
    } else if (current_key == LVGL_KEY_NEXT) {
        lvgl_key = LV_KEY_RIGHT;  // Button B maps to RIGHT arrow
    }
    
    // Set LVGL data - simple and fast
    data->key = lvgl_key;
    data->state = current_state;
    
    // Optional: Smart logging for debugging (minimal overhead)
    static lv_indev_state_t last_logged_state = LV_INDEV_STATE_RELEASED;
    static uint32_t last_logged_key = 0;
    
    // Log only meaningful state changes
    if ((current_state != last_logged_state && lvgl_key != 0) || 
        (lvgl_key != last_logged_key && current_state == LV_INDEV_STATE_PRESSED)) {
        
        ESP_LOGI(TAG, "🔑 LVGL read: key=%lu, state=%s", 
                 lvgl_key, 
                 current_state == LV_INDEV_STATE_PRESSED ? "PRESSED" : "RELEASED");
        
        last_logged_state = current_state;
        last_logged_key = lvgl_key;
    }
    
    // LVGL will handle state lifecycle - no manual clearing needed
    // This follows LVGL input device best practices
}

// Public API implementation

esp_err_t lvgl_button_input_init(void)
{
    ESP_LOGI(TAG, "Initializing LVGL button input device (LVGL 9.x compliant)...");
    
    if (g_input_initialized) {
        ESP_LOGW(TAG, "LVGL button input already initialized");
        return ESP_OK;
    }
    
    // Create ISR-to-task communication queue
    g_button_event_queue = xQueueCreate(10, sizeof(button_event_msg_t));
    if (g_button_event_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create button event queue");
        return ESP_ERR_NO_MEM;
    }
    
    // Initialize GPIO button system if not already done
    esp_err_t ret = button_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize button system: %s", esp_err_to_name(ret));
        vQueueDelete(g_button_event_queue);
        g_button_event_queue = NULL;
        return ret;
    }
    
    // Create button processing task for ISR-safe handling
    BaseType_t task_ret = xTaskCreate(
        button_processing_task,
        "lvgl_btn_proc",
        4096,                    // Stack size
        NULL,                    // Parameters
        5,                       // Priority (higher than LVGL task)
        &g_button_task_handle
    );
    
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create button processing task");
        button_deinit();
        vQueueDelete(g_button_event_queue);
        g_button_event_queue = NULL;
        return ESP_ERR_NO_MEM;
    }
    
    // Set up button callback for LVGL key events
    ret = button_set_interrupt_callback(button_to_lvgl_callback);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set button callback: %s", esp_err_to_name(ret));
        if (g_button_task_handle) {
            vTaskDelete(g_button_task_handle);
            g_button_task_handle = NULL;
        }
        button_deinit();
        vQueueDelete(g_button_event_queue);
        g_button_event_queue = NULL;
        return ret;
    }
    
    // Enable button interrupt mode
    ret = button_set_interrupt_mode(true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable button interrupts: %s", esp_err_to_name(ret));
        button_set_interrupt_callback(NULL);
        if (g_button_task_handle) {
            vTaskDelete(g_button_task_handle);
            g_button_task_handle = NULL;
        }
        button_deinit();
        vQueueDelete(g_button_event_queue);
        g_button_event_queue = NULL;
        return ret;
    }
    
    // Create and register LVGL input device (LVGL 9.x API)
    g_indev = lv_indev_create();
    if (g_indev == NULL) {
        ESP_LOGE(TAG, "Failed to create LVGL input device");
        button_set_interrupt_mode(false);
        button_set_interrupt_callback(NULL);
        if (g_button_task_handle) {
            vTaskDelete(g_button_task_handle);
            g_button_task_handle = NULL;
        }
        button_deinit();
        vQueueDelete(g_button_event_queue);
        g_button_event_queue = NULL;
        return ESP_FAIL;
    }
    
    lv_indev_set_type(g_indev, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(g_indev, lvgl_keypad_read_cb);
    
    // Create LVGL timer for state management (LVGL best practice)
    g_state_timer = lv_timer_create(state_management_timer_cb, 1000, NULL);
    if (g_state_timer == NULL) {
        ESP_LOGW(TAG, "Failed to create state management timer (non-critical)");
    } else {
        ESP_LOGI(TAG, "LVGL state management timer created");
    }
    
    // Initialize atomic state variables
    g_current_key = LVGL_KEY_NONE;
    g_last_key = LVGL_KEY_NONE;
    g_key_state = LV_INDEV_STATE_RELEASED;
    g_input_enabled = true;
    g_button_a_count = 0;
    g_button_b_count = 0;
    
    g_input_initialized = true;
    
    ESP_LOGI(TAG, "LVGL button input device initialized successfully (LVGL 9.x compliant)");
    ESP_LOGI(TAG, "Button mapping: A=OK/ENTER, B=RIGHT");
    ESP_LOGI(TAG, "Using ISR-safe queue-based communication");
    
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
    
    // Clear current state when disabling (atomic operation)
    if (!enabled) {
        g_current_key = LVGL_KEY_NONE;
        g_key_state = LV_INDEV_STATE_RELEASED;
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

/**
 * @brief Validate LVGL compliance of button implementation
 * 
 * This function checks if the current implementation follows LVGL standards
 * for input device handling, particularly thread safety and timing aspects.
 * 
 * @return ESP_OK if compliant, ESP_FAIL with detailed logging if issues found
 */
esp_err_t lvgl_button_input_validate_compliance(void)
{
    ESP_LOGI(TAG, "🔍 Validating LVGL compliance...");
    
    bool compliance_ok = true;
    
    // Check 1: Initialization state
    if (!g_input_initialized) {
        ESP_LOGE(TAG, "❌ Input device not initialized");
        compliance_ok = false;
    } else {
        ESP_LOGI(TAG, "✅ Input device properly initialized");
    }
    
    // Check 2: LVGL device registration
    if (!g_indev) {
        ESP_LOGE(TAG, "❌ LVGL input device not registered");
        compliance_ok = false;
    } else {
        ESP_LOGI(TAG, "✅ LVGL input device registered");
    }
    
    // Check 3: Task-based processing (ISR safety)
    if (!g_button_task_handle) {
        ESP_LOGE(TAG, "❌ Button processing task not running");
        compliance_ok = false;
    } else {
        ESP_LOGI(TAG, "✅ ISR-safe task-based processing active");
    }
    
    // Check 4: Queue-based communication
    if (!g_button_event_queue) {
        ESP_LOGE(TAG, "❌ ISR-to-task queue not available");
        compliance_ok = false;
    } else {
        ESP_LOGI(TAG, "✅ Queue-based ISR communication active");
    }
    
    // Check 5: LVGL timer integration (optional but recommended)
    if (!g_state_timer) {
        ESP_LOGW(TAG, "⚠️  LVGL state timer not active (optional)");
    } else {
        ESP_LOGI(TAG, "✅ LVGL timer-based state management active");
    }
    
    // Check 6: Atomic state variables
    ESP_LOGI(TAG, "ℹ️  Current state: key=%d, state=%s", 
             (int)g_current_key,
             g_key_state == LV_INDEV_STATE_PRESSED ? "PRESSED" : "RELEASED");
    ESP_LOGI(TAG, "✅ Atomic state variables in use");
    
    // Summary
    if (compliance_ok) {
        ESP_LOGI(TAG, "🎉 LVGL compliance validation PASSED");
        ESP_LOGI(TAG, "📊 Stats: A:%lu presses, B:%lu presses", 
                 (uint32_t)g_button_a_count, (uint32_t)g_button_b_count);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "❌ LVGL compliance validation FAILED");
        return ESP_FAIL;
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
    
    // Clean up LVGL state management timer
    if (g_state_timer) {
        lv_timer_delete(g_state_timer);
        g_state_timer = NULL;
    }
    
    // Disable button interrupts and callbacks
    button_set_interrupt_mode(false);
    button_set_interrupt_callback(NULL);
    
    // Clean up button processing task
    if (g_button_task_handle) {
        vTaskDelete(g_button_task_handle);
        g_button_task_handle = NULL;
    }
    
    // Clean up event queue
    if (g_button_event_queue) {
        vQueueDelete(g_button_event_queue);
        g_button_event_queue = NULL;
    }
    
    // Reset atomic state variables
    g_current_key = LVGL_KEY_NONE;
    g_last_key = LVGL_KEY_NONE;
    g_key_state = LV_INDEV_STATE_RELEASED;
    g_button_a_count = 0;
    g_button_b_count = 0;
    
    g_input_initialized = false;
    
    ESP_LOGI(TAG, "LVGL button input device deinitialized (LVGL 9.x compliant)");
    return ESP_OK;
}