# 🔊 M5StickC Plus无源蜂鸣器驱动

## 📋 硬件规格

- **蜂鸣器类型**: 无源蜂鸣器 (Passive Buzzer)
- **GPIO引脚**: GPIO2
- **驱动方式**: PWM (LEDC)
- **频率范围**: 20Hz - 20kHz
- **音量控制**: 0-100% (PWM占空比)

## 🔧 驱动特性

### ✅ 基础功能
- PWM频率和占空比控制
- 可调音量 (0-100%)
- 自动资源管理
- 错误处理和状态检查

### 🎵 音效功能
- 单音调播放 (指定频率和时长)
- 预设音符常量 (C4-C6)
- 预设旋律:
  - `buzzer_play_startup()` - 开机音效
  - `buzzer_play_success()` - 成功音效  
  - `buzzer_play_error()` - 错误警告音
  - `buzzer_play_notification()` - 通知音

### 🧪 测试功能
- 频率扫描测试 (400Hz-2000Hz)
- 音阶测试 (C4-C5)
- 音量级别测试 (25%, 50%, 75%, 100%)
- 预设旋律演示

## 📖 API使用示例

### 基础初始化
```c
#include "buzzer.h"

// 初始化蜂鸣器
esp_err_t ret = buzzer_init();
if (ret == ESP_OK) {
    ESP_LOGI(TAG, "蜂鸣器初始化成功");
}
```

### 播放音调
```c
// 播放440Hz音调500ms
buzzer_tone(440, 500);

// 播放A4音符250ms
buzzer_tone(NOTE_A4, DURATION_QUARTER);

// 连续播放 (需要手动停止)
buzzer_tone(NOTE_C5, 0);
vTaskDelay(pdMS_TO_TICKS(1000));
buzzer_stop();
```

### 音量控制
```c
// 设置音量为50%
buzzer_set_volume(50);

// 设置音量为最大
buzzer_set_volume(100);

// 静音
buzzer_set_volume(0);
```

### 预设音效
```c
// 开机音效 (上升音阶)
buzzer_play_startup();

// 成功音效 (愉快旋律)
buzzer_play_success();

// 错误音效 (警告音)
buzzer_play_error();

// 通知音效 (双音调)
buzzer_play_notification();
```

### 完整测试
```c
// 运行所有测试模式
buzzer_test_patterns();
```

### 资源清理
```c
// 清理资源 (自动停止音调)
buzzer_deinit();
```

## 🎼 音符常量

### 第4八度音符 (中央C八度)
```c
NOTE_C4  = 262Hz   // Do
NOTE_D4  = 294Hz   // Re  
NOTE_E4  = 330Hz   // Mi
NOTE_F4  = 349Hz   // Fa
NOTE_G4  = 392Hz   // Sol
NOTE_A4  = 440Hz   // La (标准音)
NOTE_B4  = 494Hz   // Si
```

### 第5八度音符 (高音)
```c
NOTE_C5  = 523Hz   // 高音Do
NOTE_D5  = 587Hz   // 高音Re
NOTE_E5  = 659Hz   // 高音Mi
NOTE_F5  = 698Hz   // 高音Fa
NOTE_G5  = 784Hz   // 高音Sol
NOTE_A5  = 880Hz   // 高音La
NOTE_B5  = 988Hz   // 高音Si
```

## ⚙️ 技术实现

### PWM配置
- **LEDC定时器**: LEDC_TIMER_0
- **LEDC通道**: LEDC_CHANNEL_0  
- **分辨率**: 13位 (8192级精度)
- **时钟源**: LEDC_AUTO_CLK
- **速度模式**: LEDC_LOW_SPEED_MODE

### 音量实现
音量通过PWM占空比控制：
```c
// 最大占空比 = 2^13 - 1 = 8191
// 实际占空比 = (最大占空比 * 音量%) / 200
// 除以200是因为使用50%占空比作为最大值(方波)
uint32_t duty = (max_duty * volume_percent) / 200;
```

### 错误处理
- 参数验证 (频率范围20-20000Hz)
- 初始化状态检查
- ESP-IDF错误码返回
- 详细错误日志输出

## 🔌 集成到项目

蜂鸣器已集成到M5StickC Plus开机测试序列中:

1. **开机演示**: 在硬件外设演示中自动运行
2. **测试顺序**: AXP192 → 显示屏 → **蜂鸣器** → 其他外设
3. **测试内容**: 初始化 → 启动音效 → 完整测试 → 成功音效 → 清理

### 演示流程
```
🔊 开始无源蜂鸣器演示
🔊 蜂鸣器初始化成功  
🎵 开始蜂鸣器测试...
🎵 播放启动音效
🎵 频率扫描测试 (400Hz-2000Hz)
🎵 音阶测试 (C4-C5)
🎵 音量测试 (25%-100%)
🎵 预设旋律演示
🎵 蜂鸣器测试完成
🎵 播放成功音效
🧹 清理蜂鸣器资源
```

## 🚀 使用建议

### 性能优化
- 短音调使用`buzzer_beep()`更高效
- 长时间播放使用`buzzer_tone(freq, 0)`然后手动停止
- 音量调节在50%以下可降低功耗

### 用户体验
- 开机音效: 使用`buzzer_play_startup()`
- 操作确认: 使用`buzzer_play_notification()`  
- 成功反馈: 使用`buzzer_play_success()`
- 错误警告: 使用`buzzer_play_error()`

### 功耗考虑
- 不使用时调用`buzzer_deinit()`释放资源
- 降低音量可减少功耗
- 避免高频率长时间播放

## 🐛 故障排除

### 常见问题
1. **无声音输出**
   - 检查GPIO2连接
   - 确认`buzzer_init()`返回ESP_OK
   - 检查音量设置 (`buzzer_set_volume()`)

2. **音调不准确**  
   - 验证频率范围 (20-20000Hz)
   - 检查PWM时钟配置
   - 确认LEDC定时器未被其他组件占用

3. **初始化失败**
   - 确认GPIO2未被其他功能占用
   - 检查LEDC资源可用性
   - 查看错误日志获取详细信息

### 调试信息
启用详细日志查看蜂鸣器运行状态:
```c
esp_log_level_set("BUZZER", ESP_LOG_DEBUG);
```