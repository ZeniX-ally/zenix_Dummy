#!/usr/bin/env bash
# 开发环境自检：检查工具链、上游源码、串口权限
# 用法: ./tools/check_env.sh
set -euo pipefail

UPSTREAM="${DUMMY_ROOT:-$HOME/Dummy-Robot}"

PASS=0
FAIL=0

check() { # <名称> <命令>
  local name="$1"; shift
  if "$@" >/dev/null 2>&1; then
    echo "  [OK]   $name"
    PASS=$((PASS+1))
  else
    echo "  [MISS] $name"
    FAIL=$((FAIL+1))
  fi
}

echo "== 工具链 =="
check "git"            git --version
check "cmake"          cmake --version
check "make"           make --version
check "arm-none-eabi-gcc" arm-none-eabi-gcc --version
check "openocd"        openocd --version
check "st-flash"       st-flash --version
check "python3"        python3 --version
check "pyserial (python3-serial)" python3 -c "import serial"

echo "== 上游源码 =="
if [[ -d "$UPSTREAM" ]]; then
  echo "  [OK]   上游目录: $UPSTREAM"
  PASS=$((PASS+1))
else
  echo "  [MISS] 上游目录: $UPSTREAM（可用 DUMMY_ROOT 覆盖）"
  FAIL=$((FAIL+1))
fi

echo "== 串口权限 =="
if groups | grep -qw dialout; then
  echo "  [OK]   用户已在 dialout 组"
  PASS=$((PASS+1))
else
  echo "  [MISS] 不在 dialout 组，无法访问串口"
  FAIL=$((FAIL+1))
fi

echo
echo "通过: $PASS  缺失: $FAIL"
[[ "$FAIL" -eq 0 ]]
