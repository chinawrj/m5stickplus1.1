#include "st7789_driver.h"
#include "axp192.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "ST7789";

/**
 * @brief Initialize ST7789 TFT display
 */
esp_err_t st7789_init(esp_lcd_panel_handle_t *panel_handle)
{
    ESP_LOGI(TAG, "Initializing ST7789 TFT display");
    
    if (panel_handle == NULL) {
        ESP_LOGE(TAG, "Panel handle pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = ESP_OK;
    
    // Power on TFT display through AXP192
    ESP_LOGI(TAG, "Powering on TFT display");
    ret = axp192_power_tft_display(true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to power on TFT display: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Give power some time to stabilize
    vTaskDelay(pdMS_TO_TICKS(100));

    // Step 1: Initialize SPI bus
    ESP_LOGI(TAG, "Initializing SPI bus");
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = ST7789_PIN_MOSI,
        .miso_io_num = GPIO_NUM_NC,  // Not used for display
        .sclk_io_num = ST7789_PIN_SCLK,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = ST7789_LCD_H_RES * ST7789_LCD_V_RES * sizeof(uint16_t),
    };
    
    ret = spi_bus_initialize(ST7789_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return ret;
    }

    // Step 2: Create LCD panel IO handle
    ESP_LOGI(TAG, "Creating LCD panel IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;
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
        ESP_LOGE(TAG, "Failed to create panel IO: %s", esp_err_to_name(ret));
        spi_bus_free(ST7789_SPI_HOST);
        return ret;
    }

    // Step 3: Create LCD panel handle
    ESP_LOGI(TAG, "Creating LCD panel");
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = ST7789_PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,  // RGB565
    };
    
    ret = esp_lcd_new_panel_st7789(io_handle, &panel_config, panel_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create panel: %s", esp_err_to_name(ret));
        esp_lcd_panel_io_del(io_handle);
        spi_bus_free(ST7789_SPI_HOST);
        return ret;
    }

    // Step 4: Initialize the panel
    ESP_LOGI(TAG, "Initializing panel");
    ret = esp_lcd_panel_reset(*panel_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to reset panel: %s", esp_err_to_name(ret));
        goto error_cleanup;
    }
    
    ret = esp_lcd_panel_init(*panel_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init panel: %s", esp_err_to_name(ret));
        goto error_cleanup;
    }

    // Step 5: Configure display settings (based on M5Stack configuration)
    ESP_LOGI(TAG, "Configuring display settings");
    
    // Set gap for M5StickC Plus specific panel
    ret = esp_lcd_panel_set_gap(*panel_handle, ST7789_OFFSET_X, ST7789_OFFSET_Y);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set gap: %s", esp_err_to_name(ret));
        goto error_cleanup;
    }
    
    // Invert colors (required for this specific panel)
    ret = esp_lcd_panel_invert_color(*panel_handle, true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to invert colors: %s", esp_err_to_name(ret));
        goto error_cleanup;
    }
    
    // Turn on display
    ret = esp_lcd_panel_disp_on_off(*panel_handle, true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to turn on display: %s", esp_err_to_name(ret));
        goto error_cleanup;
    }

    ESP_LOGI(TAG, "ST7789 display initialized successfully");
    ESP_LOGI(TAG, "Resolution: %dx%d, Offset: (%d,%d)", 
             ST7789_LCD_H_RES, ST7789_LCD_V_RES, ST7789_OFFSET_X, ST7789_OFFSET_Y);
    
    return ESP_OK;

error_cleanup:
    esp_lcd_panel_del(*panel_handle);
    esp_lcd_panel_io_del(io_handle);
    spi_bus_free(ST7789_SPI_HOST);
    *panel_handle = NULL;
    return ret;
}

/**
 * @brief Fill entire screen with solid color
 */
esp_err_t st7789_fill_screen(esp_lcd_panel_handle_t panel_handle, uint16_t color)
{
    if (panel_handle == NULL) {
        ESP_LOGE(TAG, "Panel handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    // Create color buffer for one line
    const size_t line_size = ST7789_LCD_H_RES * sizeof(uint16_t);
    uint16_t *line_buffer = malloc(line_size);
    if (line_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate line buffer");
        return ESP_ERR_NO_MEM;
    }

    // Fill line buffer with color
    for (int i = 0; i < ST7789_LCD_H_RES; i++) {
        line_buffer[i] = color;
    }

    esp_err_t ret = ESP_OK;
    // Draw line by line to avoid large memory allocation
    for (int y = 0; y < ST7789_LCD_V_RES; y++) {
        ret = esp_lcd_panel_draw_bitmap(panel_handle, 0, y, ST7789_LCD_H_RES, y + 1, line_buffer);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to draw line %d: %s", y, esp_err_to_name(ret));
            break;
        }
    }

    free(line_buffer);
    return ret;
}

/**
 * @brief Draw a filled rectangle
 */
esp_err_t st7789_draw_rect(esp_lcd_panel_handle_t panel_handle, 
                          int x, int y, int width, int height, 
                          uint16_t color)
{
    if (panel_handle == NULL) {
        ESP_LOGE(TAG, "Panel handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    // Validate coordinates
    if (x < 0 || y < 0 || x + width > ST7789_LCD_H_RES || y + height > ST7789_LCD_V_RES) {
        ESP_LOGE(TAG, "Rectangle coordinates out of bounds: (%d,%d) %dx%d", x, y, width, height);
        return ESP_ERR_INVALID_ARG;
    }

    // Create buffer for rectangle
    const size_t buffer_size = width * height * sizeof(uint16_t);
    uint16_t *rect_buffer = malloc(buffer_size);
    if (rect_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate rectangle buffer");
        return ESP_ERR_NO_MEM;
    }

    // Fill buffer with color
    for (int i = 0; i < width * height; i++) {
        rect_buffer[i] = color;
    }

    // Draw rectangle
    esp_err_t ret = esp_lcd_panel_draw_bitmap(panel_handle, x, y, x + width, y + height, rect_buffer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to draw rectangle: %s", esp_err_to_name(ret));
    }

    free(rect_buffer);
    return ret;
}

/**
 * @brief Display test patterns
 */
esp_err_t st7789_test_patterns(esp_lcd_panel_handle_t panel_handle)
{
    if (panel_handle == NULL) {
        ESP_LOGE(TAG, "Panel handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Starting test patterns");
    esp_err_t ret = ESP_OK;

    // Test 1: Solid colors
    ESP_LOGI(TAG, "Test 1: Solid colors");
    uint16_t colors[] = {ST7789_COLOR_RED, ST7789_COLOR_GREEN, ST7789_COLOR_BLUE, ST7789_COLOR_WHITE};
    const char* color_names[] = {"RED", "GREEN", "BLUE", "WHITE"};
    
    for (int i = 0; i < 4; i++) {
        ESP_LOGI(TAG, "Displaying %s", color_names[i]);
        ret = st7789_fill_screen(panel_handle, colors[i]);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to fill screen with %s", color_names[i]);
            return ret;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    // Test 2: Color bars
    ESP_LOGI(TAG, "Test 2: Color bars");
    const int bar_height = ST7789_LCD_V_RES / 4;
    uint16_t bar_colors[] = {ST7789_COLOR_RED, ST7789_COLOR_GREEN, ST7789_COLOR_BLUE, ST7789_COLOR_WHITE};
    
    for (int i = 0; i < 4; i++) {
        ret = st7789_draw_rect(panel_handle, 0, i * bar_height, ST7789_LCD_H_RES, bar_height, bar_colors[i]);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to draw color bar %d", i);
            return ret;
        }
    }
    vTaskDelay(pdMS_TO_TICKS(2000));

    // Test 3: Gradient pattern
    ESP_LOGI(TAG, "Test 3: Gradient pattern");
    for (int x = 0; x < ST7789_LCD_H_RES; x++) {
        uint8_t intensity = (x * 255) / ST7789_LCD_H_RES;
        uint16_t gradient_color = st7789_rgb888_to_rgb565(intensity, intensity, intensity);
        ret = st7789_draw_rect(panel_handle, x, 0, 1, ST7789_LCD_V_RES, gradient_color);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to draw gradient line %d", x);
            return ret;
        }
    }
    vTaskDelay(pdMS_TO_TICKS(2000));

    // Test 4: Geometric shapes
    ESP_LOGI(TAG, "Test 4: Geometric shapes");
    ret = st7789_fill_screen(panel_handle, ST7789_COLOR_BLACK);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Draw rectangles in different colors
    ret = st7789_draw_rect(panel_handle, 10, 10, 30, 30, ST7789_COLOR_RED);
    if (ret == ESP_OK) {
        ret = st7789_draw_rect(panel_handle, 50, 50, 30, 30, ST7789_COLOR_GREEN);
    }
    if (ret == ESP_OK) {
        ret = st7789_draw_rect(panel_handle, 90, 90, 30, 30, ST7789_COLOR_BLUE);
    }
    if (ret == ESP_OK) {
        ret = st7789_draw_rect(panel_handle, 30, 180, 75, 40, ST7789_COLOR_YELLOW);
    }
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to draw geometric shapes");
        return ret;
    }
    
    vTaskDelay(pdMS_TO_TICKS(3000));

    ESP_LOGI(TAG, "Test patterns completed successfully");
    return ESP_OK;
}

/**
 * @brief Set display brightness via AXP192
 */
esp_err_t st7789_set_brightness(esp_lcd_panel_handle_t panel_handle, uint8_t brightness)
{
    if (panel_handle == NULL) {
        ESP_LOGE(TAG, "Panel handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    // Brightness is controlled by AXP192 LDO2 voltage
    // For now, we just log the request - actual brightness control
    // would require extending AXP192 driver with LDO2 brightness control
    ESP_LOGI(TAG, "Brightness control requested: %d/255", brightness);
    ESP_LOGI(TAG, "Note: Brightness is controlled by AXP192 LDO2 voltage");
    
    // For demonstration, we can turn display on/off
    bool display_on = (brightness > 0);
    esp_err_t ret = esp_lcd_panel_disp_on_off(panel_handle, display_on);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to control display power: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Display %s", display_on ? "ON" : "OFF");
    return ESP_OK;
}

/**
 * @brief Convert RGB888 to RGB565
 */
uint16_t st7789_rgb888_to_rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    // RGB565: RRRRR GGGGGG BBBBB
    // RGB888: RRRRRRRR GGGGGGGG BBBBBBBB
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}