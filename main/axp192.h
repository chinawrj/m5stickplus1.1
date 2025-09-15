#ifndef AXP192_H
#define AXP192_H

#include <stdbool.h>
#include "esp_err.h"
#include "driver/i2c_master.h"
#include "driver/i2c_types.h"

// AXP192 I2C地址
#define AXP192_I2C_ADDR         0x34

// I2C配置
#define I2C_MASTER_NUM          I2C_NUM_0
#define I2C_MASTER_SCL_IO       22      // GPIO22
#define I2C_MASTER_SDA_IO       21      // GPIO21
#define I2C_MASTER_FREQ_HZ      100000  // 100kHz
#define I2C_MASTER_TX_BUF_DISABLE 0
#define I2C_MASTER_RX_BUF_DISABLE 0
#define I2C_MASTER_TIMEOUT_MS   1000

// AXP192寄存器地址
#define AXP192_POWER_STATUS     0x00    // 电源状态
#define AXP192_CHARGE_STATUS    0x01    // 充电状态
#define AXP192_OTG_VBUS_STATUS  0x04    // OTG/VBUS状态
#define AXP192_DATA_BUFFER0     0x06    // 数据缓冲区0
#define AXP192_DATA_BUFFER1     0x07    // 数据缓冲区1
#define AXP192_DATA_BUFFER2     0x08    // 数据缓冲区2
#define AXP192_DATA_BUFFER3     0x09    // 数据缓冲区3
#define AXP192_DATA_BUFFER4     0x0A    // 数据缓冲区4
#define AXP192_DATA_BUFFER5     0x0B    // 数据缓冲区5

// 电源输出控制
#define AXP192_EXTEN_DC2_CTL    0x10    // EXTEN & DC-DC2 控制
#define AXP192_DC1_DC3_CTL      0x12    // DC-DC1 & DC-DC3 控制
#define AXP192_LDO23_DC123_CTL  0x12    // LDO2/3 & DC-DC1/2/3 控制

// 电压设置
#define AXP192_DC2_VOLTAGE      0x23    // DC-DC2 输出电压设置
#define AXP192_DC1_VOLTAGE      0x26    // DC-DC1 输出电压设置
#define AXP192_DC3_VOLTAGE      0x27    // DC-DC3 输出电压设置
#define AXP192_LDO23_VOLTAGE    0x28    // LDO2/3 输出电压设置

// 充电控制
#define AXP192_CHARGE_CTL1      0x33    // 充电控制1
#define AXP192_CHARGE_CTL2      0x34    // 充电控制2

// 电池容量
#define AXP192_BAT_PERCEN_CAL   0xB9    // 电池电量百分比

// ADC控制
#define AXP192_ADC_EN1          0x82    // ADC使能设置1
#define AXP192_ADC_EN2          0x83    // ADC使能设置2

// 电池电压ADC (高8位和低4位)
#define AXP192_BAT_AVERVOL_H8   0x78    // 电池电压高8位
#define AXP192_BAT_AVERVOL_L4   0x79    // 电池电压低4位

// 电池电流ADC (高8位和低5位)
#define AXP192_BAT_AVERCHGCUR_H8  0x7A  // 电池充电电流高8位
#define AXP192_BAT_AVERCHGCUR_L5  0x7B  // 电池充电电流低5位
#define AXP192_BAT_AVERDISCHGCUR_H8 0x7C // 电池放电电流高8位
#define AXP192_BAT_AVERDISCHGCUR_L5 0x7D // 电池放电电流低5位

// 函数声明
esp_err_t axp192_init(void);
esp_err_t axp192_write_byte(uint8_t reg_addr, uint8_t data);
esp_err_t axp192_read_byte(uint8_t reg_addr, uint8_t *data);
esp_err_t axp192_read_bytes(uint8_t reg_addr, uint8_t *data, size_t len);

// 电源管理功能
esp_err_t axp192_set_dc_voltage(uint8_t dcdc, uint16_t voltage_mv);
esp_err_t axp192_set_ldo_voltage(uint8_t ldo, uint16_t voltage_mv);
esp_err_t axp192_power_on(uint8_t channel);
esp_err_t axp192_power_off(uint8_t channel);

// 电池相关功能
esp_err_t axp192_get_battery_voltage(float *voltage);
esp_err_t axp192_get_battery_current(float *current);
esp_err_t axp192_get_battery_power(float *power);
esp_err_t axp192_get_battery_level(uint8_t *level);

// 充电相关功能
esp_err_t axp192_set_charge_current(uint16_t current_ma);
esp_err_t axp192_enable_charge(bool enable);

// 官方实现的高级功能
esp_err_t axp192_get_battery_charge_current(float *charge_current);
esp_err_t axp192_get_battery_discharge_current(float *discharge_current);
bool axp192_is_charging(void);
esp_err_t axp192_get_internal_temperature(float *temperature);
esp_err_t axp192_get_vbus_voltage(float *voltage);
esp_err_t axp192_get_vbus_current(float *current);
bool axp192_is_vbus_present(void);
bool axp192_is_battery_present(void);

// 电源通道定义
#define AXP192_DCDC1    0x01
#define AXP192_DCDC2    0x02
#define AXP192_DCDC3    0x04
#define AXP192_LDO2     0x08
#define AXP192_LDO3     0x10
#define AXP192_EXTEN    0x40

// M5StickC Plus硬件电源通道映射 (基于官方文档)
// Microphone   : LDOio0
// RTC          : LDO1 (Always On)
// TFT Backlight: LDO2
// TFT IC       : LDO3
// ESP32/3.3V   : DC-DC1
// MPU6886      : DC-DC1 (共享)
// 5V GROVE     : IPSOUT

// ====================================================================
// 🛡️ M5StickC Plus安全电源管理API (推荐使用)
// 基于官方M5Unified最佳实践，固定电压值防止硬件损坏
// ====================================================================

// M5StickC Plus硬件电源管理 - 高级安全API
esp_err_t axp192_power_tft_display(bool enable);      // 屏幕显示模块 (LDO3=3.0V固定)
esp_err_t axp192_power_tft_backlight(bool enable);    // 屏幕背光 (LDO2=3.3V固定)
esp_err_t axp192_power_microphone(bool enable);       // 麦克风模块 (LDO0=3.3V固定)
esp_err_t axp192_power_grove_5v(bool enable);         // 5V GROVE端口 (EXTEN)

// 系统电源管理
esp_err_t axp192_power_esp32(bool enable);            // ESP32主控 (DCDC1=3.3V固定)

// 状态查询API
bool axp192_get_tft_display_status(void);             // 获取屏幕电源状态
bool axp192_get_tft_backlight_status(void);           // 获取背光电源状态
bool axp192_get_microphone_status(void);              // 获取麦克风电源状态
bool axp192_get_grove_5v_status(void);                // 获取5V输出状态

// ====================================================================
// ⚠️ 注意：直接电压控制API已隐藏为内部函数，仅在axp192.c内部使用
// 这样可以防止外部代码意外调用危险的电压设置函数
// ====================================================================

// 5V GROVE输出控制 (保留公开，相对安全)
esp_err_t axp192_enable_exten(bool enable);          // 5V GROVE输出控制
bool axp192_get_exten_status(void);                  // 获取5V输出状态

// GPIO控制 (基于官方API)
esp_err_t axp192_set_gpio0(bool state);              // GPIO0控制
esp_err_t axp192_set_gpio1(bool state);              // GPIO1控制
esp_err_t axp192_set_gpio2(bool state);              // GPIO2控制
esp_err_t axp192_set_gpio3(bool state);              // GPIO3控制
esp_err_t axp192_set_gpio4(bool state);              // GPIO4控制

#endif // AXP192_H