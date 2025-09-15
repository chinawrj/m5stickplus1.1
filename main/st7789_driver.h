#ifndef ST7789_DRIVER_H
#define ST7789_DRIVER_H

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_err.h"

// M5StickC Plus 1.1 TFT Display Configuration
#define ST7789_SPI_HOST         SPI2_HOST
#define ST7789_LCD_PIXEL_CLOCK  20000000  // 20MHz

// GPIO Pin Mapping (from M5Stack official specs)
#define ST7789_PIN_MOSI         GPIO_NUM_15
#define ST7789_PIN_SCLK         GPIO_NUM_13
#define ST7789_PIN_CS           GPIO_NUM_5
#define ST7789_PIN_DC           GPIO_NUM_23
#define ST7789_PIN_RST          GPIO_NUM_18

// Display specifications
#define ST7789_LCD_H_RES        135
#define ST7789_LCD_V_RES        240
#define ST7789_OFFSET_X         52
#define ST7789_OFFSET_Y         40

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
 * @brief Initialize ST7789 TFT display
 * 
 * This function initializes the ST7789 display controller using ESP-IDF's
 * built-in LCD driver. It configures SPI bus, panel IO, and display panel.
 * 
 * @param panel_handle Output parameter for LCD panel handle
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t st7789_init(esp_lcd_panel_handle_t *panel_handle);

/**
 * @brief Fill entire screen with solid color
 * 
 * @param panel_handle LCD panel handle from st7789_init()
 * @param color RGB565 color value
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t st7789_fill_screen(esp_lcd_panel_handle_t panel_handle, uint16_t color);

/**
 * @brief Draw a filled rectangle
 * 
 * @param panel_handle LCD panel handle
 * @param x X coordinate (0-134)
 * @param y Y coordinate (0-239)
 * @param width Rectangle width
 * @param height Rectangle height
 * @param color RGB565 color value
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t st7789_draw_rect(esp_lcd_panel_handle_t panel_handle, 
                          int x, int y, int width, int height, 
                          uint16_t color);

/**
 * @brief Display test patterns
 * 
 * Shows various test patterns to verify display functionality:
 * - Color bars (red, green, blue, white)
 * - Gradient patterns
 * - Geometric shapes
 * 
 * @param panel_handle LCD panel handle
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t st7789_test_patterns(esp_lcd_panel_handle_t panel_handle);

/**
 * @brief Set display backlight brightness
 * 
 * Note: This function controls the display power through AXP192.
 * The actual backlight is controlled by the power management chip.
 * 
 * @param panel_handle LCD panel handle
 * @param brightness Brightness level (0-255, 0=off, 255=max)
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t st7789_set_brightness(esp_lcd_panel_handle_t panel_handle, uint8_t brightness);

/**
 * @brief Convert RGB888 to RGB565
 * 
 * @param r Red component (0-255)
 * @param g Green component (0-255)  
 * @param b Blue component (0-255)
 * @return uint16_t RGB565 color value
 */
uint16_t st7789_rgb888_to_rgb565(uint8_t r, uint8_t g, uint8_t b);

#endif // ST7789_DRIVER_H