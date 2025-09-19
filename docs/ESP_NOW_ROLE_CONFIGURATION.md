# ESP-NOW角色配置指南

## 概述

通过修改ESP-NOW的magic number比较逻辑，可以强制指定设备角色：
- **M5StickC Plus**: 永远作为接收者 (RECEIVER-ONLY)
- **M5NanoC6**: 永远作为发送者 (SENDER-ONLY)

## M5StickC Plus配置 (已完成)

**文件**: `main/espnow_manager.c`

**修改内容**:
```c
// 原始代码 (官方例子的动态角色协商)
if (send_param->unicast == false && send_param->magic >= recv_magic) {
    // 成为sender
}

// 修改后 (强制为接收者)
if (false && send_param->unicast == false && send_param->magic >= recv_magic) {
    // 永远不会成为sender
} else {
    ESP_LOGI(TAG, "📥 M5StickC Plus acting as RECEIVER-ONLY");
    s_stats.is_sender = false;
}
```

**结果**: M5StickC Plus永远不会发送unicast数据，只接收数据。

## M5NanoC6配置 (需要实施)

**文件**: `main/espnow_manager.c` (或对应的ESP-NOW管理文件)

**修改内容**:
```c
// 原始代码
if (send_param->unicast == false && send_param->magic >= recv_magic) {
    // 成为sender
}

// 修改后 (强制为发送者)
if (true || (send_param->unicast == false && send_param->magic >= recv_magic)) {
    ESP_LOGI(TAG, "🎯 M5NanoC6 acting as SENDER-ONLY");
    s_stats.is_sender = true;
    // ... 发送逻辑
}
```

**或者更简单的方法**:
```c
// 直接让M5NanoC6的magic number永远最大
send_param->magic = 0xFFFFFFFF;  // 最大值，永远胜出
```

## 验证方法

### M5StickC Plus (接收者)
运行后应该看到日志：
```
📥 M5StickC Plus acting as RECEIVER-ONLY (Magic: 0x12345678 < 0x87654321)
📨 Receive 0th unicast from: xx:xx:xx:xx:xx:xx, len: 10
```

### M5NanoC6 (发送者)
运行后应该看到日志：
```
🎯 M5NanoC6 acting as SENDER-ONLY
📤 Send data to xx:xx:xx:xx:xx:xx
✅ Send success
```

## 实现方案对比

| 方案 | M5StickC Plus | M5NanoC6 | 优点 | 缺点 |
|------|---------------|----------|------|------|
| **条件强制** | `if (false && ...)` | `if (true \|\| ...)` | 逻辑清晰 | 需要两边都修改 |
| **Magic强制** | `magic = 0x00000000` | `magic = 0xFFFFFFFF` | 简单直接 | 可能影响其他逻辑 |
| **角色标志** | `role = RECEIVER` | `role = SENDER` | 最清晰 | 需要重构代码 |

## 建议实施步骤

1. ✅ **M5StickC Plus**: 已完成接收者配置
2. 🔄 **M5NanoC6**: 修改为发送者配置  
3. 🧪 **测试**: 验证数据流向 (NanoC6 → StickC Plus)
4. 📊 **监控**: 确认角色固定且通信稳定

## 注意事项

- 修改后设备角色将固定，不再动态协商
- 确保两个设备使用相同的PMK和LMK密钥
- 测试时注意观察日志中的角色确认信息
- broadcast数据仍然是双向的，只有unicast受影响