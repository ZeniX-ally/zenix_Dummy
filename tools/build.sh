#!/usr/bin/env bash
# 一键编译 Dummy-Robot 两个固件（主控 STM32F405 + 驱动 STM32F103）
# 用法: ./tools/build.sh [-j <并行数>] [--clean]
set -euo pipefail

UPSTREAM="${DUMMY_ROOT:-$HOME/Dummy-Robot}"
JOBS=2
CLEAN=0

usage() {
  echo "用法: $0 [-j <并行数>] [--clean]"
  echo "  -j <n>    并行编译任务数（默认 2）"
  echo "  --clean   先清理 build 目录"
  exit 0
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    -j) JOBS="$2"; shift 2 ;;
    --clean) CLEAN=1; shift ;;
    -h|--help) usage ;;
    *) echo "未知参数: $1"; usage ;;
  esac
done

CXX_FLAGS="-include cstdio -include string -fpermissive"
FW_DIRS=(
  "2.Firmware/Core-STM32F4-fw"
  "2.Firmware/Ctrl-Step-Driver-STM32F1-fw"
)

[[ -d "$UPSTREAM" ]] || { echo "错误: 上游目录不存在: $UPSTREAM（可用 DUMMY_ROOT 覆盖）"; exit 1; }

for rel in "${FW_DIRS[@]}"; do
  dir="$UPSTREAM/$rel"
  echo "==> [$rel]"
  if [[ "$CLEAN" == 1 && -d "$dir/build" ]]; then
    rm -rf "$dir/build"
  fi
  cmake -S "$dir" -B "$dir/build" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_FLAGS="$CXX_FLAGS"
  cmake --build "$dir/build" -j"$JOBS"
done

echo "完成。产物在 build/ 下的 .elf / .hex / .bin。"
