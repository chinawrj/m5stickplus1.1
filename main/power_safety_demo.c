/*
 * M5StickC Plus安全电源管理API示例
 * 基于官方M5Unified最佳实践，确保硬件安全
 */

#include "axp192.h"
#include "esp_log.h"

static const char *TAG = "POWER_DEMO";

void safe_power_management_demo(void)
{
    ESP_LOGI(TAG, "🛡️ M5StickC Plus安全电源管理演示");
    
    // ============================================
    // 推荐使用：安全API（固定电压，防止硬件损坏）
    // ============================================
    
    ESP_LOGI(TAG, "✅ 启用屏幕显示模块 (LDO3=3.0V)");
    if (axp192_power_tft_display(true) == ESP_OK) {
        ESP_LOGI(TAG, "   屏幕显示电源已开启");
    }
    
    ESP_LOGI(TAG, "✅ 启用屏幕背光 (LDO2=3.3V)");
    if (axp192_power_tft_backlight(true) == ESP_OK) {
        ESP_LOGI(TAG, "   屏幕背光已开启");
    }
    
    ESP_LOGI(TAG, "✅ 启用麦克风模块 (LDO0=3.3V)");
    if (axp192_power_microphone(true) == ESP_OK) {
        ESP_LOGI(TAG, "   麦克风电源已开启");
    }
    
    ESP_LOGI(TAG, "✅ 启用5V GROVE端口");
    if (axp192_power_grove_5v(true) == ESP_OK) {
        ESP_LOGI(TAG, "   5V输出已开启");
    }
    
    // 状态查询
    ESP_LOGI(TAG, "📊 电源状态:");
    ESP_LOGI(TAG, "   屏幕显示: %s", axp192_get_tft_display_status() ? "开启" : "关闭");
    ESP_LOGI(TAG, "   屏幕背光: %s", axp192_get_tft_backlight_status() ? "开启" : "关闭");
    ESP_LOGI(TAG, "   麦克风: %s", axp192_get_microphone_status() ? "开启" : "关闭");
    ESP_LOGI(TAG, "   5V输出: %s", axp192_get_grove_5v_status() ? "开启" : "关闭");
    
    // ============================================
    // ⚠️ 高级用户：直接电压控制API（有风险！）
    // ============================================
    
    ESP_LOGW(TAG, "⚠️ 以下为高级API，仅限专业用途！");
    ESP_LOGW(TAG, "   错误使用可能损坏硬件！");
    
    // 示例：如果需要特殊电压设置
    // ESP_LOGW(TAG, "🔧 设置特殊电压 - 仅限调试");
    // axp192_set_ldo3_voltage(2800);  // 警告：可能损坏屏幕！
    // axp192_enable_ldo3(true);
    
    ESP_LOGI(TAG, "🎯 建议：始终使用安全API而非直接电压控制");
}

void power_saving_demo(void)
{
    ESP_LOGI(TAG, "🔋 省电模式演示");
    
    // 关闭非必要功能节省电量
    ESP_LOGI(TAG, "💡 关闭屏幕背光节省电量");
    axp192_power_tft_backlight(false);
    
    ESP_LOGI(TAG, "🎤 关闭麦克风节省电量");
    axp192_power_microphone(false);
    
    ESP_LOGI(TAG, "🔌 关闭5V输出节省电量");
    axp192_power_grove_5v(false);
    
    // 保留屏幕显示用于状态指示
    ESP_LOGI(TAG, "📺 保留屏幕显示用于状态指示");
    // axp192_power_tft_display()保持开启
}

void battery_monitoring_demo(void)
{
    ESP_LOGI(TAG, "🔋 电池监控演示");
    
    float voltage, current, power, temp;
    uint8_t level;
    
    if (axp192_get_battery_voltage(&voltage) == ESP_OK) {
        ESP_LOGI(TAG, "🔋 电池电压: %.2fV", voltage);
    }
    
    if (axp192_get_battery_current(&current) == ESP_OK) {
        ESP_LOGI(TAG, "⚡ 电池电流: %.2fmA", current);
    }
    
    if (axp192_get_battery_power(&power) == ESP_OK) {
        ESP_LOGI(TAG, "⚡ 电池功率: %.2fmW", power);
    }
    
    if (axp192_get_battery_level(&level) == ESP_OK) {
        ESP_LOGI(TAG, "📊 电池电量: %d%%", level);
    }
    
    if (axp192_get_internal_temperature(&temp) == ESP_OK) {
        ESP_LOGI(TAG, "🌡️ 内部温度: %.1f°C", temp);
    }
    
    ESP_LOGI(TAG, "🔌 充电状态: %s", axp192_is_charging() ? "充电中" : "未充电");
    ESP_LOGI(TAG, "🔌 电池连接: %s", axp192_is_battery_present() ? "已连接" : "未连接");
    ESP_LOGI(TAG, "🔌 USB连接: %s", axp192_is_vbus_present() ? "已连接" : "未连接");
}

// 安全使用建议：
// 
// ✅ 推荐：
// - 使用axp192_power_*()安全API
// - 固定电压值，防止硬件损坏
// - 基于官方M5Unified规范
//
// ⚠️ 谨慎：
// - axp192_set_*_voltage()直接电压控制
// - 仅在完全了解硬件规格时使用
// - 错误电压可能永久损坏硬件
//
// 🔒 安全特性：
// - LDO3固定3.0V（屏幕显示IC）
// - LDO2固定3.3V（屏幕背光）
// - LDO0固定3.3V（麦克风）
// - DCDC1固定3.3V（ESP32主控）