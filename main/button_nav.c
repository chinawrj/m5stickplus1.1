/*
 * Button Navigation Handler Implementation for M5StickC Plus 1.1
 */

#include "button_nav.h"
#include "page_manager.h"
#include "button.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static const char *TAG = "BUTTON_NAV";

// Navigation event types
typedef enum {
    NAV_EVENT_NEXT_PAGE,
    NAV_EVENT_PREV_PAGE
} nav_event_t;

// Navigation state
static bool g_nav_enabled = true;
static QueueHandle_t g_nav_queue = NULL;
static TaskHandle_t g_nav_task_handle = NULL;

// Navigation task to handle LVGL operations safely
static void navigation_task(void *pvParameters)
{
    nav_event_t event;
    
    ESP_LOGI(TAG, "Navigation task started");
    
    while (1) {
        // Wait for navigation events
        if (xQueueReceive(g_nav_queue, &event, portMAX_DELAY) == pdTRUE) {
            if (!g_nav_enabled) {
                continue;
            }
            
            esp_err_t ret = ESP_OK;
            
            switch (event) {
                case NAV_EVENT_NEXT_PAGE:
                    ESP_LOGI(TAG, "Processing next page request");
                    ret = page_manager_next();
                    if (ret == ESP_OK) {
                        ESP_LOGI(TAG, "Navigated to next page: %s", 
                                page_manager_get_name(page_manager_get_current()));
                    } else {
                        ESP_LOGW(TAG, "Failed to navigate to next page: %s", esp_err_to_name(ret));
                    }
                    break;
                    
                case NAV_EVENT_PREV_PAGE:
                    ESP_LOGI(TAG, "Processing previous page request");
                    ret = page_manager_prev();
                    if (ret == ESP_OK) {
                        ESP_LOGI(TAG, "Navigated to previous page: %s",
                                page_manager_get_name(page_manager_get_current()));
                    } else {
                        ESP_LOGW(TAG, "Failed to navigate to previous page: %s", esp_err_to_name(ret));
                    }
                    break;
                    
                default:
                    ESP_LOGW(TAG, "Unknown navigation event: %d", event);
                    break;
            }
        }
    }
}

// Button press handling for navigation (ISR-safe)
static void navigation_button_callback(button_id_t button_id, button_event_t event, uint32_t press_duration)
{
    if (!g_nav_enabled) {
        return;
    }
    
    // Only handle short press events for navigation
    if (event != BUTTON_EVENT_SHORT_PRESS) {
        return;
    }
    
    ESP_LOGI(TAG, "Navigation button pressed: %s", 
             button_id == BUTTON_A ? "A (Next)" : "B (Prev)");
    
    nav_event_t nav_event;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    switch (button_id) {
        case BUTTON_A:
            // Button A: Next page
            nav_event = NAV_EVENT_NEXT_PAGE;
            break;
            
        case BUTTON_B:
            // Button B: Previous page
            nav_event = NAV_EVENT_PREV_PAGE;
            break;
            
        default:
            ESP_LOGW(TAG, "Unknown button ID: %d", button_id);
            return;
    }
    
    // Send event to navigation task (ISR-safe)
    if (xQueueSendFromISR(g_nav_queue, &nav_event, &xHigherPriorityTaskWoken) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to send navigation event to queue");
    }
    
    // Wake up higher priority task if needed
    if (xHigherPriorityTaskWoken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

// Public functions
esp_err_t button_nav_init(void)
{
    ESP_LOGI(TAG, "Initializing button navigation");
    
    // Create navigation event queue
    g_nav_queue = xQueueCreate(5, sizeof(nav_event_t));
    if (g_nav_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create navigation queue");
        return ESP_ERR_NO_MEM;
    }
    
    // Create navigation task
    BaseType_t task_ret = xTaskCreate(navigation_task, "nav_task", 4096, NULL, 3, &g_nav_task_handle);
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create navigation task");
        vQueueDelete(g_nav_queue);
        g_nav_queue = NULL;
        return ESP_ERR_NO_MEM;
    }
    
    // Set button callback for navigation
    esp_err_t ret = button_set_interrupt_callback(navigation_button_callback);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set button callback: %s", esp_err_to_name(ret));
        button_nav_deinit();
        return ret;
    }
    
    // Enable button interrupt mode
    ret = button_set_interrupt_mode(true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable button interrupts: %s", esp_err_to_name(ret));
        button_nav_deinit();
        return ret;
    }
    
    g_nav_enabled = true;
    
    ESP_LOGI(TAG, "Button navigation initialized successfully");
    ESP_LOGI(TAG, "Button A: Next page, Button B: Previous page");
    
    return ESP_OK;
}

void button_nav_set_enabled(bool enabled)
{
    g_nav_enabled = enabled;
    ESP_LOGI(TAG, "Button navigation %s", enabled ? "enabled" : "disabled");
}

bool button_nav_is_enabled(void)
{
    return g_nav_enabled;
}

esp_err_t button_nav_deinit(void)
{
    ESP_LOGI(TAG, "Deinitializing button navigation");
    
    g_nav_enabled = false;
    
    // Disable button interrupts
    esp_err_t ret = button_set_interrupt_mode(false);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to disable button interrupts: %s", esp_err_to_name(ret));
    }
    
    // Clear button callback
    button_set_interrupt_callback(NULL);
    
    // Delete navigation task
    if (g_nav_task_handle != NULL) {
        vTaskDelete(g_nav_task_handle);
        g_nav_task_handle = NULL;
    }
    
    // Delete navigation queue
    if (g_nav_queue != NULL) {
        vQueueDelete(g_nav_queue);
        g_nav_queue = NULL;
    }
    
    ESP_LOGI(TAG, "Button navigation deinitialized");
    return ESP_OK;
}