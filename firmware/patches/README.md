# patches/ — 上游兼容修改补丁

本目录存放对上游 `~/Dummy-Robot` 源码的所有修改补丁，是"差异唯一事实来源"。

## 已有修改（对应 `ENV.md` 第 3 节）

| 补丁 | 说明 |
|---|---|
| _待导出_ | `Core-STM32F4-fw/Bsp/imu/` 增加 `mpu6050.hpp` 小写软链接 |
| _待导出_ | `Ctrl-Step-Driver-STM32F1-fw` 中 `stdint-gcc.h` → `<stdint.h>`（3 处） |

## 流程

在 `~/Dummy-Robot` 中导出：

```bash
git diff > zenix_Dummy/firmware/patches/xxx-<描述>.patch
```

并在 `docs/roadmap.md` 的变更记录中登记。
