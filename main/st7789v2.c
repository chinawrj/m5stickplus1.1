/**
 * @file st7789v2.c
 * @brief ST7789v2 TFT Display Driver Implementation for M5StickC Plus
 */

#include "st7789v2.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <math.h>

static const char *TAG = "ST7789";

static spi_device_handle_t spi_device = NULL;
static bool st7789_initialized = false;

// Current display rotation and dimensions
static uint8_t current_rotation = 0;
static uint16_t display_width = ST7789_WIDTH;
static uint16_t display_height = ST7789_HEIGHT;

/**
 * @brief Send command to ST7789 via SPI
 */
static esp_err_t st7789_send_command(uint8_t cmd)
{
    esp_err_t ret;
    spi_transaction_t trans = {
        .length = 8,
        .tx_buffer = &cmd,
        .flags = SPI_TRANS_USE_TXDATA,
    };
    trans.tx_data[0] = cmd;
    
    // Set DC low for command
    gpio_set_level(ST7789_PIN_DC, 0);
    ret = spi_device_transmit(spi_device, &trans);
    
    return ret;
}

/**
 * @brief Send data to ST7789 via SPI
 */
static esp_err_t st7789_send_data(const uint8_t *data, size_t len)
{
    if (len == 0) return ESP_OK;
    
    esp_err_t ret;
    spi_transaction_t trans = {
        .length = len * 8,
        .tx_buffer = data,
    };
    
    // Set DC high for data
    gpio_set_level(ST7789_PIN_DC, 1);
    ret = spi_device_transmit(spi_device, &trans);
    
    return ret;
}

/**
 * @brief Send single byte of data to ST7789
 */
static esp_err_t st7789_send_data_byte(uint8_t data)
{
    return st7789_send_data(&data, 1);
}

/**
 * @brief Set address window for drawing
 */
static esp_err_t st7789_set_address_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    esp_err_t ret;
    
    // Column address set
    ret = st7789_send_command(ST7789_CASET);
    if (ret != ESP_OK) return ret;
    
    uint8_t col_data[4] = {
        (x0 >> 8) & 0xFF, x0 & 0xFF,
        (x1 >> 8) & 0xFF, x1 & 0xFF
    };
    ret = st7789_send_data(col_data, 4);
    if (ret != ESP_OK) return ret;
    
    // Row address set
    ret = st7789_send_command(ST7789_RASET);
    if (ret != ESP_OK) return ret;
    
    uint8_t row_data[4] = {
        (y0 >> 8) & 0xFF, y0 & 0xFF,
        (y1 >> 8) & 0xFF, y1 & 0xFF
    };
    ret = st7789_send_data(row_data, 4);
    if (ret != ESP_OK) return ret;
    
    // Memory write
    ret = st7789_send_command(ST7789_RAMWR);
    
    return ret;
}

esp_err_t st7789_init(void)
{
    esp_err_t ret;
    
    ESP_LOGI(TAG, "Initializing ST7789v2 display...");
    
    // Configure GPIO pins
    gpio_config_t gpio_conf = {
        .pin_bit_mask = (1ULL << ST7789_PIN_DC) | (1ULL << ST7789_PIN_RST),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ret = gpio_config(&gpio_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GPIO pins");
        return ret;
    }
    
    // Initialize SPI bus
    spi_bus_config_t bus_config = {
        .mosi_io_num = ST7789_PIN_MOSI,
        .miso_io_num = -1,  // Not used
        .sclk_io_num = ST7789_PIN_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = ST7789_WIDTH * ST7789_HEIGHT * 2 + 8,
    };
    
    ret = spi_bus_initialize(SPI2_HOST, &bus_config, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Add SPI device
    spi_device_interface_config_t dev_config = {
        .clock_speed_hz = 26 * 1000 * 1000,  // 26 MHz
        .mode = 0,                            // SPI mode 0
        .spics_io_num = ST7789_PIN_CS,
        .queue_size = 7,
    };
    
    ret = spi_bus_add_device(SPI2_HOST, &dev_config, &spi_device);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add SPI device: %s", esp_err_to_name(ret));
        spi_bus_free(SPI2_HOST);
        return ret;
    }
    
    // Hardware reset
    gpio_set_level(ST7789_PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(ST7789_PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Software reset
    ret = st7789_send_command(ST7789_SWRESET);
    if (ret != ESP_OK) return ret;
    vTaskDelay(pdMS_TO_TICKS(150));
    
    // Sleep out
    ret = st7789_send_command(ST7789_SLPOUT);
    if (ret != ESP_OK) return ret;
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Color mode: 16-bit RGB565
    ret = st7789_send_command(ST7789_COLMOD);
    if (ret != ESP_OK) return ret;
    ret = st7789_send_data_byte(0x55);  // 16-bit color
    if (ret != ESP_OK) return ret;
    
    // Memory access control (rotation)
    ret = st7789_send_command(ST7789_MADCTL);
    if (ret != ESP_OK) return ret;
    ret = st7789_send_data_byte(ST7789_MADCTL_RGB);
    if (ret != ESP_OK) return ret;
    
    // Display inversion off
    ret = st7789_send_command(ST7789_INVOFF);
    if (ret != ESP_OK) return ret;
    
    // Normal display mode
    ret = st7789_send_command(ST7789_NORON);
    if (ret != ESP_OK) return ret;
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Display on
    ret = st7789_send_command(ST7789_DISPON);
    if (ret != ESP_OK) return ret;
    vTaskDelay(pdMS_TO_TICKS(10));
    
    st7789_initialized = true;
    ESP_LOGI(TAG, "ST7789v2 display initialized successfully (135x240)");
    
    return ESP_OK;
}

esp_err_t st7789_deinit(void)
{
    if (!st7789_initialized) return ESP_OK;
    
    st7789_display_off();
    
    if (spi_device) {
        spi_bus_remove_device(spi_device);
        spi_device = NULL;
    }
    spi_bus_free(SPI2_HOST);
    
    st7789_initialized = false;
    ESP_LOGI(TAG, "ST7789v2 display deinitialized");
    
    return ESP_OK;
}

esp_err_t st7789_display_on(void)
{
    if (!st7789_initialized) return ESP_ERR_INVALID_STATE;
    return st7789_send_command(ST7789_DISPON);
}

esp_err_t st7789_display_off(void)
{
    if (!st7789_initialized) return ESP_ERR_INVALID_STATE;
    return st7789_send_command(ST7789_DISPOFF);
}

esp_err_t st7789_set_invert(bool invert)
{
    if (!st7789_initialized) return ESP_ERR_INVALID_STATE;
    return st7789_send_command(invert ? ST7789_INVON : ST7789_INVOFF);
}

esp_err_t st7789_set_rotation(uint8_t rotation)
{
    if (!st7789_initialized) return ESP_ERR_INVALID_STATE;
    
    esp_err_t ret;
    uint8_t madctl = ST7789_MADCTL_RGB;
    
    rotation = rotation % 4;
    current_rotation = rotation;
    
    switch (rotation) {
        case 0:  // 0°
            display_width = ST7789_WIDTH;
            display_height = ST7789_HEIGHT;
            break;
        case 1:  // 90°
            madctl |= ST7789_MADCTL_MV | ST7789_MADCTL_MY;
            display_width = ST7789_HEIGHT;
            display_height = ST7789_WIDTH;
            break;
        case 2:  // 180°
            madctl |= ST7789_MADCTL_MX | ST7789_MADCTL_MY;
            display_width = ST7789_WIDTH;
            display_height = ST7789_HEIGHT;
            break;
        case 3:  // 270°
            madctl |= ST7789_MADCTL_MV | ST7789_MADCTL_MX;
            display_width = ST7789_HEIGHT;
            display_height = ST7789_WIDTH;
            break;
    }
    
    ret = st7789_send_command(ST7789_MADCTL);
    if (ret != ESP_OK) return ret;
    ret = st7789_send_data_byte(madctl);
    
    ESP_LOGI(TAG, "Display rotation set to %d° (%dx%d)", rotation * 90, display_width, display_height);
    
    return ret;
}

esp_err_t st7789_fill_screen(uint16_t color)
{
    if (!st7789_initialized) return ESP_ERR_INVALID_STATE;
    
    esp_err_t ret = st7789_set_address_window(0, 0, display_width - 1, display_height - 1);
    if (ret != ESP_OK) return ret;
    
    uint32_t pixel_count = display_width * display_height;
    uint16_t color_be = __builtin_bswap16(color);  // Convert to big-endian
    
    // Send color data in chunks
    const size_t chunk_size = 2048;
    uint8_t *buffer = malloc(chunk_size);
    if (!buffer) {
        ESP_LOGE(TAG, "Failed to allocate buffer for fill_screen");
        return ESP_ERR_NO_MEM;
    }
    
    // Fill buffer with color pattern
    for (size_t i = 0; i < chunk_size; i += 2) {
        buffer[i] = color_be >> 8;
        buffer[i + 1] = color_be & 0xFF;
    }
    
    size_t bytes_total = pixel_count * 2;
    size_t bytes_sent = 0;
    
    while (bytes_sent < bytes_total) {
        size_t bytes_to_send = (bytes_total - bytes_sent > chunk_size) ? chunk_size : (bytes_total - bytes_sent);
        ret = st7789_send_data(buffer, bytes_to_send);
        if (ret != ESP_OK) {
            free(buffer);
            return ret;
        }
        bytes_sent += bytes_to_send;
    }
    
    free(buffer);
    return ESP_OK;
}

esp_err_t st7789_draw_pixel(uint16_t x, uint16_t y, uint16_t color)
{
    if (!st7789_initialized) return ESP_ERR_INVALID_STATE;
    if (x >= display_width || y >= display_height) return ESP_ERR_INVALID_ARG;
    
    esp_err_t ret = st7789_set_address_window(x, y, x, y);
    if (ret != ESP_OK) return ret;
    
    uint16_t color_be = __builtin_bswap16(color);
    uint8_t color_data[2] = { color_be >> 8, color_be & 0xFF };
    
    return st7789_send_data(color_data, 2);
}

esp_err_t st7789_fill_rect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color)
{
    if (!st7789_initialized) return ESP_ERR_INVALID_STATE;
    if (x >= display_width || y >= display_height) return ESP_ERR_INVALID_ARG;
    
    // Clip rectangle to display bounds
    if (x + width > display_width) width = display_width - x;
    if (y + height > display_height) height = display_height - y;
    
    esp_err_t ret = st7789_set_address_window(x, y, x + width - 1, y + height - 1);
    if (ret != ESP_OK) return ret;
    
    uint32_t pixel_count = width * height;
    uint16_t color_be = __builtin_bswap16(color);
    uint8_t color_data[2] = { color_be >> 8, color_be & 0xFF };
    
    for (uint32_t i = 0; i < pixel_count; i++) {
        ret = st7789_send_data(color_data, 2);
        if (ret != ESP_OK) return ret;
    }
    
    return ESP_OK;
}

esp_err_t st7789_draw_line(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color)
{
    if (!st7789_initialized) return ESP_ERR_INVALID_STATE;
    
    int16_t dx = abs(x1 - x0);
    int16_t dy = abs(y1 - y0);
    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t err = dx - dy;
    
    while (true) {
        st7789_draw_pixel(x0, y0, color);
        
        if (x0 == x1 && y0 == y1) break;
        
        int16_t e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
    
    return ESP_OK;
}

uint16_t st7789_rgb888_to_rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

esp_err_t st7789_test_rgb_pattern(void)
{
    if (!st7789_initialized) return ESP_ERR_INVALID_STATE;
    
    ESP_LOGI(TAG, "Displaying RGB test pattern...");
    
    uint16_t height_third = display_height / 3;
    
    // Red section
    esp_err_t ret = st7789_fill_rect(0, 0, display_width, height_third, ST7789_RED);
    if (ret != ESP_OK) return ret;
    
    // Green section
    ret = st7789_fill_rect(0, height_third, display_width, height_third, ST7789_GREEN);
    if (ret != ESP_OK) return ret;
    
    // Blue section
    ret = st7789_fill_rect(0, height_third * 2, display_width, display_height - height_third * 2, ST7789_BLUE);
    
    return ret;
}

esp_err_t st7789_test_gradient(void)
{
    if (!st7789_initialized) return ESP_ERR_INVALID_STATE;
    
    ESP_LOGI(TAG, "Displaying gradient test pattern...");
    
    for (uint16_t y = 0; y < display_height; y++) {
        uint8_t intensity = (y * 255) / display_height;
        uint16_t color = st7789_rgb888_to_rgb565(intensity, intensity, intensity);
        
        esp_err_t ret = st7789_fill_rect(0, y, display_width, 1, color);
        if (ret != ESP_OK) return ret;
    }
    
    return ESP_OK;
}

esp_err_t st7789_test_color_bars(void)
{
    if (!st7789_initialized) return ESP_ERR_INVALID_STATE;
    
    ESP_LOGI(TAG, "Displaying color bars test pattern...");
    
    uint16_t colors[] = {
        ST7789_RED, ST7789_GREEN, ST7789_BLUE, ST7789_YELLOW,
        ST7789_MAGENTA, ST7789_CYAN, ST7789_WHITE, ST7789_BLACK
    };
    
    uint16_t bar_width = display_width / 8;
    
    for (int i = 0; i < 8; i++) {
        uint16_t x = i * bar_width;
        uint16_t width = (i == 7) ? (display_width - x) : bar_width;
        
        esp_err_t ret = st7789_fill_rect(x, 0, width, display_height, colors[i]);
        if (ret != ESP_OK) return ret;
    }
    
    return ESP_OK;
}

esp_err_t st7789_test_checkerboard(void)
{
    if (!st7789_initialized) return ESP_ERR_INVALID_STATE;
    
    ESP_LOGI(TAG, "Displaying checkerboard test pattern...");
    
    const uint16_t square_size = 16;
    
    for (uint16_t y = 0; y < display_height; y += square_size) {
        for (uint16_t x = 0; x < display_width; x += square_size) {
            uint16_t color = ((x / square_size + y / square_size) % 2) ? ST7789_WHITE : ST7789_BLACK;
            uint16_t width = (x + square_size > display_width) ? (display_width - x) : square_size;
            uint16_t height = (y + square_size > display_height) ? (display_height - y) : square_size;
            
            esp_err_t ret = st7789_fill_rect(x, y, width, height, color);
            if (ret != ESP_OK) return ret;
        }
    }
    
    return ESP_OK;
}