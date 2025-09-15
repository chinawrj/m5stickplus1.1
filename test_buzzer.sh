#!/bin/bash

# M5StickC Plus 蜂鸣器测试脚本
# 使用方法: ./test_buzzer.sh [PORT]

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 项目目录
PROJECT_DIR="/Users/rjwang/fun/m5stickplus1.1"

echo -e "${BLUE}🔊 M5StickC Plus 蜂鸣器测试脚本${NC}"
echo "=================================================="

# 检查是否在项目目录
if [ ! -f "$PROJECT_DIR/main/buzzer.h" ]; then
    echo -e "${RED}❌ 错误: 未找到蜂鸣器驱动文件${NC}"
    echo "请确保在正确的项目目录运行此脚本"
    exit 1
fi

cd "$PROJECT_DIR"

# 设置ESP-IDF环境
echo -e "${YELLOW}📦 设置ESP-IDF环境...${NC}"
if [ -f "$HOME/esp/esp-idf/export.sh" ]; then
    source "$HOME/esp/esp-idf/export.sh"
    echo -e "${GREEN}✅ ESP-IDF环境设置完成${NC}"
else
    echo -e "${RED}❌ 错误: 未找到ESP-IDF安装${NC}"
    echo "请先安装ESP-IDF并确保路径正确"
    exit 1
fi

# 编译项目
echo -e "${YELLOW}🔨 编译项目...${NC}"
if idf.py build; then
    echo -e "${GREEN}✅ 编译成功${NC}"
else
    echo -e "${RED}❌ 编译失败${NC}"
    exit 1
fi

# 检测串口
PORT=${1:-""}
if [ -z "$PORT" ]; then
    echo -e "${YELLOW}🔍 自动检测串口...${NC}"
    
    # macOS串口检测
    if [[ "$OSTYPE" == "darwin"* ]]; then
        PORTS=$(ls /dev/cu.usbserial-* 2>/dev/null || ls /dev/cu.SLAB_USBtoUART* 2>/dev/null || echo "")
        if [ -n "$PORTS" ]; then
            PORT=$(echo "$PORTS" | head -n1)
            echo -e "${GREEN}📱 发现设备: $PORT${NC}"
        fi
    fi
    
    # Linux串口检测
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        PORTS=$(ls /dev/ttyUSB* 2>/dev/null || echo "")
        if [ -n "$PORTS" ]; then
            PORT=$(echo "$PORTS" | head -n1)
            echo -e "${GREEN}📱 发现设备: $PORT${NC}"
        fi
    fi
    
    if [ -z "$PORT" ]; then
        echo -e "${YELLOW}⚠️  未自动检测到设备，请手动指定端口:${NC}"
        echo "使用方法: $0 /dev/cu.usbserial-xxxxx"
        echo ""
        echo "可用端口:"
        ls /dev/cu.* 2>/dev/null | grep -E "(usbserial|SLAB)" || echo "未找到串口设备"
        exit 1
    fi
fi

echo -e "${BLUE}📡 使用串口: $PORT${NC}"

# 烧录固件
echo -e "${YELLOW}⚡ 烧录固件到M5StickC Plus...${NC}"
if idf.py -p "$PORT" flash; then
    echo -e "${GREEN}✅ 烧录成功${NC}"
else
    echo -e "${RED}❌ 烧录失败${NC}"
    echo "请检查:"
    echo "1. M5StickC Plus是否已连接"
    echo "2. 串口权限是否正确"
    echo "3. 设备是否进入下载模式"
    exit 1
fi

# 开始监控
echo -e "${BLUE}🎵 开始监控蜂鸣器测试...${NC}"
echo "=================================================="
echo -e "${GREEN}预期听到以下音效序列:${NC}"
echo "1. 🎵 启动音效 (上升音阶: C4-E4-G4-C5)"
echo "2. 🔍 频率扫描 (400Hz-2000Hz)"
echo "3. 🎼 音阶测试 (C4到C5八个音符)"
echo "4. 🔊 音量测试 (25%/50%/75%/100%)"
echo "5. 🎵 预设旋律演示"
echo "6. ✅ 成功音效 (愉快旋律: C5-E5-G5)"
echo ""
echo -e "${YELLOW}按 Ctrl+] 退出监控${NC}"
echo "=================================================="

# 启动监控并过滤蜂鸣器相关信息
idf.py -p "$PORT" monitor | grep -E "(BUZZER|🔊|🎵|🎨|🖥️)" --line-buffered || true

echo -e "${BLUE}🎉 蜂鸣器测试完成！${NC}"