#include "axp192.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "AXP192";
static i2c_master_bus_handle_t i2c_bus_handle = NULL;
static i2c_master_dev_handle_t axp192_dev_handle = NULL;

/**
 * @brief Initialize I2C master device
 */
static esp_err_t i2c_master_init(void)
{
    // Configure I2C bus
    i2c_master_bus_config_t i2c_bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_MASTER_NUM,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    
    esp_err_t ret = i2c_new_master_bus(&i2c_bus_config, &i2c_bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C master bus creation failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Configure AXP192 device
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = AXP192_I2C_ADDR,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };

    ret = i2c_master_bus_add_device(i2c_bus_handle, &dev_cfg, &axp192_dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C device add failed: %s", esp_err_to_name(ret));
        i2c_del_master_bus(i2c_bus_handle);
        return ret;
    }

    return ESP_OK;
}

/**
 * @brief Write one byte to AXP192
 */
esp_err_t axp192_write_byte(uint8_t reg_addr, uint8_t data)
{
    uint8_t write_buf[2] = {reg_addr, data};
    esp_err_t ret = i2c_master_transmit(axp192_dev_handle, write_buf, sizeof(write_buf), I2C_MASTER_TIMEOUT_MS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C write failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

/**
 * @brief 从AXP192读取一个字节
 */
esp_err_t axp192_read_byte(uint8_t reg_addr, uint8_t *data)
{
    return axp192_read_bytes(reg_addr, data, 1);
}

/**
 * @brief 从AXP192读取多个字节
 */
esp_err_t axp192_read_bytes(uint8_t reg_addr, uint8_t *data, size_t len)
{
    esp_err_t ret = i2c_master_transmit_receive(axp192_dev_handle, &reg_addr, 1, data, len, I2C_MASTER_TIMEOUT_MS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C read failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

/**
 * @brief 初始化AXP192
 */
esp_err_t axp192_init(void)
{
    esp_err_t ret;
    
    // 初始化I2C
    ret = i2c_master_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C master initialization failed");
        return ret;
    }
    
    // 检查AXP192是否响应
    uint8_t data;
    ret = axp192_read_byte(AXP192_POWER_STATUS, &data);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to communicate with AXP192");
        return ret;
    }
    
    ESP_LOGI(TAG, "AXP192 detected, power status: 0x%02X", data);
    
    // 使能ADC功能
    ret = axp192_write_byte(AXP192_ADC_EN1, 0xFF);  // 使能所有ADC
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable ADC");
        return ret;
    }
    
    ret = axp192_write_byte(AXP192_ADC_EN2, 0xFF);  // 使能所有ADC
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable ADC2");
        return ret;
    }
    
    // 设置充电参数 (可选)
    ret = axp192_write_byte(AXP192_CHARGE_CTL1, 0xC0);  // 使能充电，4.2V目标电压
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set charge control");
        return ret;
    }
    
    // 默认开启LCD和BUZZER电源 (开机即可用)
    ESP_LOGI(TAG, "Enabling default power channels for LCD and BUZZER...");
    
    // 开启TFT显示屏电源 (LDO3=3.0V)
    ret = axp192_power_tft_display(true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable TFT display power");
        return ret;
    }
    
    // 开启TFT背光电源 (LDO2=3.3V)
    ret = axp192_power_tft_backlight(true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable TFT backlight power");
        return ret;
    }
    
    // 开启5V GROVE电源 (EXTEN) - BUZZER必需
    ret = axp192_power_grove_5v(true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable 5V GROVE power for BUZZER");
        return ret;
    }

    // Enable LDO0 for Microphone (3.3V)
    ret = axp192_power_microphone(true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable Microphone power");
        return ret;
    }

    // 开启DC-DC1供电ESP32核心 (3.3V)
    ret = axp192_power_esp32(true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable ESP32 power");
        return ret;
    }
    
    ESP_LOGI(TAG, "AXP192 initialized successfully with LCD and BUZZER power enabled");
    return ESP_OK;
}

/**
 * @brief 获取电池电压 (单位：V)
 */
esp_err_t axp192_get_battery_voltage(float *voltage)
{
    uint8_t data[2];
    esp_err_t ret = axp192_read_bytes(AXP192_BAT_AVERVOL_H8, data, 2);
    if (ret != ESP_OK) {
        return ret;
    }
    
    uint16_t vol = ((data[0] << 4) | (data[1] & 0x0F));  // 12位ADC值
    *voltage = vol * 1.1 / 1000.0;  // 转换为电压值 (V)
    
    return ESP_OK;
}

/**
 * @brief 获取电池电流 (单位：mA)
 */
esp_err_t axp192_get_battery_current(float *current)
{
    uint8_t data[2];
    esp_err_t ret;
    
    // 读取充电电流
    ret = axp192_read_bytes(AXP192_BAT_AVERCHGCUR_H8, data, 2);
    if (ret != ESP_OK) {
        return ret;
    }
    
    uint16_t charge_current = ((data[0] << 5) | (data[1] & 0x1F));  // 13位ADC值
    float charge_ma = charge_current * 0.5;  // 0.5mA/LSB
    
    // 读取放电电流
    ret = axp192_read_bytes(AXP192_BAT_AVERDISCHGCUR_H8, data, 2);
    if (ret != ESP_OK) {
        return ret;
    }
    
    uint16_t discharge_current = ((data[0] << 5) | (data[1] & 0x1F));  // 13位ADC值
    float discharge_ma = discharge_current * 0.5;  // 0.5mA/LSB
    
    // 净电流 = 充电电流 - 放电电流
    *current = charge_ma - discharge_ma;
    
    return ESP_OK;
}

/**
 * @brief 获取电池功率 (单位：mW)
 */
esp_err_t axp192_get_battery_power(float *power)
{
    float voltage, current;
    esp_err_t ret;
    
    ret = axp192_get_battery_voltage(&voltage);
    if (ret != ESP_OK) {
        return ret;
    }
    
    ret = axp192_get_battery_current(&current);
    if (ret != ESP_OK) {
        return ret;
    }
    
    *power = voltage * current;  // P = V * I
    
    return ESP_OK;
}

/**
 * @brief 获取电池电量百分比
 */
esp_err_t axp192_get_battery_level(uint8_t *level)
{
    esp_err_t ret = axp192_read_byte(AXP192_BAT_PERCEN_CAL, level);
    if (ret != ESP_OK) {
        return ret;
    }
    
    if (*level > 100) {
        *level = 100;  // 限制在100%以内
    }
    
    return ESP_OK;
}

/**
 * @brief 设置充电电流
 */
esp_err_t axp192_set_charge_current(uint16_t current_ma)
{
    uint8_t data;
    esp_err_t ret = axp192_read_byte(AXP192_CHARGE_CTL1, &data);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // 清除电流设置位 [3:0]
    data &= 0xF0;
    
    // 设置充电电流 (100mA到1320mA，步长80mA)
    if (current_ma < 100) current_ma = 100;
    if (current_ma > 1320) current_ma = 1320;
    
    uint8_t current_level = (current_ma - 100) / 80;
    data |= (current_level & 0x0F);
    
    return axp192_write_byte(AXP192_CHARGE_CTL1, data);
}

/**
 * @brief 使能/禁用充电
 */
esp_err_t axp192_enable_charge(bool enable)
{
    uint8_t data;
    esp_err_t ret = axp192_read_byte(AXP192_CHARGE_CTL1, &data);
    if (ret != ESP_OK) {
        return ret;
    }
    
    if (enable) {
        data |= 0x80;  // 设置bit7使能充电
    } else {
        data &= 0x7F;  // 清除bit7禁用充电
    }
    
    return axp192_write_byte(AXP192_CHARGE_CTL1, data);
}

/**
 * @brief 电源通道控制
 */
esp_err_t axp192_power_on(uint8_t channel)
{
    uint8_t data;
    esp_err_t ret = axp192_read_byte(AXP192_LDO23_DC123_CTL, &data);
    if (ret != ESP_OK) {
        return ret;
    }
    
    data |= channel;
    return axp192_write_byte(AXP192_LDO23_DC123_CTL, data);
}

/**
 * @brief 获取电池充电电流 (单位：mA)
 */
esp_err_t axp192_get_battery_charge_current(float *charge_current)
{
    uint8_t data[2];
    esp_err_t ret = axp192_read_bytes(AXP192_BAT_AVERCHGCUR_H8, data, 2);
    if (ret != ESP_OK) {
        return ret;
    }
    
    uint16_t raw_current = ((data[0] << 5) | (data[1] & 0x1F));  // 13位ADC值
    *charge_current = raw_current * 0.5;  // 0.5mA/LSB (官方精度)
    
    return ESP_OK;
}

/**
 * @brief 获取电池放电电流 (单位：mA)
 */
esp_err_t axp192_get_battery_discharge_current(float *discharge_current)
{
    uint8_t data[2];
    esp_err_t ret = axp192_read_bytes(AXP192_BAT_AVERDISCHGCUR_H8, data, 2);
    if (ret != ESP_OK) {
        return ret;
    }
    
    uint16_t raw_current = ((data[0] << 5) | (data[1] & 0x1F));  // 13位ADC值
    *discharge_current = raw_current * 0.5;  // 0.5mA/LSB (官方精度)
    
    return ESP_OK;
}

/**
 * @brief 检查是否正在充电 (官方实现)
 */
bool axp192_is_charging(void)
{
    uint8_t status;
    esp_err_t ret = axp192_read_byte(AXP192_CHARGE_STATUS, &status);  // 0x01 充电状态寄存器
    if (ret != ESP_OK) {
        return false;
    }
    // bit6: 正在充电状态指示 (1=充电中, 0=未充电)
    return (status & 0x40) != 0;
}

/**
 * @brief 获取内部温度 (单位：°C, 官方算法)
 */
esp_err_t axp192_get_internal_temperature(float *temperature)
{
    uint8_t data[2];
    esp_err_t ret = axp192_read_bytes(0x5E, data, 2);  // 内部温度寄存器
    if (ret != ESP_OK) {
        return ret;
    }
    
    uint16_t raw_temp = ((data[0] << 4) | (data[1] & 0x0F));  // 12位ADC值
    *temperature = raw_temp * 0.1 - 144.7;  // 官方温度转换公式
    
    return ESP_OK;
}

/**
 * @brief 获取VBUS电压 (单位：V)
 */
esp_err_t axp192_get_vbus_voltage(float *voltage)
{
    uint8_t data[2];
    esp_err_t ret = axp192_read_bytes(0x5A, data, 2);  // VBUS电压寄存器
    if (ret != ESP_OK) {
        return ret;
    }
    
    uint16_t raw_voltage = ((data[0] << 4) | (data[1] & 0x0F));  // 12位ADC值
    *voltage = raw_voltage * (1.7 / 1000.0);  // 官方VBUS电压转换
    
    return ESP_OK;
}

/**
 * @brief 获取VBUS电流 (单位：mA)
 */
esp_err_t axp192_get_vbus_current(float *current)
{
    uint8_t data[2];
    esp_err_t ret = axp192_read_bytes(0x5C, data, 2);  // VBUS电流寄存器
    if (ret != ESP_OK) {
        return ret;
    }
    
    uint16_t raw_current = ((data[0] << 4) | (data[1] & 0x0F));  // 12位ADC值
    *current = raw_current * 0.375;  // 官方VBUS电流转换 (0.375mA/LSB)
    
    return ESP_OK;
}

/**
 * @brief 检查VBUS是否存在 (官方实现)
 */
bool axp192_is_vbus_present(void)
{
    uint8_t status;
    esp_err_t ret = axp192_read_byte(AXP192_POWER_STATUS, &status);
    if (ret != ESP_OK) {
        return false;
    }
    return (status & 0x20) != 0;  // bit5: VBUS存在指示
}

/**
 * @brief 检查电池是否存在 (官方实现)
 */
bool axp192_is_battery_present(void)
{
    uint8_t status;
    esp_err_t ret = axp192_read_byte(0x01, &status);  // 状态寄存器
    if (ret != ESP_OK) {
        return false;
    }
    return (status & 0x20) != 0;  // bit5: 电池存在指示
}

// ========================= M5StickC Plus电源管理API =========================
// 基于M5Unified官方实现的完整电源控制功能

/**
 * @brief 设置DCDC1电压 (ESP32 VDD) - 内部函数
 * @param voltage_mv 电压值(mV), 范围: 700-3500mV, 步进25mV
 */
static esp_err_t axp192_set_dcdc1_voltage(int voltage_mv)
{
    voltage_mv -= 700;
    uint8_t val = (voltage_mv < 0) ? 0 : (voltage_mv / 25);
    if (val > 0x7F) val = 0x7F;  // 最大值限制
    
    esp_err_t ret = axp192_write_byte(0x26, val);  // DCDC1电压寄存器
    if (ret != ESP_OK) return ret;
    
    // 使能DCDC1 (bit0 of reg 0x12)
    if (voltage_mv >= 0) {
        uint8_t ctrl;
        ret = axp192_read_byte(0x12, &ctrl);
        if (ret == ESP_OK) {
            ret = axp192_write_byte(0x12, ctrl | 0x01);
        }
    }
    return ret;
}

/**
 * @brief 设置DCDC2电压 (未使用) - 内部函数
 * @param voltage_mv 电压值(mV), 范围: 700-2275mV, 步进25mV
 */
static esp_err_t axp192_set_dcdc2_voltage(int voltage_mv)
{
    voltage_mv -= 700;
    uint8_t val = (voltage_mv < 0) ? 0 : (voltage_mv / 25);
    if (val > 0x3F) val = 0x3F;  // DCDC2最大值
    
    esp_err_t ret = axp192_write_byte(0x23, val);  // DCDC2电压寄存器
    if (ret != ESP_OK) return ret;
    
    // 使能DCDC2 (bit4 of reg 0x12)
    if (voltage_mv >= 0) {
        uint8_t ctrl;
        ret = axp192_read_byte(0x12, &ctrl);
        if (ret == ESP_OK) {
            ret = axp192_write_byte(0x12, ctrl | 0x10);
        }
    }
    return ret;
}

/**
 * @brief 设置DCDC3电压 (LCD背光) - 内部函数
 * @param voltage_mv 电压值(mV), 范围: 700-3500mV, 步进25mV
 */
static esp_err_t axp192_set_dcdc3_voltage(int voltage_mv)
{
    voltage_mv -= 700;
    uint8_t val = (voltage_mv < 0) ? 0 : (voltage_mv / 25);
    if (val > 0x7F) val = 0x7F;
    
    esp_err_t ret = axp192_write_byte(0x27, val);  // DCDC3电压寄存器
    if (ret != ESP_OK) return ret;
    
    // 使能DCDC3 (bit1 of reg 0x12)
    if (voltage_mv >= 0) {
        uint8_t ctrl;
        ret = axp192_read_byte(0x12, &ctrl);
        if (ret == ESP_OK) {
            ret = axp192_write_byte(0x12, ctrl | 0x02);
        }
    }
    return ret;
}

/**
 * @brief 设置LDO0电压 (麦克风) - 内部函数
 * @param voltage_mv 电压值(mV), 范围: 1800-3300mV, 步进100mV
 */
static esp_err_t axp192_set_ldo0_voltage(int voltage_mv)
{
    voltage_mv -= 1800;
    uint8_t val = (voltage_mv < 0) ? 0 : (voltage_mv / 100);
    if (val > 0x0F) val = 0x0F;
    
    esp_err_t ret = axp192_write_byte(0x91, val);  // LDO0电压寄存器
    if (ret != ESP_OK) return ret;
    
    // LDO0控制 (reg 0x90: 0x02=LDO模式, 0x07=浮空)
    return axp192_write_byte(0x90, (voltage_mv >= 0) ? 0x02 : 0x07);
}

/**
 * @brief 设置LDO2电压 (TFT背光) - 内部函数
 * @param voltage_mv 电压值(mV), 范围: 1800-3300mV, 步进100mV
 */
static esp_err_t axp192_set_ldo2_voltage(int voltage_mv)
{
    voltage_mv -= 1800;
    uint8_t val = (voltage_mv < 0) ? 0 : (voltage_mv / 100);
    if (val > 0x0F) val = 0x0F;
    
    // LDO2电压设置 (reg 0x28高4位)
    uint8_t now;
    esp_err_t ret = axp192_read_byte(0x28, &now);
    if (ret != ESP_OK) return ret;
    
    now = (now & 0x0F) | (val << 4);
    ret = axp192_write_byte(0x28, now);
    if (ret != ESP_OK) return ret;
    
    // 使能LDO2 (bit2 of reg 0x12)
    uint8_t ctrl;
    ret = axp192_read_byte(0x12, &ctrl);
    if (ret == ESP_OK) {
        if (voltage_mv >= 0) {
            ret = axp192_write_byte(0x12, ctrl | 0x04);
        } else {
            ret = axp192_write_byte(0x12, ctrl & ~0x04);
        }
    }
    return ret;
}

/**
 * @brief 设置LDO3电压 (TFT IC) - 内部函数
 * @param voltage_mv 电压值(mV), 范围: 1800-3300mV, 步进100mV
 */
static esp_err_t axp192_set_ldo3_voltage(int voltage_mv)
{
    voltage_mv -= 1800;
    uint8_t val = (voltage_mv < 0) ? 0 : (voltage_mv / 100);
    if (val > 0x0F) val = 0x0F;
    
    // LDO3电压设置 (reg 0x28低4位)
    uint8_t now;
    esp_err_t ret = axp192_read_byte(0x28, &now);
    if (ret != ESP_OK) return ret;
    
    now = (now & 0xF0) | val;
    ret = axp192_write_byte(0x28, now);
    if (ret != ESP_OK) return ret;
    
    // 使能LDO3 (bit3 of reg 0x12)
    uint8_t ctrl;
    ret = axp192_read_byte(0x12, &ctrl);
    if (ret == ESP_OK) {
        if (voltage_mv >= 0) {
            ret = axp192_write_byte(0x12, ctrl | 0x08);
        } else {
            ret = axp192_write_byte(0x12, ctrl & ~0x08);
        }
    }
    return ret;
}

/**
 * @brief 使能/禁用DCDC1 (ESP32电源) - 内部函数
 */
static esp_err_t axp192_enable_dcdc1(bool enable)
{
    uint8_t ctrl;
    esp_err_t ret = axp192_read_byte(0x12, &ctrl);
    if (ret != ESP_OK) return ret;
    
    if (enable) {
        ctrl |= 0x01;
    } else {
        ctrl &= ~0x01;
    }
    return axp192_write_byte(0x12, ctrl);
}

/**
 * @brief 使能/禁用DCDC2 - 内部函数
 */
static esp_err_t axp192_enable_dcdc2(bool enable)
{
    uint8_t ctrl;
    esp_err_t ret = axp192_read_byte(0x12, &ctrl);
    if (ret != ESP_OK) return ret;
    
    if (enable) {
        ctrl |= 0x10;
    } else {
        ctrl &= ~0x10;
    }
    return axp192_write_byte(0x12, ctrl);
}

/**
 * @brief 使能/禁用DCDC3 (LCD背光) - 内部函数
 */
static esp_err_t axp192_enable_dcdc3(bool enable)
{
    uint8_t ctrl;
    esp_err_t ret = axp192_read_byte(0x12, &ctrl);
    if (ret != ESP_OK) return ret;
    
    if (enable) {
        ctrl |= 0x02;
    } else {
        ctrl &= ~0x02;
    }
    return axp192_write_byte(0x12, ctrl);
}

/**
 * @brief 使能/禁用LDO0 (麦克风) - 内部函数
 */
static esp_err_t axp192_enable_ldo0(bool enable)
{
    return axp192_write_byte(0x90, enable ? 0x02 : 0x07);
}

/**
 * @brief 使能/禁用LDO2 (TFT背光) - 内部函数
 */
static esp_err_t axp192_enable_ldo2(bool enable)
{
    uint8_t ctrl;
    esp_err_t ret = axp192_read_byte(0x12, &ctrl);
    if (ret != ESP_OK) return ret;
    
    if (enable) {
        ctrl |= 0x04;
    } else {
        ctrl &= ~0x04;
    }
    return axp192_write_byte(0x12, ctrl);
}

/**
 * @brief 使能/禁用LDO3 (TFT IC) - 内部函数
 */
static esp_err_t axp192_enable_ldo3(bool enable)
{
    uint8_t ctrl;
    esp_err_t ret = axp192_read_byte(0x12, &ctrl);
    if (ret != ESP_OK) return ret;
    
    if (enable) {
        ctrl |= 0x08;
    } else {
        ctrl &= ~0x08;
    }
    return axp192_write_byte(0x12, ctrl);
}

/**
 * @brief 使能/禁用EXTEN (5V GROVE输出)
 */
esp_err_t axp192_enable_exten(bool enable)
{
    uint8_t ctrl;
    esp_err_t ret = axp192_read_byte(0x12, &ctrl);
    if (ret != ESP_OK) return ret;
    
    if (enable) {
        ctrl |= 0x40;
    } else {
        ctrl &= ~0x40;
    }
    return axp192_write_byte(0x12, ctrl);
}

/**
 * @brief 获取EXTEN状态
 */
bool axp192_get_exten_status(void)
{
    uint8_t ctrl;
    esp_err_t ret = axp192_read_byte(0x12, &ctrl);
    if (ret != ESP_OK) return false;
    
    return (ctrl & 0x40) != 0;
}

/**
 * @brief 设置GPIO0状态 (官方实现)
 */
esp_err_t axp192_set_gpio0(bool state)
{
    return axp192_write_byte(0x90, state ? 0x06 : 0x05);  // 高电平或低电平
}

/**
 * @brief 设置GPIO1状态
 */
esp_err_t axp192_set_gpio1(bool state)
{
    return axp192_write_byte(0x92, state ? 0x06 : 0x05);
}

/**
 * @brief 设置GPIO2状态
 */
esp_err_t axp192_set_gpio2(bool state)
{
    return axp192_write_byte(0x93, state ? 0x06 : 0x05);
}

/**
 * @brief 设置GPIO3状态
 */
esp_err_t axp192_set_gpio3(bool state)
{
    uint8_t ctrl;
    esp_err_t ret = axp192_read_byte(0x96, &ctrl);
    if (ret != ESP_OK) return ret;
    
    if (state) {
        ctrl |= 0x01;
    } else {
        ctrl &= ~0x01;
    }
    ret = axp192_write_byte(0x96, ctrl);
    if (ret != ESP_OK) return ret;
    
    // 设置GPIO模式
    uint8_t mode;
    ret = axp192_read_byte(0x95, &mode);
    if (ret == ESP_OK) {
        mode = (mode & ~0x03) | 0x81;  // GPIO模式
        ret = axp192_write_byte(0x95, mode);
    }
    return ret;
}

/**
 * @brief 设置GPIO4状态
 */
esp_err_t axp192_set_gpio4(bool state)
{
    uint8_t ctrl;
    esp_err_t ret = axp192_read_byte(0x96, &ctrl);
    if (ret != ESP_OK) return ret;
    
    if (state) {
        ctrl |= 0x02;
    } else {
        ctrl &= ~0x02;
    }
    ret = axp192_write_byte(0x96, ctrl);
    if (ret != ESP_OK) return ret;
    
    // 设置GPIO模式
    uint8_t mode;
    ret = axp192_read_byte(0x95, &mode);
    if (ret == ESP_OK) {
        mode = (mode & ~0x0C) | 0x84;  // GPIO模式
        ret = axp192_write_byte(0x95, mode);
    }
    return ret;
}

// ====================================================================
// 🛡️ M5StickC Plus安全电源管理API实现
// 基于官方M5Unified固定电压值，防止硬件损坏
// ====================================================================

esp_err_t axp192_power_tft_display(bool enable)
{
    ESP_LOGI(TAG, "TFT Display: %s (LDO3=3.0V)", enable ? "ON" : "OFF");
    
    if (enable) {
        // 官方M5StickC Plus标准：LDO3=3.0V
        ESP_ERROR_CHECK(axp192_set_ldo3_voltage(3000));
        ESP_ERROR_CHECK(axp192_enable_ldo3(true));
    } else {
        ESP_ERROR_CHECK(axp192_enable_ldo3(false));
    }
    
    return ESP_OK;
}

esp_err_t axp192_power_tft_backlight(bool enable)
{
    ESP_LOGI(TAG, "TFT Backlight: %s (LDO2=3.3V)", enable ? "ON" : "OFF");
    
    if (enable) {
        // 官方M5StickC Plus标准：LDO2=3.3V
        ESP_ERROR_CHECK(axp192_set_ldo2_voltage(3300));
        ESP_ERROR_CHECK(axp192_enable_ldo2(true));
    } else {
        ESP_ERROR_CHECK(axp192_enable_ldo2(false));
    }
    
    return ESP_OK;
}

esp_err_t axp192_power_microphone(bool enable)
{
    ESP_LOGI(TAG, "Microphone: %s (LDO0=3.3V)", enable ? "ON" : "OFF");
    
    if (enable) {
        // 标准麦克风电压：LDO0=3.3V
        ESP_ERROR_CHECK(axp192_set_ldo0_voltage(3300));
        ESP_ERROR_CHECK(axp192_enable_ldo0(true));
    } else {
        ESP_ERROR_CHECK(axp192_enable_ldo0(false));
    }
    
    return ESP_OK;
}

esp_err_t axp192_power_grove_5v(bool enable)
{
    ESP_LOGI(TAG, "5V GROVE: %s (EXTEN)", enable ? "ON" : "OFF");
    return axp192_enable_exten(enable);
}

esp_err_t axp192_power_esp32(bool enable)
{
    ESP_LOGI(TAG, "ESP32 Main Power: %s (DCDC1=3.3V)", enable ? "ON" : "OFF");
    
    if (enable) {
        // ESP32标准电压：DCDC1=3.3V
        ESP_ERROR_CHECK(axp192_set_dcdc1_voltage(3300));
        ESP_ERROR_CHECK(axp192_enable_dcdc1(true));
    } else {
        // 注意：关闭ESP32主电源将导致系统重启！
        ESP_LOGW(TAG, "Warning: Disabling ESP32 power will cause system reset!");
        ESP_ERROR_CHECK(axp192_enable_dcdc1(false));
    }
    
    return ESP_OK;
}

bool axp192_get_tft_display_status(void)
{
    uint8_t reg_val;
    if (axp192_read_byte(0x12, &reg_val) == ESP_OK) {
        return (reg_val & 0x08) != 0;  // LDO3 enable bit
    }
    return false;
}

bool axp192_get_tft_backlight_status(void)
{
    uint8_t reg_val;
    if (axp192_read_byte(0x12, &reg_val) == ESP_OK) {
        return (reg_val & 0x04) != 0;  // LDO2 enable bit
    }
    return false;
}

bool axp192_get_microphone_status(void)
{
    uint8_t reg_val;
    if (axp192_read_byte(0x90, &reg_val) == ESP_OK) {
        return (reg_val & 0x07) == 0x02;  // LDO0 mode
    }
    return false;
}

bool axp192_get_grove_5v_status(void)
{
    return axp192_get_exten_status();
}