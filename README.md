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

### 🔧 Development Environment

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

# Flash and monitor
idf.py -p /dev/cu.usbserial-[YOUR_PORT] flash monitor
```

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

# 烧录并监控
idf.py -p /dev/cu.usbserial-[YOUR_PORT] flash monitor
```

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

# ESPNOW Example

(See the README.md file in the upper level 'examples' directory for more information about examples.)

This example shows how to use ESPNOW of wifi. Example does the following steps:

* Start WiFi.
* Initialize ESPNOW.
* Register ESPNOW sending or receiving callback function.
* Add ESPNOW peer information.
* Send and receive ESPNOW data.

This example need at least two ESP devices:

* In order to get the MAC address of the other device, Device1 firstly send broadcast ESPNOW data with 'state' set as 0.
* When Device2 receiving broadcast ESPNOW data from Device1 with 'state' as 0, adds Device1 into the peer list.
  Then start sending broadcast ESPNOW data with 'state' set as 1.
* When Device1 receiving broadcast ESPNOW data with 'state' as 1, compares the local magic number with that in the data.
  If the local one is bigger than that one, stop sending broadcast ESPNOW data and starts sending unicast ESPNOW data to Device2.
* If Device2 receives unicast ESPNOW data, also stop sending broadcast ESPNOW data.

In practice, if the MAC address of the other device is known, it's not required to send/receive broadcast ESPNOW data first,
just add the device into the peer list and send/receive unicast ESPNOW data.

There are a lot of "extras" on top of ESPNOW data, such as type, state, sequence number, CRC and magic in this example. These "extras" are
not required to use ESPNOW. They are only used to make this example to run correctly. However, it is recommended that users add some "extras"
to make ESPNOW data more safe and more reliable.

## How to use example

### Configure the project

```
idf.py menuconfig
```

* Set WiFi mode (station or SoftAP) under Example Configuration Options.
* Set ESPNOW primary master key under Example Configuration Options.
  This parameter must be set to the same value for sending and recving devices.
* Set ESPNOW local master key under Example Configuration Options.
  This parameter must be set to the same value for sending and recving devices.
* Set Channel under Example Configuration Options.
  The sending device and the recving device must be on the same channel.
* Set Send count and Send delay under Example Configuration Options.
* Set Send len under Example Configuration Options.
* Set Enable Long Range Options.
  When this parameter is enabled, the ESP32 device will send data at the PHY rate of 512Kbps or 256Kbps
  then the data can be transmitted over long range between two ESP32 devices.

### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output:

```
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the Getting Started Guide for full steps to configure and use ESP-IDF to build projects.

## Example Output

Here is the example of ESPNOW receiving device console output.

```
I (898) phy: phy_version: 3960, 5211945, Jul 18 2018, 10:40:07, 0, 0
I (898) wifi: mode : sta (30:ae:a4:80:45:68)
I (898) espnow_example: WiFi started
I (898) ESPNOW: espnow [version: 1.0] init
I (5908) espnow_example: Start sending broadcast data
I (6908) espnow_example: send data to ff:ff:ff:ff:ff:ff
I (7908) espnow_example: send data to ff:ff:ff:ff:ff:ff
I (52138) espnow_example: send data to ff:ff:ff:ff:ff:ff
I (52138) espnow_example: Receive 0th broadcast data from: 30:ae:a4:0c:34:ec, len: 200
I (53158) espnow_example: send data to ff:ff:ff:ff:ff:ff
I (53158) espnow_example: Receive 1th broadcast data from: 30:ae:a4:0c:34:ec, len: 200
I (54168) espnow_example: send data to ff:ff:ff:ff:ff:ff
I (54168) espnow_example: Receive 2th broadcast data from: 30:ae:a4:0c:34:ec, len: 200
I (54168) espnow_example: Receive 0th unicast data from: 30:ae:a4:0c:34:ec, len: 200
I (54678) espnow_example: Receive 1th unicast data from: 30:ae:a4:0c:34:ec, len: 200
I (55668) espnow_example: Receive 2th unicast data from: 30:ae:a4:0c:34:ec, len: 200
```

Here is the example of ESPNOW sending device console output.

```
I (915) phy: phy_version: 3960, 5211945, Jul 18 2018, 10:40:07, 0, 0
I (915) wifi: mode : sta (30:ae:a4:0c:34:ec)
I (915) espnow_example: WiFi started
I (915) ESPNOW: espnow [version: 1.0] init
I (5915) espnow_example: Start sending broadcast data
I (5915) espnow_example: Receive 41th broadcast data from: 30:ae:a4:80:45:68, len: 200
I (5915) espnow_example: Receive 42th broadcast data from: 30:ae:a4:80:45:68, len: 200
I (5925) espnow_example: Receive 44th broadcast data from: 30:ae:a4:80:45:68, len: 200
I (5935) espnow_example: Receive 45th broadcast data from: 30:ae:a4:80:45:68, len: 200
I (6965) espnow_example: send data to ff:ff:ff:ff:ff:ff
I (6965) espnow_example: Receive 46th broadcast data from: 30:ae:a4:80:45:68, len: 200
I (7975) espnow_example: send data to ff:ff:ff:ff:ff:ff
I (7975) espnow_example: Receive 47th broadcast data from: 30:ae:a4:80:45:68, len: 200
I (7975) espnow_example: Start sending unicast data
I (7975) espnow_example: send data to 30:ae:a4:80:45:68
I (9015) espnow_example: send data to 30:ae:a4:80:45:68
I (9015) espnow_example: Receive 48th broadcast data from: 30:ae:a4:80:45:68, len: 200
I (10015) espnow_example: send data to 30:ae:a4:80:45:68
I (16075) espnow_example: send data to 30:ae:a4:80:45:68
I (17075) espnow_example: send data to 30:ae:a4:80:45:68
I (24125) espnow_example: send data to 30:ae:a4:80:45:68
```

## Troubleshooting

If ESPNOW data can not be received from another device, maybe the two devices are not
on the same channel or the primary key and local key are different.

In real application, if the receiving device is in station mode only and it connects to an AP,
modem sleep should be disabled. Otherwise, it may fail to revceive ESPNOW data from other devices.
