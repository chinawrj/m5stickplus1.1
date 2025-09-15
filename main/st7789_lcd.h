#ifndef ST7789_LCD_H
#define ST7789_LCD_H

#include "esp_err.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_st7789.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"

// M5StickC Plus 1.1 TFT Display Configuration
// GPIO Pin Mapping (ESP32-PICO-D4 to ST7789v2)
#define ST7789_PIN_MOSI         GPIO_NUM_15  // TFT_MOSI
#define ST7789_PIN_SCLK         GPIO_NUM_13  // TFT_CLK
#define ST7789_PIN_CS           GPIO_NUM_5   // TFT_CS
#define ST7789_PIN_DC           GPIO_NUM_23  // TFT_DC
#define ST7789_PIN_RST          GPIO_NUM_18  // TFT_RST

// Display specifications for ST7789v2
#define ST7789_LCD_H_RES        135
#define ST7789_LCD_V_RES        240
#define ST7789_LCD_PIXEL_CLOCK  (10 * 1000 * 1000)  // 10MHz for stability

// SPI Host configuration
#define ST7789_SPI_HOST         SPI2_HOST
#define ST7789_SPI_DMA_CHAN     SPI_DMA_CH_AUTO

// Color definitions (RGB565)
#define ST7789_COLOR_BLACK      0x0000
#define ST7789_COLOR_WHITE      0xFFFF
#define ST7789_COLOR_RED        0xF800
#define ST7789_COLOR_GREEN      0x07E0
#define ST7789_COLOR_BLUE       0x001F
#define ST7789_COLOR_YELLOW     0xFFE0
#define ST7789_COLOR_MAGENTA    0xF81F
#define ST7789_COLOR_CYAN       0x07FF

/**
 * @brief Initialize ST7789 TFT display using ESP-IDF LCD components
 * 
 * This function properly initializes the ST7789 display using:
 * - ESP-IDF SPI driver for communication
 * - ESP-IDF LCD panel driver for ST7789
 * - Proper power management through AXP192
 * 
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t st7789_lcd_init(void);

/**
 * @brief Display test patterns using ESP-IDF LCD panel operations
 * 
 * Shows colorful test patterns using proper LCD panel operations.
 * This demonstrates the display functionality with actual graphics.
 * 
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t st7789_lcd_test_patterns(void);

/**
 * @brief Set display brightness via backlight control
 * 
 * @param brightness Brightness level (0-255, 0=off, 255=max)
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t st7789_lcd_set_brightness(uint8_t brightness);

/**
 * @brief Get the LCD panel handle for advanced operations
 * 
 * @return esp_lcd_panel_handle_t Panel handle or NULL if not initialized
 */
esp_lcd_panel_handle_t st7789_lcd_get_panel_handle(void);

/**
 * @brief Draw a filled rectangle on the display
 * 
 * @param x X coordinate (0 to ST7789_LCD_H_RES-1)
 * @param y Y coordinate (0 to ST7789_LCD_V_RES-1) 
 * @param width Width of rectangle
 * @param height Height of rectangle
 * @param color RGB565 color value
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t st7789_lcd_draw_rect(int x, int y, int width, int height, uint16_t color);

/**
 * @brief Clear the entire display with specified color
 * 
 * @param color RGB565 color value
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t st7789_lcd_clear(uint16_t color);

/**
 * @brief Convert RGB888 to RGB565
 * 
 * @param r Red component (0-255)
 * @param g Green component (0-255)  
 * @param b Blue component (0-255)
 * @return uint16_t RGB565 color value
 */
uint16_t st7789_lcd_rgb888_to_rgb565(uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief Clean up ST7789 LCD resources
 * 
 * Properly cleanup SPI bus, LCD panel, and power management.
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t st7789_lcd_deinit(void);

#endif // ST7789_LCD_H