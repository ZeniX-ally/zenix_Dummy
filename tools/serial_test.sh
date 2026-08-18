#!/usr/bin/env bash
# 串口连通性自检：枚举串口并做回显测试
# 用法: ./tools/serial_test.sh [-p <串口设备>]
set -euo pipefail

usage() {
  echo "用法: $0 [-p <串口设备>]"
  echo "  -p <设备>   指定串口（如 /dev/ttyACM0），默认自动探测"
  exit 0
}

PORT=""
while [[ $# -gt 0 ]]; do
  case "$1" in
    -p) PORT="$2"; shift 2 ;;
    -h|--help) usage ;;
    *) echo "未知参数: $1"; usage ;;
  esac
done

# 探测串口设备
probe() {
  ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null || true
}

if [[ -z "$PORT" ]]; then
  PORT=$(probe | head -1)
  [[ -n "$PORT" ]] || { echo "未检测到串口设备，请检查 USB 连接与 dialout 权限"; exit 1; }
fi

[[ -e "$PORT" ]] || { echo "错误: 串口不存在: $PORT"; exit 1; }
[[ -r "$PORT" ]] || { echo "权限不足: $PORT，请确认在 dialout 组（注销重登生效）"; exit 1; }

echo "串口: $PORT 就绪"
echo "可用 CLI-Tool:"
echo "  cd ~/Dummy-Robot/3.Software/CLI-Tool && python3 run_shell.py"
