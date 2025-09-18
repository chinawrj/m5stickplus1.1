/*
 * LVGL Integration for M5StickC Plus 1.1
 * Based on ESP-IDF official LVGL examples and working ST7789 LCD code
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/lock.h>
#include <assert.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "lvgl.h"

#include "axp192.h"
#include "st7789_lcd.h"

static const char *TAG = "LVGL_INIT";

// M5StickC Plus LCD configuration (matching our working ST7789 code)
#define LCD_HOST            SPI2_HOST
#define LCD_PIXEL_CLOCK_HZ  (10 * 1000 * 1000)  // 10MHz for maximum stability
#define PIN_NUM_SCLK        13
#define PIN_NUM_MOSI        15
#define PIN_NUM_MISO        -1  // Not used
#define PIN_NUM_LCD_DC      23
#define PIN_NUM_LCD_RST     18
#define PIN_NUM_LCD_CS      5

// LVGL configuration (based on official example)
#define LVGL_DRAW_BUF_LINES    20    // Number of display lines in each draw buffer
#define LVGL_TICK_PERIOD_MS    2
#define LVGL_TASK_MAX_DELAY_MS 500
#define LVGL_TASK_MIN_DELAY_MS 1
#define LVGL_TASK_STACK_SIZE   (4 * 1024)
#define LVGL_TASK_PRIORITY     2

// LVGL library is not thread-safe, use a mutex to protect it (simplified for now)
// static _lock_t lvgl_api_lock;

static bool notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    lv_display_t *disp = (lv_display_t *)user_ctx;
    lv_display_flush_ready(disp);
    return false;
}

static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    esp_lcd_panel_handle_t panel_handle = lv_display_get_user_data(disp);
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;
    
    // Copy a buffer's content to a specific area of the display
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, px_map);
}

static void increase_lvgl_tick(void *arg)
{
    /* Tell LVGL how many milliseconds has elapsed */
    lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

static void lvgl_port_task(void *arg)
{
    ESP_LOGI(TAG, "Starting LVGL task");
    
    while (1) {
        // _lock_acquire(&lvgl_api_lock);  // Simplified for now
        uint32_t task_delay_ms = lv_timer_handler();
        // _lock_release(&lvgl_api_lock);  // Simplified for now
        
        if (task_delay_ms > LVGL_TASK_MAX_DELAY_MS) {
            task_delay_ms = LVGL_TASK_MAX_DELAY_MS;
        } else if (task_delay_ms < LVGL_TASK_MIN_DELAY_MS) {
            task_delay_ms = LVGL_TASK_MIN_DELAY_MS;
        }
        
        // Ensure IDLE task gets a chance to run and reset watchdog
        // Force minimum delay to prevent task starvation
        if (task_delay_ms < 10) {
            task_delay_ms = 10;  // Minimum 10ms to ensure IDLE task runs
        }
        
        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
        
        // Additional safety: yield CPU every 100 iterations to prevent starvation
        static uint32_t yield_counter = 0;
        if (++yield_counter >= 100) {
            yield_counter = 0;
            taskYIELD();  // Explicitly yield to other tasks
        }
    }
}

esp_err_t lvgl_init_base(void)
{
    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();

    ESP_LOGI(TAG, "Initialize SPI bus");
    spi_bus_config_t buscfg = {
        .sclk_io_num = PIN_NUM_SCLK,
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = ST7789_LCD_H_RES * 80 * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = PIN_NUM_LCD_DC,
        .cs_gpio_num = PIN_NUM_LCD_CS,
        .pclk_hz = LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));

    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_NUM_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };
    ESP_LOGI(TAG, "Install ST7789 panel driver");
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));
    
    // Set panel gap and mirror settings for M5StickC Plus - LANDSCAPE MODE
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel_handle, 40, 52));  // Swap gap values for landscape
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, false, true));  // Mirror Y axis for landscape
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_handle, true));        // Enable XY swap for landscape
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    ESP_LOGI(TAG, "Turn on LCD backlight");
    axp192_power_tft_backlight(true);

    ESP_LOGI(TAG, "Initialize LVGL");
    // Create a lvgl display for LANDSCAPE mode (240x135)
    lv_display_t *display = lv_display_create(ST7789_LCD_V_RES, ST7789_LCD_H_RES);  // Swap W/H for landscape

    // Alloc draw buffers used by LVGL for LANDSCAPE mode
    // Buffer size based on landscape width (240 pixels)
    size_t draw_buffer_sz = ST7789_LCD_V_RES * LVGL_DRAW_BUF_LINES * sizeof(lv_color16_t);

    void *buf1 = spi_bus_dma_memory_alloc(LCD_HOST, draw_buffer_sz, 0);
    if (buf1 == NULL) {
        ESP_LOGE(TAG, "Failed to allocate LVGL draw buffer 1");
        return ESP_FAIL;
    }
    void *buf2 = spi_bus_dma_memory_alloc(LCD_HOST, draw_buffer_sz, 0);
    if (buf2 == NULL) {
        ESP_LOGE(TAG, "Failed to allocate LVGL draw buffer 2");
        free(buf1);
        return ESP_FAIL;
    }
    
    // Initialize LVGL draw buffers (following official example)
    lv_display_set_buffers(display, buf1, buf2, draw_buffer_sz, LV_DISPLAY_RENDER_MODE_PARTIAL);
    
    // Associate the panel handle to the display
    lv_display_set_user_data(display, panel_handle);
    
    // Set color depth (following official example)
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    
    // Set the callback which can copy the rendered image to an area of the display
    lv_display_set_flush_cb(display, lvgl_flush_cb);

    ESP_LOGI(TAG, "Install LVGL tick timer");
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &increase_lvgl_tick,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, LVGL_TICK_PERIOD_MS * 1000));

    ESP_LOGI(TAG, "Register io panel event callback for LVGL flush ready notification");
    const esp_lcd_panel_io_callbacks_t cbs = {
        .on_color_trans_done = notify_lvgl_flush_ready,
    };
    esp_lcd_panel_io_register_event_callbacks(io_handle, &cbs, display);

    ESP_LOGI(TAG, "Start LVGL task");
    xTaskCreate(lvgl_port_task, "LVGL", LVGL_TASK_STACK_SIZE, NULL, LVGL_TASK_PRIORITY, NULL);

    ESP_LOGI(TAG, "LVGL base initialization complete (no demo UI created)");
    return ESP_OK;
}