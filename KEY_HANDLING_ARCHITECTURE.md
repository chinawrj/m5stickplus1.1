# 按键处理架构设计文档

## 🎯 **问题背景**

原始设计中，`page_manager_lvgl.c` 只处理 `LV_KEY_RIGHT` 进行页面导航，其他按键没有得到适当处理。各个页面无法响应页面特定的按键操作。

## 🏗️ **新架构设计**

### **分层按键处理机制**

```
按键事件流向：
Hardware Button → LVGL Input Device → Screen Level Handler → Page-specific Handler → Global Handler
```

### **处理优先级**

1. **页面级处理器** - 最高优先级，页面可以"拦截"特定按键
2. **全局导航处理器** - 处理通用导航按键（如`LV_KEY_RIGHT`）
3. **未处理按键** - 记录日志但不影响系统运行

## 📋 **实现细节**

### **1. 页面控制器接口扩展**

在 `page_manager.h` 中为页面控制器添加按键处理钩子：

```c
typedef struct {
    // 生命周期函数
    esp_err_t (*init)(void);
    esp_err_t (*create)(void);
    esp_err_t (*update)(void);
    esp_err_t (*destroy)(void);
    
    // 数据状态管理
    bool (*is_data_updated)(void);
    
    // 🆕 输入事件处理（可选）
    bool (*handle_key_event)(uint32_t key);  // 返回true表示已处理
    
    // 页面信息
    const char *name;
    page_id_t page_id;
} page_controller_t;
```

### **2. 页面管理器按键分发**

在 `page_manager.c` 中添加按键事件分发函数：

```c
bool page_manager_handle_key_event(uint32_t key)
{
    // 获取当前页面控制器
    const page_controller_t *controller = g_page_controllers[g_current_page];
    
    // 首先让当前页面处理按键事件
    if (controller->handle_key_event) {
        bool handled = controller->handle_key_event(key);
        if (handled) {
            return true;  // 页面已处理，无需全局处理
        }
    }
    
    return false;  // 页面未处理，交给全局处理器
}
```

### **3. LVGL 屏幕级按键处理器**

在 `page_manager_lvgl.c` 中修改屏幕级按键处理：

```c
static void screen_key_event_cb(lv_event_t *e)
{
    uint32_t key = lv_event_get_key(e);
    
    // 🆕 首先询问当前页面是否要处理此按键
    bool page_handled = page_manager_handle_key_event(key);
    if (page_handled) {
        return;  // 页面已处理，完成
    }
    
    // 页面未处理，执行全局导航逻辑
    if (key == LV_KEY_RIGHT) {
        page_manager_lvgl_next();  // 导航到下一页
    } else if (key == LV_KEY_ENTER) {
        // 全局ENTER按键处理（如果需要）
    }
    // 其他按键记录但不处理
}
```

## 🔧 **各页面按键处理示例**

### **Monitor 页面**

```c
static bool monitor_page_handle_key_event(uint32_t key)
{
    switch (key) {
        case LV_KEY_ENTER:
            // 切换电源信息显示模式
            ESP_LOGI(TAG, "Toggle power info display");
            return true;
            
        case LV_KEY_UP:
            // 增加显示亮度
            ESP_LOGI(TAG, "Increase brightness");
            return true;
            
        case LV_KEY_DOWN:
            // 降低显示亮度
            ESP_LOGI(TAG, "Decrease brightness");
            return true;
            
        default:
            return false;  // 让全局处理器处理
    }
}
```

### **ESP-NOW 页面**

```c
static bool espnow_page_handle_key_event(uint32_t key)
{
    switch (key) {
        case LV_KEY_ENTER:
            // 发送测试数据包
            ESP_LOGI(TAG, "Send test packet");
            g_packets_sent++;
            return true;
            
        case LV_KEY_UP:
            // 增加发射功率
            ESP_LOGI(TAG, "Increase transmission power");
            return true;
            
        case LV_KEY_DOWN:
            // 降低发射功率
            ESP_LOGI(TAG, "Decrease transmission power");
            return true;
            
        default:
            return false;  // 让全局处理器处理
    }
}
```

## 🎮 **按键功能映射**

| 按键 | 全局功能 | Monitor页面 | ESP-NOW页面 |
|------|----------|-------------|-------------|
| **RIGHT** | 下一页导航 | - | - |
| **ENTER** | - | 切换电源显示 | 发送测试包 |
| **UP** | - | 增加亮度 | 增加发射功率 |
| **DOWN** | - | 降低亮度 | 降低发射功率 |

## ✅ **架构优势**

### **1. 清晰的职责分离**
- **全局导航**: 页面切换由页面管理器统一处理
- **页面功能**: 页面特定操作由各页面自己处理
- **扩展性强**: 新页面可以轻松添加自定义按键行为

### **2. 符合LVGL最佳实践**
- 使用标准的LVGL事件处理机制
- 屏幕级事件处理器作为事件分发中心
- 页面控制器模式便于管理复杂交互

### **3. 灵活的按键响应**
- 页面可以"拦截"任何按键进行特殊处理
- 未处理的按键自动回退到全局处理
- 支持页面间按键行为的差异化

### **4. 易于调试和维护**
- 详细的日志记录显示按键处理路径
- 明确的返回值指示处理状态
- 模块化设计便于单独测试

## 🚀 **使用方法**

### **为新页面添加按键处理**

1. **在页面实现文件中添加按键处理函数**:
   ```c
   static bool my_page_handle_key_event(uint32_t key) {
       // 处理页面特定按键
       return handled;
   }
   ```

2. **在页面控制器中注册处理函数**:
   ```c
   static const page_controller_t my_controller = {
       // ... 其他函数
       .handle_key_event = my_page_handle_key_event,
       // ... 其他属性
   };
   ```

3. **按键事件将自动路由到页面处理器**

## 🎯 **最佳实践建议**

1. **保持一致性**: 相同按键在不同页面应有相似的功能逻辑
2. **提供反馈**: 按键操作应通过UI或日志提供明确反馈
3. **优雅降级**: 页面应只处理真正需要的按键，其他交给全局
4. **文档化**: 每个页面的按键功能应在注释中明确说明

这个架构为M5StickC Plus 1.1提供了强大且灵活的按键处理能力，既保持了全局导航的一致性，又允许各页面实现独特的交互功能。