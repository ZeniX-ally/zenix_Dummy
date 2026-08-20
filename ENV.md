# Dummy 机械臂开发环境（本机 Linux Mint 22.3）

## 已安装工具

| 工具 | 版本 | 用途 |
|---|---|---|
| git | 2.43.0 | 版本管理 |
| cmake | 3.28.3 | 固件构建 |
| make | 4.3 | 构建 |
| gcc-arm-none-eabi | 13.2.1 | ARM 交叉编译 |
| libstdc++-arm-none-eabi-newlib | 13.2.1 | ARM C++ 标准库 |
| openocd | 0.12.0 | ST-Link 烧录/调试 |
| stlink-tools | 1.8.0 | ST-Link 命令行烧录 |
| python3 | 3.12.3 | CLI-Tool |
| python3-serial / usb / jinja2 / ipython | apt | CLI-Tool 依赖 |

## 编译命令

主控固件（STM32F405）：

```bash
cd ~/Dummy-Robot/2.Firmware/Core-STM32F4-fw
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-include cstdio -include string -fpermissive"
cmake --build build -j2
```

电机驱动固件（STM32F103）：

```bash
cd ~/Dummy-Robot/2.Firmware/Ctrl-Step-Driver-STM32F1-fw
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-include cstdio -include string -fpermissive"
cmake --build build -j2
```

输出文件在 `build/` 下的 `.elf / .hex / .bin`。

## 已对上游源码做的本机兼容修改

GCC 13 比原仓库时代更严格，修改已归档入 `firmware/patches/patch-20260818-84453f1.patch`：

1. `Core-STM32F4-fw/Bsp/imu/` 下源码中的 `#include "mpu6050.hpp"` 修正为 `#include "MPU6050.hpp"`（解决 Linux 大小写敏感问题）
2. `Ctrl-Step-Driver-STM32F1-fw/UserApp/common_inc.h`
3. `Ctrl-Step-Driver-STM32F1-fw/UserApp/configurations.h`
4. `Ctrl-Step-Driver-STM32F1-fw/Port/Platform/Utils/st_hardware.h`

其中 2~4 是把 `stdint-gcc.h` 改为标准 `<stdint.h>`。可用 `./tools/apply_patches.sh` 一键应用。

## 串口权限

用户 `zenix` 已加入 `dialout` 组。如果插入 ST-Link/串口后仍提示权限不足，注销重登一次即可。

## CLI-Tool

```bash
cd ~/Dummy-Robot/3.Software/CLI-Tool
python3 run_shell.py --help
```

可接上机械臂后通过 USB/串口控制。
