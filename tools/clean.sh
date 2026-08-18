#!/usr/bin/env bash
# 清理上游仓库的构建产物（build 目录）
# 用法: ./tools/clean.sh
set -euo pipefail

UPSTREAM="${DUMMY_ROOT:-$HOME/Dummy-Robot}"

[[ -d "$UPSTREAM" ]] || { echo "错误: 上游目录不存在: $UPSTREAM"; exit 1; }

echo "将删除以下 build 目录:"
find "$UPSTREAM/2.Firmware" -maxdepth 2 -type d -name build -print
read -r -p "确认清理? [y/N] " ans
[[ "$ans" == "y" || "$ans" == "Y" ]] || { echo "已取消"; exit 0; }

find "$UPSTREAM/2.Firmware" -maxdepth 2 -type d -name build -exec rm -rf {} +
echo "清理完成。"
