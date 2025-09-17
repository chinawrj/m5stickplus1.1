# GitHub Copilot Instructions for M5StickC Plus 1.1 ESP-IDF Project

You are an AI coding assistant specializing in ESP-IDF embedded development for the M5StickC Plus 1.1 hardware platform. This project integrates LVGL GUI framework with AXP192 power management and ST7789v2 display driver.

## Project Overview

**Hardware Platform**: M5StickC Plus 1.1 with ESP32-PICO-D4  
**Framework**: ESP-IDF 5.5.1  
**GUI Library**: LVGL 8.4.0  
**Primary Components**: AXP192 power management, ST7789v2 TFT display (1.14", 135x240), MPU6886 IMU, buttons, LEDs, audio  

## Development Environment Setup

### ESP-IDF Environment Activation
Before working with this project, always activate the ESP-IDF environment:

```bash
# Navigate to your ESP-IDF installation directory
cd ~/esp/esp-idf

# Activate ESP-IDF environment (run this in every new terminal session)
. ./export.sh

# Verify ESP-IDF is active (should show version 5.5.1)
idf.py --version
```

### Build and Flash Commands
Use these specific commands for M5StickC Plus development:

```bash
# source the export.sh before any idf.py commands
source ~/esp/esp-idf/export.sh

# Set target chip (run once per project)
source ~/esp/esp-idf/export.sh && idf.py set-target esp32

# Build the project
source ~/esp/esp-idf/export.sh && idf.py build

# Must Flash only with 1500000 baud rate
source ~/esp/esp-idf/export.sh && idf.py -b 1500000 flash

# Must Monitor with 115200 baud rate
source ~/esp/esp-idf/export.sh && idf.py -b 115200 monitor

# Clean build (when needed)
source ~/esp/esp-idf/export.sh && idf.py fullclean
```

**Important Notes**:
- Always use baud rate 500K for optimal communication with M5StickC Plus
- M5StickC Plus supports multiple baud rates: 115200, 250K, 500K, 750K, 1500K bps
- 500K provides faster flashing while maintaining stability
- The USB port typically appears as `/dev/cu.usbserial-*` on macOS
- Activate ESP-IDF environment in every terminal session before using `idf.py`

## Architecture & Design Principles

### 1. Power Management Safety First
- **ALWAYS** use safe power management APIs from `axp192.h`
- **NEVER** use direct voltage setting functions outside of `axp192.c`
- Follow official M5StickC Plus power specifications:
  - LDO3: 3.0V (TFT Display IC)
  - LDO2: 3.3V (TFT Backlight)  
  - LDO0: 3.3V (Microphone)
  - DCDC1: 3.3V (ESP32 Main)
  - EXTEN: 5.0V (GROVE Port)

```c
// ✅ CORRECT: Use safe power APIs
axp192_power_tft_display(true);      // Automatically uses 3.0V
axp192_power_tft_backlight(true);    // Automatically uses 3.3V

// ❌ WRONG: Direct voltage control (avoid)
axp192_set_ldo3_voltage(3000);       // Dangerous if incorrect value
```

### 2. Thread Safety for LVGL
- **CRITICAL**: LVGL operations must occur in dedicated tasks
- Initialize LVGL in separate task (`lvgl_init_task`) after hardware setup
- Use proper task synchronization with global flags
- Never call LVGL functions from interrupt handlers

```c
// ✅ CORRECT: LVGL in dedicated task
static void lvgl_init_task(void *pvParameters) {
    // Wait for hardware initialization
    while (!g_hardware_demo_ready) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    // Initialize LVGL safely
    esp_err_t ret = lvgl_init_with_lcd_panel(NULL);
    // Create timer task and demos...
}
```

### 3. Display Configuration Specifics
- **M5StickC Plus ST7789v2 Settings**:
  - Driver Chip: ST7789v2 (official M5Stack specification)
  - Resolution: 135x240 pixels
  - Screen Size: 1.14 inch TFT LCD
  - Panel offset: X=52, Y=40 (required for proper alignment)
  - Color format: RGB565 with byte swap (`CONFIG_LV_COLOR_16_SWAP=y`)
  - SPI frequency: 10MHz (stable) or 20MHz (higher performance)
  - Rotation: Landscape mode (`disp_drv.rotated = 1`)

```c
// ✅ CORRECT: M5StickC Plus display configuration
ret = esp_lcd_panel_set_gap(panel_handle, 52, 40);  // Critical offsets
ret = esp_lcd_panel_mirror(panel_handle, false, true);  // Y-axis mirror
ret = esp_lcd_panel_swap_xy(panel_handle, true);  // Swap for landscape
```

## File Organization & Responsibilities

### Core Hardware Modules
- `axp192.c/.h` - Power management with safety-first API design
- `st7789_lcd.c/.h` - ST7789v2 TFT display driver with M5StickC Plus optimizations
- `lvgl_helper.c/.h` - LVGL integration layer with power management
- `button.c/.h` - GPIO button handling with interrupt support
- `red_led.c/.h` - Status LED control with patterns
- `buzzer.c/.h` - Audio output management

### Application Layer
- `espnow_example_main.c` - Main application with task orchestration
- `espnow_example.h` - Project-wide definitions and configurations

### Configuration Files
- `sdkconfig` - ESP-IDF project configuration with LVGL optimizations
- `idf_component.yml` - Component dependencies (LVGL 8.x)
- `CMakeLists.txt` - Build system configuration

## Coding Standards & Patterns

### 1. Error Handling Pattern
```c
esp_err_t function_name(void) {
    esp_err_t ret;
    
    ret = some_operation();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Operation failed: %s", esp_err_to_name(ret));
        // Cleanup on failure
        cleanup_resources();
        return ret;
    }
    
    // Verify state after critical operations
    vTaskDelay(pdMS_TO_TICKS(100));  // Stabilization delay
    if (!verify_operation()) {
        ESP_LOGE(TAG, "Verification failed");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}
```

### 2. Hardware Initialization Sequence
```c
void app_main(void) {
    // 1. Basic ESP-IDF initialization
    esp_err_t ret = nvs_flash_init();
    ESP_ERROR_CHECK(ret);
    
    // 2. Power management first
    ESP_ERROR_CHECK(axp192_init());
    
    // 3. Hardware peripherals
    ESP_ERROR_CHECK(red_led_init());
    ESP_ERROR_CHECK(button_init());
    ESP_ERROR_CHECK(buzzer_init());
    
    // 4. Start hardware demo tasks
    xTaskCreate(axp192_monitor_task, "axp192_monitor", 8192, NULL, 5, NULL);
    
    // 5. LVGL initialization in separate task (thread safety)
    xTaskCreate(lvgl_init_task, "lvgl_init", 8192, NULL, 4, NULL);
    
    // 6. Network/application features last
    example_wifi_init();
    example_espnow_init();
}
```

### 3. Memory Management
- Use appropriate stack sizes: 8192 bytes for LVGL tasks, 4096 for simple tasks
- Configure LVGL memory: `CONFIG_LV_MEM_SIZE_KILOBYTES=64`
- Use static buffers for display: `lv_color_t lvgl_disp_buf[SIZE/2]`

## Hardware-Specific Guidelines

### GPIO Pin Mapping Reference
| Function | GPIO | Configuration | Notes |
|----------|------|---------------|-------|
| Button A | 37 | Input, pull-up | Active low |
| Button B | 39 | Input, pull-up | Active low |
| Red LED | 10 | Output | Active high |
| TFT MOSI | 15 | SPI output | 20MHz capable |
| TFT CLK | 13 | SPI clock | Shared with SD |
| TFT CS | 5 | Output | Active low |
| TFT DC | 23 | Output | Data/Command |
| TFT RST | 18 | Output | Hardware reset |
| I2C SDA | 21 | I2C data | AXP192, MPU6886 |
| I2C SCL | 22 | I2C clock | 100kHz standard |

### Display Color Examples
```c
// Use these proven color definitions
#define COLOR_BLACK   0x0000
#define COLOR_WHITE   0xFFFF
#define COLOR_RED     0xF800
#define COLOR_GREEN   0x07E0
#define COLOR_BLUE    0x001F
#define COLOR_YELLOW  0xFFE0
#define COLOR_CYAN    0x07FF
#define COLOR_MAGENTA 0xF81F

// Convert RGB888 to RGB565
uint16_t custom_color = st7789_rgb888_to_rgb565(255, 128, 64);
```

## Debug & Testing Strategies

### 1. Hardware Verification Checklist
```c
void hardware_self_test(void) {
    ESP_LOGI(TAG, "🔧 Starting hardware self-test...");
    
    // Test AXP192 communication
    assert(axp192_init() == ESP_OK);
    
    // Test power channels
    assert(axp192_power_tft_display(true) == ESP_OK);
    assert(axp192_power_tft_backlight(true) == ESP_OK);
    
    // Test display initialization
    assert(st7789_lcd_init() == ESP_OK);
    
    // Test LVGL integration
    assert(lvgl_init_with_lcd_panel(NULL) == ESP_OK);
}
```

### 2. Common Debugging Techniques
- Enable verbose logging: `idf.py menuconfig` → Component config → Log output → Debug
- Monitor power states: Use `axp192_get_battery_voltage()` for power debugging
- Display flush debugging: Monitor `lvgl_flush_cb()` call frequency
- Task stack monitoring: Use `uxTaskGetStackHighWaterMark()`

### 3. Performance Optimization
- Display refresh rate: 30ms (`CONFIG_LV_DISP_DEF_REFR_PERIOD=30`)
- Input device polling: 30ms (`CONFIG_LV_INDEV_DEF_READ_PERIOD=30`)
- SPI clock optimization: Start with 10MHz, increase to 20MHz if stable

## Examples & Code Templates

Remember: This is an embedded system with limited resources. Always prioritize reliability, safety, and efficient resource usage over complex features.