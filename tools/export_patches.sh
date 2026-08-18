#!/usr/bin/env bash
# 从上游仓库导出未提交修改为补丁，保存到 firmware/patches/
# 用法: ./tools/export_patches.sh [文件名]  （默认自动命名）
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
UPSTREAM="${DUMMY_ROOT:-$HOME/Dummy-Robot}"
PATCH_DIR="$ROOT/firmware/patches"

[[ -d "$UPSTREAM" ]] || { echo "错误: 上游目录不存在: $UPSTREAM"; exit 1; }

# 确认上游是 git 仓库
if ! git -C "$UPSTREAM" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  echo "错误: 上游目录不是 git 仓库，无法导出补丁"
  exit 1
fi

# 检查是否有未提交修改
if git -C "$UPSTREAM" diff --quiet; then
  echo "上游无未提交修改。"
  exit 0
fi

NAME="${1:-}"
if [[ -z "$NAME" ]]; then
  DATE="$(date +%Y%m%d)"
  NAME="patch-$DATE-$(git -C "$UPSTREAM" rev-parse --short HEAD).patch"
fi
OUT="$PATCH_DIR/$NAME"

git -C "$UPSTREAM" diff > "$OUT"
echo "已导出: $OUT"
echo "请在 docs/roadmap.md 变更记录中登记说明。"
