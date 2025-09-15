/**
 * @file st7789v2.h
 * @brief ST7789v2 TFT Display Driver for M5StickC Plus
 * 
 * Hardware connections:
 * ESP32-PICO-D4  | TFT Screen
 * GPIO15         | TFT_MOSI (SDA)
 * GPIO13         | TFT_CLK  (SCL)
 * GPIO23         | TFT_DC   (Data/Command)
 * GPIO18         | TFT_RST  (Reset)
 * GPIO5          | TFT_CS   (Chip Select)
 * 
 * Resolution: 135 x 240 pixels
 */

#ifndef ST7789V2_H
#define ST7789V2_H

#include "esp_err.h"
#include <stdint.h>

// Hardware pin definitions for M5StickC Plus
#define ST7789_PIN_MOSI    15    // SPI MOSI
#define ST7789_PIN_CLK     13    // SPI Clock
#define ST7789_PIN_DC      23    // Data/Command selection
#define ST7789_PIN_RST     18    // Reset
#define ST7789_PIN_CS      5     // Chip Select

// Display dimensions
#define ST7789_WIDTH       135
#define ST7789_HEIGHT      240

// Color definitions (16-bit RGB565 format)
#define ST7789_BLACK       0x0000
#define ST7789_WHITE       0xFFFF
#define ST7789_RED         0xF800
#define ST7789_GREEN       0x07E0
#define ST7789_BLUE        0x001F
#define ST7789_YELLOW      0xFFE0
#define ST7789_MAGENTA     0xF81F
#define ST7789_CYAN        0x07FF
#define ST7789_ORANGE      0xFD20
#define ST7789_PURPLE      0x8010
#define ST7789_GRAY        0x8410
#define ST7789_DARK_GREEN  0x03E0
#define ST7789_DARK_BLUE   0x0010
#define ST7789_DARK_RED    0x8000

// ST7789 command definitions
#define ST7789_NOP         0x00
#define ST7789_SWRESET     0x01
#define ST7789_RDDID       0x04
#define ST7789_RDDST       0x09
#define ST7789_SLPIN       0x10
#define ST7789_SLPOUT      0x11
#define ST7789_PTLON       0x12
#define ST7789_NORON       0x13
#define ST7789_INVOFF      0x20
#define ST7789_INVON       0x21
#define ST7789_DISPOFF     0x28
#define ST7789_DISPON      0x29
#define ST7789_CASET       0x2A
#define ST7789_RASET       0x2B
#define ST7789_RAMWR       0x2C
#define ST7789_RAMRD       0x2E
#define ST7789_PTLAR       0x30
#define ST7789_VSCRDEF     0x33
#define ST7789_COLMOD      0x3A
#define ST7789_MADCTL      0x36
#define ST7789_VSCSAD      0x37

// MADCTL bit definitions
#define ST7789_MADCTL_MY   0x80
#define ST7789_MADCTL_MX   0x40
#define ST7789_MADCTL_MV   0x20
#define ST7789_MADCTL_ML   0x10
#define ST7789_MADCTL_RGB  0x00
#define ST7789_MADCTL_BGR  0x08
#define ST7789_MADCTL_MH   0x04

/**
 * @brief Initialize ST7789v2 display
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t st7789_init(void);

/**
 * @brief Deinitialize ST7789v2 display
 * @return ESP_OK on success
 */
esp_err_t st7789_deinit(void);

/**
 * @brief Turn display on
 * @return ESP_OK on success
 */
esp_err_t st7789_display_on(void);

/**
 * @brief Turn display off
 * @return ESP_OK on success
 */
esp_err_t st7789_display_off(void);

/**
 * @brief Set display inversion
 * @param invert true to invert colors, false for normal
 * @return ESP_OK on success
 */
esp_err_t st7789_set_invert(bool invert);

/**
 * @brief Set display rotation
 * @param rotation 0-3 for 0°, 90°, 180°, 270°
 * @return ESP_OK on success
 */
esp_err_t st7789_set_rotation(uint8_t rotation);

/**
 * @brief Fill entire screen with a color
 * @param color 16-bit RGB565 color
 * @return ESP_OK on success
 */
esp_err_t st7789_fill_screen(uint16_t color);

/**
 * @brief Draw a pixel at specified coordinates
 * @param x X coordinate (0 to width-1)
 * @param y Y coordinate (0 to height-1)
 * @param color 16-bit RGB565 color
 * @return ESP_OK on success
 */
esp_err_t st7789_draw_pixel(uint16_t x, uint16_t y, uint16_t color);

/**
 * @brief Draw a filled rectangle
 * @param x X coordinate of top-left corner
 * @param y Y coordinate of top-left corner
 * @param width Rectangle width
 * @param height Rectangle height
 * @param color 16-bit RGB565 color
 * @return ESP_OK on success
 */
esp_err_t st7789_fill_rect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color);

/**
 * @brief Draw a line
 * @param x0 Start X coordinate
 * @param y0 Start Y coordinate
 * @param x1 End X coordinate
 * @param y1 End Y coordinate
 * @param color 16-bit RGB565 color
 * @return ESP_OK on success
 */
esp_err_t st7789_draw_line(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);

/**
 * @brief Display RGB color pattern test
 * @return ESP_OK on success
 */
esp_err_t st7789_test_rgb_pattern(void);

/**
 * @brief Display gradient test pattern
 * @return ESP_OK on success
 */
esp_err_t st7789_test_gradient(void);

/**
 * @brief Display color bars test pattern
 * @return ESP_OK on success
 */
esp_err_t st7789_test_color_bars(void);

/**
 * @brief Display checkerboard test pattern
 * @return ESP_OK on success
 */
esp_err_t st7789_test_checkerboard(void);

/**
 * @brief Convert RGB888 to RGB565 format
 * @param r Red component (0-255)
 * @param g Green component (0-255)
 * @param b Blue component (0-255)
 * @return 16-bit RGB565 color value
 */
uint16_t st7789_rgb888_to_rgb565(uint8_t r, uint8_t g, uint8_t b);

#endif // ST7789V2_H