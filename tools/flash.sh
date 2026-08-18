#!/usr/bin/env bash
# 用 ST-Link 烧录 Dummy-Robot 固件
# 用法: ./tools/flash.sh <core|driver|hex路径> [--reset]
set -euo pipefail

UPSTREAM="${DUMMY_ROOT:-$HOME/Dummy-Robot}"

usage() {
  echo "用法: $0 <目标>"
  echo "  core     烧录主控固件 (Core-STM32F4-fw)"
  echo "  driver   烧录驱动固件 (Ctrl-Step-Driver-STM32F1-fw)"
  echo "  <路径>   直接烧录指定 .hex/.bin 文件"
  exit 0
}

[[ $# -ge 1 ]] || usage
TARGET="$1"

# 定位 HEX 产物
case "$TARGET" in
  core)
    HEX=$(find "$UPSTREAM/2.Firmware/Core-STM32F4-fw/build" -name "*.hex" | head -1)
    ;;
  driver)
    HEX=$(find "$UPSTREAM/2.Firmware/Ctrl-Step-Driver-STM32F1-fw/build" -name "*.hex" | head -1)
    ;;
  -h|--help) usage ;;
  *)
    [[ -f "$TARGET" ]] || { echo "错误: 文件不存在: $TARGET"; exit 1; }
    HEX="$TARGET"
    ;;
esac

[[ -n "$HEX" && -f "$HEX" ]] || { echo "错误: 未找到 .hex 产物，请先运行 tools/build.sh"; exit 1; }

echo "烧录: $HEX"
st-flash --reset write "$HEX" 0x08000000
