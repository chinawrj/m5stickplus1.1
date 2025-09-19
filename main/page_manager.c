/*
 * Page Manager Implementation for M5StickC Plus 1.1 Multi-Page LVGL Application
 * Using Modular Page Controller Pattern for Better Architecture
 * Each page managed by independent modules with standardized interface
 */

#include "page_manager.h"
#include "page_manager_monitor.h"
#include "page_manager_espnow.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "PAGE_MANAGER";

// LVGL Pattern: Single Screen + Modular Page Controllers  
static lv_obj_t *g_main_screen = NULL;
static page_id_t g_current_page = PAGE_MONITOR;
static bool g_navigation_enabled = true;
static lv_timer_t *g_update_timer = NULL;

// Page controller instances
static const page_controller_t *g_page_controllers[PAGE_COUNT] = {NULL};

// Forward declarations
static void load_page(page_id_t page_id);
static void page_update_timer_cb(lv_timer_t *timer);

// Timer callback for page updates - using modular page controller pattern
static void page_update_timer_cb(lv_timer_t *timer)
{
    // Get the controller for current page
    if (g_current_page >= PAGE_COUNT || g_page_controllers[g_current_page] == NULL) {
        ESP_LOGW(TAG, "Invalid page controller for page %d", g_current_page);
        return;
    }
    
    const page_controller_t *controller = g_page_controllers[g_current_page];
    
    // Check if this page's data has been updated since last check
    // Note: is_data_updated() atomically clears the flag to avoid race conditions
    if (controller->is_data_updated && !controller->is_data_updated()) {
        ESP_LOGD(TAG, "No data update for page %s, skipping UI refresh", controller->name);
        return;
    }
    
    ESP_LOGD(TAG, "Data updated for page %s, refreshing UI", controller->name);
    
    // Update the page using its controller
    if (controller->update) {
        esp_err_t ret = controller->update();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to update page %s: %s", controller->name, esp_err_to_name(ret));
            return;
        }
    }
}

// Modular Page Controller Pattern: Thread-safe page switching
static void load_page(page_id_t page_id)
{
    ESP_LOGI(TAG, "Direct page switch to %d (no timer needed - already in LVGL task)", page_id);
    
    // Validate page ID
    if (page_id >= PAGE_COUNT) {
        ESP_LOGE(TAG, "Invalid page ID: %d", page_id);
        return;
    }
    
    // Get page controller
    const page_controller_t *controller = g_page_controllers[page_id];
    if (!controller) {
        ESP_LOGE(TAG, "No controller registered for page %d", page_id);
        return;
    }
    
    // Get the active screen
    lv_obj_t *scr = lv_screen_active();
    
    // Clean all content from screen
    lv_obj_clean(scr);
    
    ESP_LOGI(TAG, "Creating %s page...", controller->name);
    
    // Create page using its controller
    esp_err_t ret = ESP_FAIL;
    if (controller->create) {
        ret = controller->create();
    }
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "%s page created successfully", controller->name);
        g_current_page = page_id;
        ESP_LOGI(TAG, "Page %d loaded successfully (direct switch)", page_id);
    } else {
        ESP_LOGE(TAG, "Failed to create %s page: %s", controller->name, esp_err_to_name(ret));
    }
}

// Public API functions

esp_err_t page_manager_init(lv_display_t *display)
{
    ESP_LOGI(TAG, "Initializing page manager with modular page controller pattern...");
    
    if (!display) {
        ESP_LOGE(TAG, "Invalid display parameter");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Initialize page modules
    ESP_LOGI(TAG, "Initializing page modules...");
    
    // Register page controllers first
    g_page_controllers[PAGE_MONITOR] = get_monitor_page_controller();
    g_page_controllers[PAGE_ESPNOW] = get_espnow_page_controller();
    
    // Initialize each page module through its controller
    for (int i = 0; i < PAGE_COUNT; i++) {
        if (g_page_controllers[i] && g_page_controllers[i]->init) {
            esp_err_t ret = g_page_controllers[i]->init();
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to initialize %s page module: %s", 
                         g_page_controllers[i]->name, esp_err_to_name(ret));
                return ret;
            }
        }
    }
    
    ESP_LOGI(TAG, "Page controllers registered successfully");
    
    g_main_screen = lv_screen_active();
    if (!g_main_screen) {
        ESP_LOGE(TAG, "Failed to get active screen");
        return ESP_FAIL;
    }
    
    // Load initial page using modular controller
    load_page(PAGE_MONITOR);
    
    // Create update timer with 500ms interval for responsive updates
    g_update_timer = lv_timer_create(page_update_timer_cb, 500, NULL);
    if (!g_update_timer) {
        ESP_LOGE(TAG, "Failed to create update timer");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Page manager initialized successfully");
    return ESP_OK;
}

esp_err_t page_manager_next(void)
{
    if (!g_navigation_enabled) {
        ESP_LOGW(TAG, "Navigation is disabled");
        return ESP_ERR_INVALID_STATE;
    }
    
    page_id_t next_page = (g_current_page + 1) % PAGE_COUNT;
    ESP_LOGI(TAG, "Navigating from page %d to page %d", g_current_page, next_page);
    load_page(next_page);
    return ESP_OK;
}

esp_err_t page_manager_prev(void)
{
    if (!g_navigation_enabled) {
        ESP_LOGW(TAG, "Navigation is disabled");
        return ESP_ERR_INVALID_STATE;
    }
    
    page_id_t prev_page = (g_current_page == 0) ? (PAGE_COUNT - 1) : (g_current_page - 1);
    ESP_LOGI(TAG, "Navigating from page %d to page %d", g_current_page, prev_page);
    load_page(prev_page);
    return ESP_OK;
}

esp_err_t page_manager_goto(page_id_t page_id)
{
    if (!g_navigation_enabled) {
        ESP_LOGW(TAG, "Navigation is disabled");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (page_id >= PAGE_COUNT) {
        ESP_LOGE(TAG, "Invalid page ID: %d", page_id);
        return ESP_ERR_INVALID_ARG;
    }
    
    if (page_id == g_current_page) {
        ESP_LOGD(TAG, "Already on page %d", page_id);
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Navigating from page %d to page %d", g_current_page, page_id);
    load_page(page_id);
    return ESP_OK;
}

page_id_t page_manager_get_current(void)
{
    return g_current_page;
}

const char* page_manager_get_name(page_id_t page_id)
{
    if (page_id >= PAGE_COUNT || g_page_controllers[page_id] == NULL) {
        return "Unknown";
    }
    
    return g_page_controllers[page_id]->name;
}

void page_manager_update_current(void)
{
    if (g_current_page >= PAGE_COUNT || g_page_controllers[g_current_page] == NULL) {
        return;
    }
    
    const page_controller_t *controller = g_page_controllers[g_current_page];
    if (controller->update) {
        controller->update();
    }
}

bool page_manager_handle_key_event(uint32_t key)
{
    // Check if navigation is disabled
    if (!g_navigation_enabled) {
        ESP_LOGD(TAG, "Navigation disabled, ignoring key event %lu", key);
        return false;
    }
    
    // Get current page controller
    if (g_current_page >= PAGE_COUNT || g_page_controllers[g_current_page] == NULL) {
        ESP_LOGW(TAG, "Invalid page controller for page %d", g_current_page);
        return false;
    }
    
    const page_controller_t *controller = g_page_controllers[g_current_page];
    
    // First, let the current page handle the key event
    if (controller->handle_key_event) {
        bool handled = controller->handle_key_event(key);
        if (handled) {
            ESP_LOGD(TAG, "Page %s handled key event %lu", controller->name, key);
            return true;  // Page handled the key, don't process globally
        }
    }
    
    ESP_LOGD(TAG, "Page %s did not handle key %lu, processing globally", controller->name, key);
    return false;  // Page didn't handle it, let global handler process it
}

bool page_manager_is_navigation_enabled(void)
{
    return g_navigation_enabled;
}

void page_manager_set_navigation_enabled(bool enabled)
{
    g_navigation_enabled = enabled;
    ESP_LOGI(TAG, "Navigation %s", enabled ? "enabled" : "disabled");
}

esp_err_t page_manager_deinit(void)
{
    ESP_LOGI(TAG, "Deinitializing page manager...");
    
    // Stop update timer
    if (g_update_timer) {
        lv_timer_delete(g_update_timer);
        g_update_timer = NULL;
    }
    
    // Clean up page controllers
    for (int i = 0; i < PAGE_COUNT; i++) {
        if (g_page_controllers[i] && g_page_controllers[i]->destroy) {
            g_page_controllers[i]->destroy();
        }
        g_page_controllers[i] = NULL;
    }
    
    g_main_screen = NULL;
    g_current_page = PAGE_MONITOR;
    g_navigation_enabled = true;
    
    ESP_LOGI(TAG, "Page manager deinitialized");
    return ESP_OK;
}