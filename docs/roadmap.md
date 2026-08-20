# 路线图与任务进度

任务编号：**T014656**

## 上游基线

- 上游仓库：`~/Dummy-Robot`
- 上游 base commit：`84453f1` (Merge pull request #144 from TitanRGB-Contribute/main)

## 补丁列表 (firmware/patches/)

| 补丁文件 | 影响模块 | 说明 | 状态 |
|---|---|---|---|
| `patch-20260818-84453f1.patch` | Core-STM32F4-fw, Ctrl-Step-Driver-STM32F1-fw | 1. 修复 F1 固件 3 处 `stdint-gcc.h` 为 `<stdint.h>` (GCC 13 兼容)<br>2. 修复 F4 固件 3 处 `mpu6050.hpp` 引用大小写为 `MPU6050.hpp` (Linux 文件系统兼容) | ✅ 已验证应用与编译 |

## 里程碑

| 阶段 | 内容 | 状态 |
|---|---|---|
| M0 环境搭建 | 工具链 / 串口权限 / 固件可编译 | ✅ 已完成（见 `ENV.md`） |
| M1 固件就绪 | 主控 + 驱动固件编译通过并烧录 | ☐ |
| M2 机械组装 | 结构件、减速器、电机装配 | ☐ |
| M3 联调运动 | 上位机控制各关节运动 | ☐ |
| M4 夹爪抓取 | 末端夹爪动作与抓取 | ☐ |

## 变更记录

| 日期 | 内容 |
|---|---|
| 2026-08-18 | 导出兼容补丁 `patch-20260818-84453f1.patch`，包含 GCC 13 `<stdint.h>` 与 `MPU6050.hpp` 大小写修复；测试 dry-run 与 clean build 全部通过 |
