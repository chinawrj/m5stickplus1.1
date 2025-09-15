#include "st7789_lcd.h"
#include "axp192.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ESP-IDF LCD components
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_st7789.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"

static const char *TAG = "ST7789_LCD";

// Static handles for LCD panel and IO
static esp_lcd_panel_handle_t panel_handle = NULL;
static esp_lcd_panel_io_handle_t io_handle = NULL;

/**
 * @brief Initialize ST7789 TFT display using ESP-IDF LCD components
 */
esp_err_t st7789_lcd_init(void)
{
    ESP_LOGI(TAG, "Initializing ST7789 TFT display using ESP-IDF LCD components");
    
    esp_err_t ret = ESP_OK;
    
    // Power on TFT display through AXP192
    ESP_LOGI(TAG, "Powering on TFT display");
    ret = axp192_power_tft_display(true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to power on TFT display: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Power on TFT backlight
    ESP_LOGI(TAG, "Powering on TFT backlight");
    ret = axp192_power_tft_backlight(true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to power on TFT backlight: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Give power some time to stabilize
    vTaskDelay(pdMS_TO_TICKS(100));

    // Initialize SPI bus
    ESP_LOGI(TAG, "Initializing SPI bus");
    spi_bus_config_t buscfg = {
        .mosi_io_num = ST7789_PIN_MOSI,
        .miso_io_num = GPIO_NUM_NC,  // ST7789 is write-only
        .sclk_io_num = ST7789_PIN_SCLK,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = ST7789_LCD_H_RES * ST7789_LCD_V_RES * sizeof(uint16_t),
    };
    
    ret = spi_bus_initialize(ST7789_SPI_HOST, &buscfg, ST7789_SPI_DMA_CHAN);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return ret;
    }

    // Configure LCD panel IO (SPI interface)
    ESP_LOGI(TAG, "Configuring LCD panel IO");
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = ST7789_PIN_DC,
        .cs_gpio_num = ST7789_PIN_CS,
        .pclk_hz = ST7789_LCD_PIXEL_CLOCK,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    
    ret = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)ST7789_SPI_HOST, &io_config, &io_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create LCD panel IO: %s", esp_err_to_name(ret));
        spi_bus_free(ST7789_SPI_HOST);
        return ret;
    }

    // Configure LCD panel
    ESP_LOGI(TAG, "Configuring LCD panel");
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = ST7789_PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,  // ST7789 uses BGR order
        .bits_per_pixel = 16,  // RGB565
    };
    
    ret = esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create LCD panel: %s", esp_err_to_name(ret));
        esp_lcd_panel_io_del(io_handle);
        spi_bus_free(ST7789_SPI_HOST);
        return ret;
    }

    // Reset and initialize the panel
    ESP_LOGI(TAG, "Resetting and initializing LCD panel");
    ret = esp_lcd_panel_reset(panel_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to reset LCD panel: %s", esp_err_to_name(ret));
        goto cleanup;
    }
    
    ret = esp_lcd_panel_init(panel_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize LCD panel: %s", esp_err_to_name(ret));
        goto cleanup;
    }

    // Turn on the display
    ret = esp_lcd_panel_disp_on_off(panel_handle, true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to turn on LCD panel: %s", esp_err_to_name(ret));
        goto cleanup;
    }

    // Set display orientation and gap (for M5StickC Plus ST7789)
    // M5StickC Plus specific offsets: X=52, Y=40 for proper 135x240 display
    ret = esp_lcd_panel_set_gap(panel_handle, 52, 40);  // Correct offset for M5StickC Plus
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set LCD gap: %s", esp_err_to_name(ret));
        goto cleanup;
    }

    // Set correct rotation for M5StickC Plus (0 degrees - portrait mode)
    ret = esp_lcd_panel_mirror(panel_handle, true, false);  // Mirror X axis
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set LCD mirror: %s", esp_err_to_name(ret));
        goto cleanup;
    }
    
    ret = esp_lcd_panel_swap_xy(panel_handle, false);  // No XY swap for portrait
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set LCD swap XY: %s", esp_err_to_name(ret));
        goto cleanup;
    }

    ESP_LOGI(TAG, "ST7789 LCD initialization completed successfully");
    ESP_LOGI(TAG, "Display resolution: %dx%d", ST7789_LCD_H_RES, ST7789_LCD_V_RES);
    
    return ESP_OK;

cleanup:
    esp_lcd_panel_del(panel_handle);
    esp_lcd_panel_io_del(io_handle);
    spi_bus_free(ST7789_SPI_HOST);
    panel_handle = NULL;
    io_handle = NULL;
    return ret;
}

/**
 * @brief Display test patterns using ESP-IDF LCD panel operations
 */
esp_err_t st7789_lcd_test_patterns(void)
{
    if (panel_handle == NULL) {
        ESP_LOGE(TAG, "LCD panel not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Starting display test patterns using LCD panel operations");
    
    // Test 1: Clear display with different colors
    ESP_LOGI(TAG, "Test 1: Color fill test");
    uint16_t colors[] = {
        ST7789_COLOR_RED,
        ST7789_COLOR_GREEN, 
        ST7789_COLOR_BLUE,
        ST7789_COLOR_WHITE,
        ST7789_COLOR_BLACK
    };
    
    for (int i = 0; i < sizeof(colors)/sizeof(colors[0]); i++) {
        ESP_LOGI(TAG, "  Filling with color 0x%04X", colors[i]);
        esp_err_t ret = st7789_lcd_clear(colors[i]);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to clear display: %s", esp_err_to_name(ret));
            return ret;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    
    // Test 2: Draw colored rectangles
    ESP_LOGI(TAG, "Test 2: Rectangle drawing test");
    st7789_lcd_clear(ST7789_COLOR_BLACK);
    
    // Draw colorful rectangles
    st7789_lcd_draw_rect(10, 10, 30, 40, ST7789_COLOR_RED);
    st7789_lcd_draw_rect(50, 20, 30, 40, ST7789_COLOR_GREEN);
    st7789_lcd_draw_rect(90, 30, 30, 40, ST7789_COLOR_BLUE);
    
    st7789_lcd_draw_rect(10, 80, 40, 30, ST7789_COLOR_YELLOW);
    st7789_lcd_draw_rect(60, 90, 40, 30, ST7789_COLOR_MAGENTA);
    st7789_lcd_draw_rect(10, 130, 115, 30, ST7789_COLOR_CYAN);
    
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // Test 3: Gradient effect
    ESP_LOGI(TAG, "Test 3: Simple gradient effect");
    for (int y = 0; y < ST7789_LCD_V_RES; y += 4) {
        uint8_t intensity = (y * 255) / ST7789_LCD_V_RES;
        uint16_t color = st7789_lcd_rgb888_to_rgb565(intensity, 0, 255 - intensity);
        st7789_lcd_draw_rect(0, y, ST7789_LCD_H_RES, 4, color);
    }
    
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    ESP_LOGI(TAG, "All LCD test patterns completed successfully");
    return ESP_OK;
}

/**
 * @brief Set display brightness via AXP192 backlight control
 */
esp_err_t st7789_lcd_set_brightness(uint8_t brightness)
{
    ESP_LOGI(TAG, "Setting display brightness: %d/255", brightness);
    
    // For now, we implement simple on/off control via AXP192
    bool backlight_on = (brightness > 0);
    
    esp_err_t ret = axp192_power_tft_backlight(backlight_on);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to control backlight: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Backlight %s", backlight_on ? "ON" : "OFF");
    return ESP_OK;
}

/**
 * @brief Get the LCD panel handle for advanced operations
 */
esp_lcd_panel_handle_t st7789_lcd_get_panel_handle(void)
{
    return panel_handle;
}

/**
 * @brief Draw a filled rectangle on the display
 */
esp_err_t st7789_lcd_draw_rect(int x, int y, int width, int height, uint16_t color)
{
    if (panel_handle == NULL) {
        ESP_LOGE(TAG, "LCD panel not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Validate bounds
    if (x < 0 || y < 0 || x + width > ST7789_LCD_H_RES || y + height > ST7789_LCD_V_RES) {
        ESP_LOGE(TAG, "Rectangle bounds out of display area");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Create color buffer for the rectangle
    uint16_t *color_buffer = malloc(width * height * sizeof(uint16_t));
    if (color_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate color buffer");
        return ESP_ERR_NO_MEM;
    }
    
    // Fill buffer with color
    for (int i = 0; i < width * height; i++) {
        color_buffer[i] = color;
    }
    
    // Draw the rectangle
    esp_err_t ret = esp_lcd_panel_draw_bitmap(panel_handle, x, y, x + width, y + height, color_buffer);
    
    free(color_buffer);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to draw rectangle: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

/**
 * @brief Clear the entire display with specified color
 */
esp_err_t st7789_lcd_clear(uint16_t color)
{
    return st7789_lcd_draw_rect(0, 0, ST7789_LCD_H_RES, ST7789_LCD_V_RES, color);
}

/**
 * @brief Convert RGB888 to RGB565
 */
uint16_t st7789_lcd_rgb888_to_rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    // RGB565: RRRRR GGGGGG BBBBB
    // RGB888: RRRRRRRR GGGGGGGG BBBBBBBB
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

/**
 * @brief Clean up ST7789 LCD resources
 */
esp_err_t st7789_lcd_deinit(void)
{
    ESP_LOGI(TAG, "Cleaning up ST7789 LCD resources");
    
    esp_err_t ret = ESP_OK;
    
    // Turn off display
    if (panel_handle != NULL) {
        esp_lcd_panel_disp_on_off(panel_handle, false);
        esp_lcd_panel_del(panel_handle);
        panel_handle = NULL;
    }
    
    // Clean up panel IO
    if (io_handle != NULL) {
        esp_lcd_panel_io_del(io_handle);
        io_handle = NULL;
    }
    
    // Free SPI bus
    ret = spi_bus_free(ST7789_SPI_HOST);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to free SPI bus: %s", esp_err_to_name(ret));
    }
    
    // Turn off display power to save energy
    axp192_power_tft_display(false);
    axp192_power_tft_backlight(false);
    
    ESP_LOGI(TAG, "ST7789 LCD cleanup completed");
    return ESP_OK;
}