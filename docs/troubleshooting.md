# 排错手册

> 遇到问题时先查这里，解决后请补充记录。

## 编译

### 编译报错与 `stdint-gcc.h` 有关

GCC 13 移除了 `stdint-gcc.h`，需改为标准 `<stdint.h>`。
涉及文件（见 `ENV.md` 第 3 节），修改后导出补丁：

```bash
make patches-export
```

## 烧录

### 提示 `No ST-Link detected`

1. 检查 ST-Link 是否插入、驱动是否识别（`lsusb`）
2. 重新插拔 USB
3. 若仍失败，检查接线（SWDIO/SWCLK/GND/3V3）

## 串口

### 权限不足（Permission denied）

用户需在 `dialout` 组，加入后**注销重登**生效：

```bash
sudo usermod -aG dialout $USER
```

### 找不到串口设备

```bash
make serial   # 自动探测 /dev/ttyUSB* /dev/ttyACM*
```

## 补丁

### 应用补丁失败（可能已应用）

`apply_patches.sh` 会跳过 `git apply --check` 失败的补丁（已应用或上下文不符）。
若确需强制，手动在上游目录执行 `git apply --3way <补丁>`。

---

## 变更记录

| 日期 | 问题 | 解决 |
|---|---|---|
| _待填_ | | |
