#!/usr/bin/env bash
# 将 firmware/patches/ 下的补丁应用到上游仓库
# 用法: ./tools/apply_patches.sh [--dry-run] [补丁文件]  （默认全部应用）
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
UPSTREAM="${DUMMY_ROOT:-$HOME/Dummy-Robot}"
PATCH_DIR="$ROOT/firmware/patches"

[[ -d "$UPSTREAM" ]] || { echo "错误: 上游目录不存在: $UPSTREAM"; exit 1; }

DRY_RUN=0
TARGET_PATCH=""

usage() {
  echo "用法: $0 [--dry-run] [补丁文件]"
  echo "  不带参数: 应用 firmware/patches/ 下所有 .patch"
  echo "  --dry-run: 仅检查补丁是否能应用，不实际修改文件"
  echo "  指定文件: 仅应用该补丁"
  exit 0
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    -h|--help)
      usage
      ;;
    --dry-run|-n)
      DRY_RUN=1
      shift
      ;;
    *)
      TARGET_PATCH="$1"
      shift
      ;;
  esac
done

if [[ -n "$TARGET_PATCH" ]]; then
  FILES=("$TARGET_PATCH")
else
  mapfile -t FILES < <(find "$PATCH_DIR" -name "*.patch" | sort)
  [[ ${#FILES[@]} -gt 0 ]] || { echo "patches/ 下没有补丁文件。"; exit 0; }
fi

for f in "${FILES[@]}"; do
  if [[ $DRY_RUN -eq 1 ]]; then
    echo "==> 测试(dry-run): $f"
    if git -C "$UPSTREAM" apply --check --reverse "$f" &>/dev/null; then
      echo "    [OK] 补丁已处于应用状态 (reverse 匹配)"
    elif git -C "$UPSTREAM" apply --check "$f" &>/dev/null; then
      echo "    [OK] 可正常应用"
    else
      echo "    [FAIL] 无法应用（可能上下文不符）"
      git -C "$UPSTREAM" apply --check "$f" || true
    fi
  else
    echo "==> 应用: $f"
    if git -C "$UPSTREAM" apply --check --reverse "$f" &>/dev/null; then
      echo "    [SKIP] 补丁已应用，跳过"
    elif git -C "$UPSTREAM" apply --check "$f" 2>/dev/null; then
      git -C "$UPSTREAM" apply "$f"
      echo "    [OK] 应用成功"
    else
      echo "    警告: 应用失败（可能上下文不符）: $f"
    fi
  fi
done

echo "完成。可用 tools/check_env.sh 或重新编译验证。"
