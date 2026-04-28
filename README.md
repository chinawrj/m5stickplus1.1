# M5StickC Plus 1.1 ESP-IDF Project | M5StickC Plus 1.1 ESP-IDF 项目

🚀 **Enable all hardware devices on M5StickC Plus 1.1 using ESP-IDF** | **使用ESP-IDF完全启用M5StickC Plus 1.1所有硬件设备**

[English](#english) | [中文](#中文)

## English

### 📋 Project Overview

This project is based on ESP-IDF 5.5.1 framework, providing complete hardware driver support for M5StickC Plus 1.1 development board. It focuses on implementing safe and reliable AXP192 power management chip driver to ensure all peripheral devices work properly.

### 🎯 Project Goals

- ✅ **Power Management**: Safe control of AXP192 power management chip
- ✅ **Display Support**: TFT display screen and backlight control
- ✅ **Audio Support**: Microphone module power management
- ✅ **Expansion Interface**: 5V GROVE port control
- ✅ **Battery Monitoring**: Voltage, current, capacity, charging status detection
- ✅ **IMU Support**: MPU6886 9-axis sensor (I2C)
- 🎯 **Safety Design**: Prevent hardware damage from misoperations

### 🛠️ Hardware Specifications

#### M5StickC Plus 1.1 Core Parameters
- **MCU**: ESP32-PICO-D4 (Dual-core Xtensa LX6, 240MHz)
- **Memory**: 4MB Flash + 320KB RAM
- **Display**: 1.14-inch TFT LCD (135×240)
- **Battery**: 120mAh Lithium battery
- **Power Management**: AXP192
- **IMU**: MPU6886 (Accelerometer + Gyroscope)
- **Interface**: USB-C, GROVE(I2C), GPIO

#### AXP192 Power Management Chip
| Power Channel | Voltage | Purpose |
|---------------|---------|---------|
| LDO3 | 3.0V | TFT Display IC |
| LDO2 | 3.3V | TFT Backlight |
| LDO0 | 3.3V | Microphone |
| DCDC1 | 3.3V | ESP32 Main Controller |
| EXTEN | 5.0V | GROVE Port |

### � Hardware Connections

#### GPIO Pin Mapping
| Function | GPIO | Description | Notes |
|----------|------|-------------|-------|
| **Buttons** | | | |
| Button A | GPIO37 | Built-in button A | Pull-up, active low |
| Button B | GPIO39 | Built-in button B | Pull-up, active low |
| **LEDs** | | | |
| Red LED | GPIO10 | Status indicator | Active high |
| IR LED | GPIO9 | Infrared transmitter | PWM capable |
| **I2C Bus** | | | |
| I2C SDA | GPIO21 | I2C data line | AXP192, MPU6886 |
| I2C SCL | GPIO22 | I2C clock line | 100kHz standard |
| **TFT Display** | | | |
| TFT MOSI | GPIO15 | SPI data output | 20MHz SPI |
| TFT SCLK | GPIO13 | SPI clock | Shared with SD |
| TFT CS | GPIO5 | Chip select | Active low |
| TFT DC | GPIO23 | Data/Command | High=data, Low=cmd |
| TFT RST | GPIO18 | Hardware reset | Active low |
| **Audio** | | | |
| MIC CLK | GPIO0 | PDM microphone clock | |
| MIC DATA | GPIO34 | PDM microphone data | Input only |
| **GROVE Port** | | | |
| GROVE SDA | GPIO21 | I2C data (shared) | 3.3V or 5V selectable |
| GROVE SCL | GPIO22 | I2C clock (shared) | External pull-up required |

#### I2C Device Addresses
| Device | Address | Function |
|--------|---------|----------|
| AXP192 | 0x34 | Power management |
| MPU6886 | 0x68 | 6-axis IMU sensor |

#### Hardware Connection Diagram
```
M5StickC Plus 1.1 Block Diagram:

   ESP32-PICO-D4
        │
        ├─── I2C Bus (GPIO21/22) ──┬─── AXP192 (Power Management)
        │                          └─── MPU6886 (IMU Sensor)
        │
        ├─── SPI Bus (GPIO13/15) ───┬─── ST7789 TFT Display
        │                          └─── (Future: SD Card)
        │
        ├─── GPIO37/39 ────────────── Buttons A/B
        ├─── GPIO9/10 ─────────────── IR LED / Red LED
        ├─── GPIO0/34 ─────────────── PDM Microphone
        └─── USB-C ───────────────── Programming/Power
```

### �🔧 Development Environment

#### Required Software
- **ESP-IDF**: v5.5.1+
- **Python**: 3.8+
- **CMake**: 3.16+
- **Toolchain**: xtensa-esp32-elf-gcc

#### Installing ESP-IDF
```bash
# Download ESP-IDF
git clone -b v5.5.1 --recursive https://github.com/espressif/esp-idf.git

# Install dependencies
cd esp-idf
./install.sh

# Set environment variables
. ./export.sh
```

### 🚀 Quick Start

#### 1. Clone Project
```bash
git clone https://github.com/your-username/m5stickplus1.1-esp-idf.git
cd m5stickplus1.1-esp-idf
```

#### 2. Configure Project
```bash
# Set target chip
idf.py set-target esp32

# Configure project (optional)
idf.py menuconfig
```

#### 3. Build Project
```bash
idf.py build
```

#### 4. Flash Program
```bash
# Find USB port
ls /dev/cu.usbserial-*

# Flash at 1500000 baud (recommended for M5StickC Plus)
idf.py -b 1500000 -p /dev/cu.usbserial-[YOUR_PORT] flash

# Monitor at 115200 baud
idf.py -b 115200 -p /dev/cu.usbserial-[YOUR_PORT] monitor
```

> **Note**: M5StickC Plus 1.1 uses ESP32-PICO-D4 with **4MB flash**. This project uses a custom partition table (`partitions.csv`) with a 1.5MB factory app partition to accommodate the full-featured binary.

### 📚 API Usage Guide

#### 🛡️ Safe Power Management API (Recommended)

```c
#include "axp192.h"

// Initialize AXP192
esp_err_t ret = axp192_init();

// Safe hardware control - automatically uses correct voltage
axp192_power_tft_display(true);      // Enable screen (3.0V)
axp192_power_tft_backlight(true);    // Enable backlight (3.3V)
axp192_power_microphone(true);       // Enable microphone (3.3V)
axp192_power_grove_5v(true);         // Enable 5V output

// Battery status monitoring
float voltage, current, power;
uint8_t level;
axp192_get_battery_voltage(&voltage);    // Battery voltage
axp192_get_battery_current(&current);    // Battery current
axp192_get_battery_power(&power);        // Battery power
axp192_get_battery_level(&level);        // Battery level percentage

// Charging status check
bool charging = axp192_is_charging();
bool battery_present = axp192_is_battery_present();
```

#### 📊 Battery Monitoring Example

```c
void battery_monitor_task(void *param) {
    while (1) {
        float voltage, current, power, temp;
        uint8_t level;
        
        // Get battery information
        if (axp192_get_battery_voltage(&voltage) == ESP_OK &&
            axp192_get_battery_current(&current) == ESP_OK &&
            axp192_get_battery_level(&level) == ESP_OK) {
            
            ESP_LOGI(TAG, "🔋 Battery: %.2fV, %.1fmA, %d%%", 
                     voltage, current, level);
            
            if (axp192_is_charging()) {
                ESP_LOGI(TAG, "🔌 Charging");
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(5000)); // Update every 5 seconds
    }
}
```

#### 🖥️ ST7789 TFT Display Control

```c
#include "st7789_driver.h"

// Initialize TFT display
esp_lcd_panel_handle_t panel_handle = NULL;
esp_err_t ret = st7789_init(&panel_handle);

if (ret == ESP_OK) {
    // Fill screen with solid color
    st7789_fill_screen(panel_handle, ST7789_COLOR_BLUE);
    
    // Draw rectangles
    st7789_draw_rect(panel_handle, 10, 10, 50, 30, ST7789_COLOR_RED);
    st7789_draw_rect(panel_handle, 70, 50, 40, 40, ST7789_COLOR_GREEN);
    
    // Run comprehensive test patterns
    st7789_test_patterns(panel_handle);
    
    // Convert RGB to display format
    uint16_t custom_color = st7789_rgb888_to_rgb565(255, 128, 64);
    st7789_fill_screen(panel_handle, custom_color);
}
```

#### 🎨 TFT Display Features

**Display Specifications:**
- **Resolution**: 135×240 pixels
- **Color Format**: RGB565 (16-bit)
- **Panel Offset**: X=52, Y=40 (M5StickC Plus specific)
- **SPI Interface**: 20MHz pixel clock

**GPIO Configuration:**
| Pin | Function | GPIO |
|-----|----------|------|
| MOSI | SPI Data | GPIO15 |
| SCLK | SPI Clock | GPIO13 |
| CS | Chip Select | GPIO5 |
| DC | Data/Command | GPIO23 |
| RST | Reset | GPIO18 |

**Available Functions:**
- `st7789_init()` - Initialize display with ESP-IDF LCD driver
- `st7789_fill_screen()` - Fill entire screen with color
- `st7789_draw_rect()` - Draw filled rectangles
- `st7789_test_patterns()` - Display test patterns (color bars, gradients, shapes)
- `st7789_rgb888_to_rgb565()` - Color format conversion
- `st7789_set_brightness()` - Display power control

**Test Patterns Include:**
1. 🎨 Solid colors (Red, Green, Blue, White)
2. 📊 Color bars (horizontal stripes)
3. 🌈 Gradient patterns (grayscale)
4. 🔲 Geometric shapes (colored rectangles)

### 🔒 Safety Features

#### Hardware Protection Design
1. **Fixed Voltage Values**: All safe APIs use officially recommended fixed voltages
2. **Function Hiding**: Dangerous direct voltage control functions set to `static`, inaccessible externally
3. **Status Verification**: All operations include error checking and status validation
4. **Official Compatibility**: Based on M5Unified official library best practices

#### Why Choose Safe API?
❌ **Dangerous**: Direct voltage setting may damage hardware
```c
axp192_set_ldo3_voltage(5000);  // May burn the screen!
```

✅ **Safe**: Use preset correct voltages
```c
axp192_power_tft_display(true); // Automatically uses 3.0V
```

### 🔍 Debug and Testing

#### Hardware Testing Checklist
```c
// Basic hardware verification
void hardware_self_test(void) {
    ESP_LOGI(TAG, "🔧 Starting hardware self-test...");
    
    // 1. AXP192 Communication Test
    if (axp192_init() == ESP_OK) {
        ESP_LOGI(TAG, "✅ AXP192 communication OK");
    } else {
        ESP_LOGE(TAG, "❌ AXP192 communication failed");
    }
    
    // 2. Battery Status Test
    float voltage;
    if (axp192_get_battery_voltage(&voltage) == ESP_OK) {
        ESP_LOGI(TAG, "✅ Battery voltage: %.2fV", voltage);
    }
    
    // 3. I2C Device Scan
    ESP_LOGI(TAG, "🔍 Scanning I2C devices...");
    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
        if (i2c_device_exists(addr)) {
            ESP_LOGI(TAG, "📍 Found device at 0x%02X", addr);
        }
    }
    
    // 4. TFT Display Test
    if (st7789_init() == ESP_OK) {
        ESP_LOGI(TAG, "✅ TFT display initialized");
        st7789_test_patterns(); // Run display tests
    }
}
```

#### Software Debugging
```bash
# Enable detailed debug output
idf.py menuconfig
# Navigate to: Component config → Log output → Default log verbosity → Debug

# Build with debug symbols
idf.py build

# Monitor with detailed logs
idf.py monitor --print-filter="*:D"

# GDB debugging session
idf.py gdb
(gdb) break app_main
(gdb) continue
(gdb) info registers
```

#### Component Testing
```c
// Power management unit test
void test_power_management(void) {
    // Test all power channels
    assert(axp192_power_tft_display(true) == ESP_OK);
    assert(axp192_power_tft_backlight(true) == ESP_OK);
    assert(axp192_power_microphone(true) == ESP_OK);
    assert(axp192_power_grove_5v(true) == ESP_OK);
    
    // Verify power states
    ESP_LOGI(TAG, "🔋 All power channels enabled successfully");
}

// Display driver test
void test_display_driver(void) {
    // Color fill tests
    uint16_t colors[] = {
        ST7789_COLOR_RED, ST7789_COLOR_GREEN, 
        ST7789_COLOR_BLUE, ST7789_COLOR_WHITE
    };
    
    for (int i = 0; i < 4; i++) {
        st7789_fill_screen(panel_handle, colors[i]);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    
    ESP_LOGI(TAG, "🎨 Display test completed");
}
```

### 🔒 Security and Best Practices

#### Error Handling Patterns
```c
// Example: Robust power management with error handling
esp_err_t safe_hardware_init(void) {
    esp_err_t ret;
    
    // Initialize AXP192 with timeout
    ret = axp192_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "AXP192 init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Enable display with verification
    ret = axp192_power_tft_display(true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Display power failed: %s", esp_err_to_name(ret));
        axp192_deinit(); // Cleanup on failure
        return ret;
    }
    
    // Verify power state
    vTaskDelay(pdMS_TO_TICKS(100)); // Wait for stabilization
    if (!axp192_is_display_powered()) {
        ESP_LOGE(TAG, "Display power verification failed");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "✅ Hardware initialized safely");
    return ESP_OK;
}
```

#### Code Quality Guidelines
1. **Always check return values** from hardware functions
2. **Use watchdog timer** for critical operations
3. **Implement graceful shutdown** on low battery
4. **Add timeout protection** for I2C operations
5. **Use appropriate log levels** (DEBUG, INFO, WARN, ERROR)

#### Battery Safety
```c
// Low battery protection
void battery_safety_monitor(void *param) {
    while (1) {
        float voltage;
        if (axp192_get_battery_voltage(&voltage) == ESP_OK) {
            if (voltage < 3.0f) {
                ESP_LOGW(TAG, "⚠️ Low battery: %.2fV", voltage);
                // Enter power saving mode
                esp_deep_sleep_start();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10000)); // Check every 10s
    }
}
```

### 📚 Examples and Tutorials

#### Basic Examples
```
examples/
├── 01_power_management/     # AXP192 basic usage and safety
├── 02_display_hello/        # Simple "Hello World" on TFT
├── 03_battery_monitor/      # Real-time battery status display
├── 04_button_input/         # Button interrupt handling
├── 05_sensor_reading/       # MPU6886 accelerometer/gyroscope
└── 06_grove_expansion/      # Using GROVE port for sensors
```

#### Example 1: Simple Battery Monitor
```c
#include "axp192.h"
#include "st7789_driver.h"

void app_main(void) {
    // Initialize hardware
    axp192_init();
    axp192_power_tft_display(true);
    axp192_power_tft_backlight(true);
    
    st7789_init();
    
    while (1) {
        float voltage, current;
        uint8_t level;
        
        // Get battery info
        axp192_get_battery_voltage(&voltage);
        axp192_get_battery_current(&current);
        axp192_get_battery_level(&level);
        
        // Display on screen
        st7789_fill_screen(ST7789_COLOR_BLACK);
        char text[64];
        snprintf(text, sizeof(text), "Battery: %.2fV\nLevel: %d%%", voltage, level);
        // st7789_draw_text(10, 50, text, ST7789_COLOR_WHITE);
        
        ESP_LOGI(TAG, "🔋 %s", text);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
```

#### Example 2: Button Input Handler
```c
#include "driver/gpio.h"

#define BUTTON_A_GPIO  37
#define BUTTON_B_GPIO  39

static void IRAM_ATTR button_isr_handler(void* arg) {
    uint32_t gpio_num = (uint32_t) arg;
    // Send event to task queue for processing
    // Don't do heavy work in ISR
}

void setup_buttons(void) {
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_NEGEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << BUTTON_A_GPIO) | (1ULL << BUTTON_B_GPIO),
        .pull_down_en = 0,
        .pull_up_en = 1,
    };
    gpio_config(&io_conf);
    
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    gpio_isr_handler_add(BUTTON_A_GPIO, button_isr_handler, (void*) BUTTON_A_GPIO);
    gpio_isr_handler_add(BUTTON_B_GPIO, button_isr_handler, (void*) BUTTON_B_GPIO);
}
```

### 📖 Complete API Reference

#### AXP192 Power Management Functions
```c
// Initialization
esp_err_t axp192_init(void);                    // Initialize AXP192 chip
esp_err_t axp192_deinit(void);                  // Cleanup resources

// Safe Power Control (Recommended)
esp_err_t axp192_power_tft_display(bool enable);    // 3.0V for display IC
esp_err_t axp192_power_tft_backlight(bool enable);  // 3.3V for backlight
esp_err_t axp192_power_microphone(bool enable);     // 3.3V for microphone
esp_err_t axp192_power_grove_5v(bool enable);       // 5.0V for GROVE port

// Battery Monitoring
esp_err_t axp192_get_battery_voltage(float *voltage);    // Battery voltage (V)
esp_err_t axp192_get_battery_current(float *current);    // Battery current (mA)
esp_err_t axp192_get_battery_power(float *power);        // Battery power (mW)
esp_err_t axp192_get_battery_level(uint8_t *level);      // Battery level (0-100%)

// Status Check
bool axp192_is_charging(void);              // Is battery charging?
bool axp192_is_battery_present(void);       // Is battery connected?
bool axp192_is_vbus_present(void);          // Is USB power connected?
```

#### ST7789 Display Functions
```c
// Display Control
esp_err_t st7789_init(esp_lcd_panel_handle_t *panel_handle);
esp_err_t st7789_deinit(esp_lcd_panel_handle_t panel_handle);

// Drawing Functions
esp_err_t st7789_fill_screen(esp_lcd_panel_handle_t panel, uint16_t color);
esp_err_t st7789_draw_rect(esp_lcd_panel_handle_t panel, 
                          int x, int y, int width, int height, uint16_t color);

// Utility Functions
uint16_t st7789_rgb888_to_rgb565(uint8_t r, uint8_t g, uint8_t b);
esp_err_t st7789_test_patterns(esp_lcd_panel_handle_t panel);

// Color Constants
#define ST7789_COLOR_BLACK   0x0000
#define ST7789_COLOR_WHITE   0xFFFF
#define ST7789_COLOR_RED     0xF800
#define ST7789_COLOR_GREEN   0x07E0
#define ST7789_COLOR_BLUE    0x001F
```

#### Error Codes Reference
| Error Code | Description | Common Solutions |
|------------|-------------|------------------|
| `ESP_OK` | Success | No action needed |
| `ESP_ERR_INVALID_ARG` | Invalid parameter | Check function arguments |
| `ESP_ERR_NOT_FOUND` | Device not found | Check I2C connections and addresses |
| `ESP_ERR_TIMEOUT` | Operation timeout | Check I2C speed, add delays |
| `ESP_ERR_INVALID_STATE` | Invalid state | Initialize device before use |
| `ESP_ERR_NO_MEM` | Out of memory | Reduce heap usage, check for leaks |

---

## 中文

### 📋 项目概述

本项目基于ESP-IDF 5.5.1框架，为M5StickC Plus 1.1开发板提供完整的硬件驱动支持。重点实现了安全可靠的AXP192电源管理芯片驱动，确保所有外设设备正常工作。

### 🎯 项目目标

- ✅ **电源管理**: 安全控制AXP192电源管理芯片
- ✅ **屏幕支持**: TFT显示屏和背光控制
- ✅ **音频支持**: 麦克风模块电源管理
- ✅ **扩展接口**: 5V GROVE端口控制
- ✅ **电池监控**: 电压、电流、电量、充电状态监测
- ✅ **IMU支持**: MPU6886九轴传感器(I2C)
- 🎯 **安全设计**: 防止误操作损坏硬件

### 🛠️ 硬件规格

#### M5StickC Plus 1.1 核心参数
- **主控**: ESP32-PICO-D4 (双核Xtensa LX6, 240MHz)
- **内存**: 4MB Flash + 320KB RAM
- **屏幕**: 1.14英寸 TFT LCD (135×240)
- **电池**: 120mAh锂电池
- **电源管理**: AXP192
- **IMU**: MPU6886 (加速度计+陀螺仪)
- **接口**: USB-C, GROVE(I2C), GPIO

#### 电源管理芯片 AXP192
| 电源通道 | 电压 | 用途 |
|---------|------|------|
| LDO3 | 3.0V | TFT显示IC |
| LDO2 | 3.3V | TFT背光 |
| LDO0 | 3.3V | 麦克风 |
| DCDC1 | 3.3V | ESP32主控 |
| EXTEN | 5.0V | GROVE端口 |

#### TFT显示屏 ST7789v2
| 规格项 | 参数 |
|--------|------|
| 型号 | ST7789v2 |
| 尺寸 | 1.14英寸 |
| 分辨率 | 135×240像素 |
| 颜色 | 16位 RGB565 |
| 像素时钟 | 20MHz |
| 通信接口 | SPI |

##### TFT GPIO引脚配置
| 功能 | GPIO | 说明 |
|------|------|------|
| MOSI (数据) | 15 | SPI数据输出 |
| CLK (时钟) | 13 | SPI时钟 |
| DC (数据/命令) | 23 | 数据命令选择 |
| RST (复位) | 18 | 硬件复位 |
| CS (片选) | 5 | SPI片选 |

##### 可用功能
- `st7789_init()` - 初始化显示屏
- `st7789_fill_screen()` - 全屏填充颜色
- `st7789_fill_rect()` - 绘制填充矩形
- `st7789_set_rotation()` - 设置屏幕旋转
- `st7789_test_patterns()` - 显示测试图案
- `st7789_rgb565()` - RGB颜色转换
- `st7789_deinit()` - 清理资源

##### 测试图案
- 纯色填充测试 (红/绿/蓝/白/黑)
- 垂直条纹图案
- 水平条纹图案
- 对角线渐变
- 彩虹色渐变

### 🔧 开发环境

#### 必需软件
- **ESP-IDF**: v5.5.1+
- **Python**: 3.8+
- **CMake**: 3.16+
- **工具链**: xtensa-esp32-elf-gcc

#### 安装ESP-IDF
```bash
# 下载ESP-IDF
git clone -b v5.5.1 --recursive https://github.com/espressif/esp-idf.git

# 安装依赖
cd esp-idf
./install.sh

# 设置环境变量
. ./export.sh
```

### 🚀 快速开始

#### 1. 克隆项目
```bash
git clone https://github.com/your-username/m5stickplus1.1-esp-idf.git
cd m5stickplus1.1-esp-idf
```

#### 2. 配置项目
```bash
# 设置目标芯片
idf.py set-target esp32

# 配置项目(可选)
idf.py menuconfig
```

#### 3. 编译项目
```bash
idf.py build
```

#### 4. 烧录程序
```bash
# 查找USB端口
ls /dev/cu.usbserial-*

# 以1500000波特率烧录（M5StickC Plus推荐）
idf.py -b 1500000 -p /dev/cu.usbserial-[YOUR_PORT] flash

# 以115200波特率监控
idf.py -b 115200 -p /dev/cu.usbserial-[YOUR_PORT] monitor
```

> **注意**：M5StickC Plus 1.1使用ESP32-PICO-D4，搭载**4MB Flash**。本项目使用自定义分区表（`partitions.csv`），工厂应用分区为1.5MB，可容纳完整功能的固件二进制文件。

### 📚 API使用指南

#### 🛡️ 安全电源管理API (推荐)

```c
#include "axp192.h"

// 初始化AXP192
esp_err_t ret = axp192_init();

// 安全的硬件控制 - 自动使用正确电压
axp192_power_tft_display(true);      // 启用屏幕 (3.0V)
axp192_power_tft_backlight(true);    // 启用背光 (3.3V)
axp192_power_microphone(true);       // 启用麦克风 (3.3V)
axp192_power_grove_5v(true);         // 启用5V输出

// 电池状态监控
float voltage, current, power;
uint8_t level;
axp192_get_battery_voltage(&voltage);    // 电池电压
axp192_get_battery_current(&current);    // 电池电流
axp192_get_battery_power(&power);        // 电池功率
axp192_get_battery_level(&level);        // 电池电量百分比

// 充电状态检查
bool charging = axp192_is_charging();
bool battery_present = axp192_is_battery_present();
```

#### 📊 电池监控示例

```c
void battery_monitor_task(void *param) {
    while (1) {
        float voltage, current, power, temp;
        uint8_t level;
        
        // 获取电池信息
        if (axp192_get_battery_voltage(&voltage) == ESP_OK &&
            axp192_get_battery_current(&current) == ESP_OK &&
            axp192_get_battery_level(&level) == ESP_OK) {
            
            ESP_LOGI(TAG, "🔋 电池: %.2fV, %.1fmA, %d%%", 
                     voltage, current, level);
            
            if (axp192_is_charging()) {
                ESP_LOGI(TAG, "🔌 充电中");
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(5000)); // 5秒更新一次
    }
}
```

### 🔒 安全特性

#### 硬件保护设计
1. **固定电压值**: 所有安全API使用官方推荐的固定电压
2. **函数隐藏**: 危险的直接电压控制函数设为`static`，外部无法访问
3. **状态验证**: 所有操作都包含错误检查和状态验证
4. **官方兼容**: 基于M5Unified官方库的最佳实践

#### 为什么选择安全API？
❌ **危险**: 直接设置电压可能损坏硬件
```c
axp192_set_ldo3_voltage(5000);  // 可能烧毁屏幕！
```

✅ **安全**: 使用预设的正确电压
```c
axp192_power_tft_display(true); // 自动使用3.0V
```

### � 调试和测试

#### 硬件测试清单
```c
// 基础硬件验证
void hardware_self_test(void) {
    ESP_LOGI(TAG, "🔧 开始硬件自检...");
    
    // 1. AXP192通信测试
    if (axp192_init() == ESP_OK) {
        ESP_LOGI(TAG, "✅ AXP192通信正常");
    } else {
        ESP_LOGE(TAG, "❌ AXP192通信失败");
    }
    
    // 2. 电池状态测试
    float voltage;
    if (axp192_get_battery_voltage(&voltage) == ESP_OK) {
        ESP_LOGI(TAG, "✅ 电池电压: %.2fV", voltage);
    }
    
    // 3. I2C设备扫描
    ESP_LOGI(TAG, "🔍 扫描I2C设备...");
    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
        if (i2c_device_exists(addr)) {
            ESP_LOGI(TAG, "📍 发现设备: 0x%02X", addr);
        }
    }
    
    // 4. TFT显示测试
    if (st7789_init() == ESP_OK) {
        ESP_LOGI(TAG, "✅ TFT显示屏初始化成功");
        st7789_test_patterns(); // 运行显示测试
    }
}
```

#### 软件调试
```bash
# 启用详细调试输出
idf.py menuconfig
# 导航到: Component config → Log output → Default log verbosity → Debug

# 带调试符号编译
idf.py build

# 监控详细日志
idf.py monitor --print-filter="*:D"

# GDB调试会话
idf.py gdb
(gdb) break app_main
(gdb) continue
(gdb) info registers
```

#### 组件测试
```c
// 电源管理单元测试
void test_power_management(void) {
    // 测试所有电源通道
    assert(axp192_power_tft_display(true) == ESP_OK);
    assert(axp192_power_tft_backlight(true) == ESP_OK);
    assert(axp192_power_microphone(true) == ESP_OK);
    assert(axp192_power_grove_5v(true) == ESP_OK);
    
    // 验证电源状态
    ESP_LOGI(TAG, "🔋 所有电源通道启用成功");
}

// 显示驱动测试
void test_display_driver(void) {
    // 颜色填充测试
    uint16_t colors[] = {
        ST7789_COLOR_RED, ST7789_COLOR_GREEN, 
        ST7789_COLOR_BLUE, ST7789_COLOR_WHITE
    };
    
    for (int i = 0; i < 4; i++) {
        st7789_fill_screen(panel_handle, colors[i]);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    
    ESP_LOGI(TAG, "🎨 显示测试完成");
}
```

### 🔒 安全性和最佳实践

#### 错误处理模式
```c
// 示例: 带错误处理的稳健电源管理
esp_err_t safe_hardware_init(void) {
    esp_err_t ret;
    
    // 带超时的AXP192初始化
    ret = axp192_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "AXP192初始化失败: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 带验证的显示器启用
    ret = axp192_power_tft_display(true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "显示器电源失败: %s", esp_err_to_name(ret));
        axp192_deinit(); // 失败时清理
        return ret;
    }
    
    // 验证电源状态
    vTaskDelay(pdMS_TO_TICKS(100)); // 等待稳定
    if (!axp192_is_display_powered()) {
        ESP_LOGE(TAG, "显示器电源验证失败");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "✅ 硬件安全初始化完成");
    return ESP_OK;
}
```

#### 代码质量指南
1. **始终检查返回值** - 所有硬件函数的返回值
2. **使用看门狗定时器** - 关键操作的保护
3. **实现优雅关机** - 低电量时的处理
4. **添加超时保护** - I2C操作的超时
5. **使用合适的日志级别** - DEBUG, INFO, WARN, ERROR

#### 电池安全
```c
// 低电量保护
void battery_safety_monitor(void *param) {
    while (1) {
        float voltage;
        if (axp192_get_battery_voltage(&voltage) == ESP_OK) {
            if (voltage < 3.0f) {
                ESP_LOGW(TAG, "⚠️ 电池电量低: %.2fV", voltage);
                // 进入省电模式
                esp_deep_sleep_start();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10000)); // 每10秒检查一次
    }
}
```

### 📚 示例和教程

#### 基础示例
```
examples/
├── 01_power_management/     # AXP192基础使用和安全性
├── 02_display_hello/        # TFT上的简单"Hello World"
├── 03_battery_monitor/      # 实时电池状态显示
├── 04_button_input/         # 按键中断处理
├── 05_sensor_reading/       # MPU6886加速度计/陀螺仪
└── 06_grove_expansion/      # 使用GROVE端口连接传感器
```

#### 示例1: 简单电池监视器
```c
#include "axp192.h"
#include "st7789_driver.h"

void app_main(void) {
    // 初始化硬件
    axp192_init();
    axp192_power_tft_display(true);
    axp192_power_tft_backlight(true);
    
    st7789_init();
    
    while (1) {
        float voltage, current;
        uint8_t level;
        
        // 获取电池信息
        axp192_get_battery_voltage(&voltage);
        axp192_get_battery_current(&current);
        axp192_get_battery_level(&level);
        
        // 在屏幕上显示
        st7789_fill_screen(ST7789_COLOR_BLACK);
        char text[64];
        snprintf(text, sizeof(text), "电池: %.2fV\n电量: %d%%", voltage, level);
        // st7789_draw_text(10, 50, text, ST7789_COLOR_WHITE);
        
        ESP_LOGI(TAG, "🔋 %s", text);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
```

#### 示例2: 按键输入处理
```c
#include "driver/gpio.h"

#define BUTTON_A_GPIO  37
#define BUTTON_B_GPIO  39

static void IRAM_ATTR button_isr_handler(void* arg) {
    uint32_t gpio_num = (uint32_t) arg;
    // 发送事件到任务队列处理
    // 不要在ISR中做繁重工作
}

void setup_buttons(void) {
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_NEGEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << BUTTON_A_GPIO) | (1ULL << BUTTON_B_GPIO),
        .pull_down_en = 0,
        .pull_up_en = 1,
    };
    gpio_config(&io_conf);
    
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    gpio_isr_handler_add(BUTTON_A_GPIO, button_isr_handler, (void*) BUTTON_A_GPIO);
    gpio_isr_handler_add(BUTTON_B_GPIO, button_isr_handler, (void*) BUTTON_B_GPIO);
}
```

### 📖 完整API参考

#### AXP192电源管理函数
```c
// 初始化
esp_err_t axp192_init(void);                    // 初始化AXP192芯片
esp_err_t axp192_deinit(void);                  // 清理资源

// 安全电源控制（推荐）
esp_err_t axp192_power_tft_display(bool enable);    // 3.0V供电显示IC
esp_err_t axp192_power_tft_backlight(bool enable);  // 3.3V供电背光
esp_err_t axp192_power_microphone(bool enable);     // 3.3V供电麦克风
esp_err_t axp192_power_grove_5v(bool enable);       // 5.0V供电GROVE端口

// 电池监控
esp_err_t axp192_get_battery_voltage(float *voltage);    // 电池电压 (V)
esp_err_t axp192_get_battery_current(float *current);    // 电池电流 (mA)
esp_err_t axp192_get_battery_power(float *power);        // 电池功率 (mW)
esp_err_t axp192_get_battery_level(uint8_t *level);      // 电池电量 (0-100%)

// 状态检查
bool axp192_is_charging(void);              // 是否正在充电？
bool axp192_is_battery_present(void);       // 是否连接电池？
bool axp192_is_vbus_present(void);          // 是否连接USB电源？
```

#### ST7789显示功能
```c
// 显示控制
esp_err_t st7789_init(esp_lcd_panel_handle_t *panel_handle);
esp_err_t st7789_deinit(esp_lcd_panel_handle_t panel_handle);

// 绘图功能
esp_err_t st7789_fill_screen(esp_lcd_panel_handle_t panel, uint16_t color);
esp_err_t st7789_draw_rect(esp_lcd_panel_handle_t panel, 
                          int x, int y, int width, int height, uint16_t color);

// 实用功能
uint16_t st7789_rgb888_to_rgb565(uint8_t r, uint8_t g, uint8_t b);
esp_err_t st7789_test_patterns(esp_lcd_panel_handle_t panel);

// 颜色常量
#define ST7789_COLOR_BLACK   0x0000
#define ST7789_COLOR_WHITE   0xFFFF
#define ST7789_COLOR_RED     0xF800
#define ST7789_COLOR_GREEN   0x07E0
#define ST7789_COLOR_BLUE    0x001F
```

#### 错误代码参考
| 错误代码 | 描述 | 常见解决方案 |
|----------|------|-------------|
| `ESP_OK` | 成功 | 无需操作 |
| `ESP_ERR_INVALID_ARG` | 无效参数 | 检查函数参数 |
| `ESP_ERR_NOT_FOUND` | 设备未找到 | 检查I2C连接和地址 |
| `ESP_ERR_TIMEOUT` | 操作超时 | 检查I2C速度，添加延时 |
| `ESP_ERR_INVALID_STATE` | 无效状态 | 使用前先初始化设备 |
| `ESP_ERR_NO_MEM` | 内存不足 | 减少堆使用，检查内存泄漏 |

### �📺 TFT显示屏API

#### 基础使用

```c
#include "st7789_driver.h"

// 初始化显示屏
esp_err_t ret = st7789_init();
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "TFT初始化失败: %s", esp_err_to_name(ret));
    return;
}

// 启用显示屏电源和背光
axp192_power_tft_display(true);
axp192_power_tft_backlight(true);

// 全屏填充颜色
st7789_fill_screen(st7789_rgb565(255, 0, 0));  // 红色

// 绘制矩形
st7789_fill_rect(10, 10, 50, 30, st7789_rgb565(0, 255, 0));  // 绿色矩形

// 设置屏幕旋转
st7789_set_rotation(1);  // 横屏模式

// 运行测试图案
st7789_test_patterns();
```

#### 颜色定义
```c
// 使用RGB565格式
uint16_t red = st7789_rgb565(255, 0, 0);
uint16_t green = st7789_rgb565(0, 255, 0);
uint16_t blue = st7789_rgb565(0, 0, 255);
uint16_t white = st7789_rgb565(255, 255, 255);
uint16_t black = st7789_rgb565(0, 0, 0);
```

#### 完整示例
```c
void display_demo(void) {
    // 初始化显示系统
    st7789_init();
    axp192_power_tft_display(true);
    axp192_power_tft_backlight(true);
    
    // 显示彩色条纹
    for (int y = 0; y < 240; y += 30) {
        uint16_t color = st7789_rgb565(y, 255 - y, (y * 2) % 255);
        st7789_fill_rect(0, y, 135, 30, color);
    }
    
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    // 运行完整测试序列
    st7789_test_patterns();
    
    // 清理资源
    st7789_deinit();
}
```

---

## 📁 Project Structure | 项目结构

```
m5stickplus1.1/
├── main/
│   ├── CMakeLists.txt           # Main program build config | 主程序构建配置
│   ├── espnow_example_main.c    # Main program and demo code | 主程序和演示代码
│   ├── espnow_example.h         # Project header file | 项目头文件
│   ├── axp192.h                 # AXP192 driver header | AXP192驱动头文件
│   ├── axp192.c                 # AXP192 driver implementation | AXP192驱动实现
│   └── Kconfig.projbuild        # Project configuration | 项目配置
├── CMakeLists.txt               # Top-level build config | 顶层构建配置
├── sdkconfig                    # ESP-IDF configuration | ESP-IDF配置
├── README.md                    # Project documentation | 项目说明
└── SAFETY_API_SUMMARY.md       # Detailed safety API doc | 安全API详细说明
```

## 🔍 Feature Demo | 功能演示

Current example program demonstrates: | 当前示例程序展示：
- ✅ AXP192 initialization and configuration | AXP192初始化和配置
- ✅ Safe enablement of all hardware modules | 所有硬件模块安全启用
- ✅ Complete battery status monitoring | 完整的电池状态监控
- ✅ TFT display initialization and test patterns | TFT显示屏初始化和测试图案
- ✅ Temperature monitoring | 温度监测
- ✅ VBUS status detection | VBUS状态检测
- ✅ Power saving mode control | 省电模式控制

## 🛠️ Build Optimization | 编译优化

### ESP-IDF Component Optimization | ESP-IDF组件优化
Project optimized to include only essential components: | 项目已优化，仅包含必需组件：
- `driver` - GPIO and I2C drivers | GPIO和I2C驱动
- `esp_system` - System core | 系统核心
- `freertos` - Real-time OS | 实时操作系统
- `log` - Logging system | 日志系统
- `nvs_flash` - Configuration storage | 配置存储

### Memory Usage | 内存使用
- **Flash Usage | Flash使用**: ~757KB (74% partition usage | 74%分区使用率)
- **RAM Usage | RAM使用**: Significantly reduced after optimization | 优化后显著减少
- **Components | 组件**: Streamlined to core functions | 精简至核心功能

## 🔧 Advanced Configuration | 高级配置

### I2C Configuration | I2C配置
```c
#define I2C_MASTER_SCL_IO       22      // GPIO22
#define I2C_MASTER_SDA_IO       21      // GPIO21
#define I2C_MASTER_FREQ_HZ      100000  // 100kHz
```

### AXP192 Device Address | AXP192设备地址
```c
#define AXP192_I2C_ADDR         0x34    // 7-bit address | 7位地址
```

## ⚠️ Important Notes | 注意事项

1. **Voltage Setting | 电压设置**: Please use only safe APIs, avoid direct voltage setting | 请只使用安全API，避免直接设置电压
2. **I2C Conflicts | I2C冲突**: Ensure MPU6886 and AXP192 don't have address conflicts | 确保MPU6886和AXP192不会产生地址冲突
3. **Power Sequence | 电源顺序**: Recommended sequence: ESP32 → Display → Other peripherals | 建议按顺序启用：ESP32 → 屏幕 → 其他外设
4. **Battery Protection | 电池保护**: Automatically enter power saving mode at low battery | 低电量时自动进入省电模式

## 🐛 Troubleshooting | 故障排除

### Common Issues | 常见问题

**Q: Compilation failed? | 编译失败？**
A: Ensure ESP-IDF version ≥5.5.1 and environment variables are set correctly | 确保ESP-IDF版本≥5.5.1，并正确设置环境变量

**Q: AXP192 communication failed? | AXP192通信失败？**
A: Check I2C pin connections, confirm device address 0x34 | 检查I2C引脚连接，确认设备地址0x34

**Q: Screen not lighting up? | 屏幕不亮？**
A: Ensure both display and backlight are enabled: | 确保同时启用display和backlight：
```c
axp192_power_tft_display(true);
axp192_power_tft_backlight(true);
```

**Q: Battery level display abnormal? | 电池电量显示异常？**
A: AXP192 needs time to calibrate, wait a few minutes and read again | AXP192需要时间校准，等待几分钟后重新读取

## 📈 Future Development Plan | 后续开发计划

- [ ] MPU6886 IMU driver implementation | MPU6886 IMU驱动实现
- [ ] TFT screen driver library integration | TFT屏幕驱动库集成
- [ ] WiFi functionality demo | WiFi功能演示
- [ ] **ESP-NOW ECDSA Key Exchange** | **ESP-NOW ECDSA密钥交互**
  - [ ] ECDSA key pair generation | ECDSA密钥对生成
  - [ ] Secure key exchange protocol | 安全密钥交换协议
  - [ ] Message signing and verification | 消息签名和验证
  - [ ] Device authentication mechanism | 设备认证机制
  - [ ] PMK (Pairwise Master Key) derivation | PMK（成对主密钥）派生
  - [ ] LMK (Local Master Key) generation | LMK（本地主密钥）生成
  - [ ] Session key management | 会话密钥管理
  - [ ] Key rotation and refresh mechanism | 密钥轮换和刷新机制
- [ ] Low power mode optimization | 低功耗模式优化
- [ ] OTA upgrade support | OTA升级支持
- [ ] Extended example applications | 示例应用扩展

## 🤝 Contributing | 贡献指南

Welcome to submit Issues and Pull Requests! | 欢迎提交Issue和Pull Request！

1. Fork the project | Fork项目
2. Create feature branch | 创建功能分支
3. Commit changes | 提交更改
4. Create Pull Request | 创建Pull Request

## 📄 License | 许可证

This project is open source under [MIT License](LICENSE). | 本项目基于 [MIT License](LICENSE) 开源协议。

## 🙏 Acknowledgments | 致谢

- [ESP-IDF](https://github.com/espressif/esp-idf) - Espressif official framework | Espressif官方框架
- [M5Unified](https://github.com/m5stack/M5Unified) - M5Stack official library reference | M5Stack官方库参考
- [M5StickC Plus](https://docs.m5stack.com/en/core/m5stickc_plus) - Official documentation | 官方文档

---

**⚡ Unleash the full potential of your M5StickC Plus! | 让您的M5StickC Plus发挥最大潜能！**
