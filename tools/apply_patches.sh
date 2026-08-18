#!/usr/bin/env bash
# 将 firmware/patches/ 下的补丁应用到上游仓库
# 用法: ./tools/apply_patches.sh [补丁文件]  （默认全部应用）
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
UPSTREAM="${DUMMY_ROOT:-$HOME/Dummy-Robot}"
PATCH_DIR="$ROOT/firmware/patches"

[[ -d "$UPSTREAM" ]] || { echo "错误: 上游目录不存在: $UPSTREAM"; exit 1; }

usage() {
  echo "用法: $0 [补丁文件]"
  echo "  不带参数: 应用 firmware/patches/ 下所有 .patch"
  echo "  指定文件: 仅应用该补丁"
  exit 0
}

case "${1:-}" in
  -h|--help) usage ;;
esac

if [[ -n "${1:-}" ]]; then
  FILES=("$1")
else
  mapfile -t FILES < <(find "$PATCH_DIR" -name "*.patch" | sort)
  [[ ${#FILES[@]} -gt 0 ]] || { echo "patches/ 下没有补丁文件。"; exit 0; }
fi

for f in "${FILES[@]}"; do
  echo "==> 应用: $f"
  git -C "$UPSTREAM" apply --check "$f" 2>/dev/null \
    && git -C "$UPSTREAM" apply "$f" \
    || { echo "    警告: 应用失败（可能已应用或上下文不符）: $f"; }
done

echo "完成。可用 tools/check_env.sh 或重新编译验证。"
