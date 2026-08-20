# patches/ — 上游兼容修改补丁

本目录存放对上游 `~/Dummy-Robot` 源码的所有修改补丁，是"差异唯一事实来源"。

## 已有补丁

| 补丁文件 | 说明 |
|---|---|
| `patch-20260818-84453f1.patch` | 1. `Ctrl-Step-Driver-STM32F1-fw` 中 `stdint-gcc.h` → `<stdint.h>`（3 处）<br>2. `Core-STM32F4-fw/Bsp/imu/` 中 `#include "mpu6050.hpp"` → `#include "MPU6050.hpp"`（3 处，解决 Linux 大小写敏感问题） |

## 流程

在 `~/zenix_Dummy` 中一键导出：

```bash
make patches-export
# 或 ./tools/export_patches.sh
```

一键测试/应用：

```bash
./tools/apply_patches.sh --dry-run
make patches-apply
```

并在 `docs/roadmap.md` 的变更记录中登记。
