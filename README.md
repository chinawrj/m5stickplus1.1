# M5StickC Plus 1.1 ESP-IDF Project

🚀 **使用ESP-IDF完全启用M5StickC Plus 1.1所有硬件设备**

## 📋 项目概述

本项目基于ESP-IDF 5.5.1框架，为M5StickC Plus 1.1开发板提供完整的硬件驱动支持。重点实现了安全可靠的AXP192电源管理芯片驱动，确保所有外设设备正常工作。

## 🎯 项目目标

- ✅ **电源管理**: 安全控制AXP192电源管理芯片
- ✅ **屏幕支持**: TFT显示屏和背光控制
- ✅ **音频支持**: 麦克风模块电源管理
- ✅ **扩展接口**: 5V GROVE端口控制
- ✅ **电池监控**: 电压、电流、电量、充电状态监测
- ✅ **IMU支持**: MPU6886九轴传感器(I2C)
- 🎯 **安全设计**: 防止误操作损坏硬件

## 🛠️ 硬件规格

### M5StickC Plus 1.1 核心参数
- **主控**: ESP32-PICO-D4 (双核Xtensa LX6, 240MHz)
- **内存**: 4MB Flash + 320KB RAM
- **屏幕**: 1.14英寸 TFT LCD (135×240)
- **电池**: 120mAh锂电池
- **电源管理**: AXP192
- **IMU**: MPU6886 (加速度计+陀螺仪)
- **接口**: USB-C, GROVE(I2C), GPIO

### 电源管理芯片 AXP192
| 电源通道 | 电压 | 用途 |
|---------|------|------|
| LDO3 | 3.0V | TFT显示IC |
| LDO2 | 3.3V | TFT背光 |
| LDO0 | 3.3V | 麦克风 |
| DCDC1 | 3.3V | ESP32主控 |
| EXTEN | 5.0V | GROVE端口 |

## 🔧 开发环境

### 必需软件
- **ESP-IDF**: v5.5.1+
- **Python**: 3.8+
- **CMake**: 3.16+
- **工具链**: xtensa-esp32-elf-gcc

### 安装ESP-IDF
```bash
# 下载ESP-IDF
git clone -b v5.5.1 --recursive https://github.com/espressif/esp-idf.git

# 安装依赖
cd esp-idf
./install.sh

# 设置环境变量
. ./export.sh
```

## 🚀 快速开始

### 1. 克隆项目
```bash
git clone https://github.com/your-username/m5stickplus1.1-esp-idf.git
cd m5stickplus1.1-esp-idf
```

### 2. 配置项目
```bash
# 设置目标芯片
idf.py set-target esp32

# 配置项目(可选)
idf.py menuconfig
```

### 3. 编译项目
```bash
idf.py build
```

### 4. 烧录程序
```bash
# 查找USB端口
ls /dev/cu.usbserial-*

# 烧录并监控
idf.py -p /dev/cu.usbserial-[YOUR_PORT] flash monitor
```

## 📚 API使用指南

### 🛡️ 安全电源管理API (推荐)

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

### 📊 电池监控示例

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

## 🔒 安全特性

### 硬件保护设计
1. **固定电压值**: 所有安全API使用官方推荐的固定电压
2. **函数隐藏**: 危险的直接电压控制函数设为`static`，外部无法访问
3. **状态验证**: 所有操作都包含错误检查和状态验证
4. **官方兼容**: 基于M5Unified官方库的最佳实践

### 为什么选择安全API？
❌ **危险**: 直接设置电压可能损坏硬件
```c
axp192_set_ldo3_voltage(5000);  // 可能烧毁屏幕！
```

✅ **安全**: 使用预设的正确电压
```c
axp192_power_tft_display(true); // 自动使用3.0V
```

## 📁 项目结构

```
m5stickplus1.1/
├── main/
│   ├── CMakeLists.txt           # 主程序构建配置
│   ├── espnow_example_main.c    # 主程序和演示代码
│   ├── espnow_example.h         # 项目头文件
│   ├── axp192.h                 # AXP192驱动头文件
│   ├── axp192.c                 # AXP192驱动实现
│   └── Kconfig.projbuild        # 项目配置
├── CMakeLists.txt               # 顶层构建配置
├── sdkconfig                    # ESP-IDF配置
├── README.md                    # 项目说明
└── SAFETY_API_SUMMARY.md       # 安全API详细说明
```

## 🔍 功能演示

当前示例程序展示：
- ✅ AXP192初始化和配置
- ✅ 所有硬件模块安全启用
- ✅ 完整的电池状态监控
- ✅ 温度监测
- ✅ VBUS状态检测
- ✅ 省电模式控制

## 🛠️ 编译优化

### ESP-IDF组件优化
项目已优化，仅包含必需组件：
- `driver` - GPIO和I2C驱动
- `esp_system` - 系统核心
- `freertos` - 实时操作系统
- `log` - 日志系统
- `nvs_flash` - 配置存储

### 内存使用
- **Flash使用**: ~757KB (74%分区使用率)
- **RAM使用**: 优化后显著减少
- **组件**: 精简至核心功能

## 🔧 高级配置

### I2C配置
```c
#define I2C_MASTER_SCL_IO       22      // GPIO22
#define I2C_MASTER_SDA_IO       21      // GPIO21
#define I2C_MASTER_FREQ_HZ      100000  // 100kHz
```

### AXP192设备地址
```c
#define AXP192_I2C_ADDR         0x34    // 7位地址
```

## ⚠️ 注意事项

1. **电压设置**: 请只使用安全API，避免直接设置电压
2. **I2C冲突**: 确保MPU6886和AXP192不会产生地址冲突
3. **电源顺序**: 建议按顺序启用：ESP32 → 屏幕 → 其他外设
4. **电池保护**: 低电量时自动进入省电模式

## 🐛 故障排除

### 常见问题

**Q: 编译失败？**
A: 确保ESP-IDF版本≥5.5.1，并正确设置环境变量

**Q: AXP192通信失败？**
A: 检查I2C引脚连接，确认设备地址0x34

**Q: 屏幕不亮？**
A: 确保同时启用display和backlight：
```c
axp192_power_tft_display(true);
axp192_power_tft_backlight(true);
```

**Q: 电池电量显示异常？**
A: AXP192需要时间校准，等待几分钟后重新读取

## 📈 后续开发计划

- [ ] MPU6886 IMU驱动实现
- [ ] TFT屏幕驱动库集成
- [ ] WiFi功能演示
- [ ] 低功耗模式优化
- [ ] OTA升级支持
- [ ] 示例应用扩展

## 🤝 贡献指南

欢迎提交Issue和Pull Request！

1. Fork项目
2. 创建功能分支
3. 提交更改
4. 创建Pull Request

## 📄 许可证

本项目基于 [MIT License](LICENSE) 开源协议。

## 🙏 致谢

- [ESP-IDF](https://github.com/espressif/esp-idf) - Espressif官方框架
- [M5Unified](https://github.com/m5stack/M5Unified) - M5Stack官方库参考
- [M5StickC Plus](https://docs.m5stack.com/en/core/m5stickc_plus) - 官方文档

---

**⚡ 让您的M5StickC Plus发挥最大潜能！**

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
