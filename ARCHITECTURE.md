# zenix_Dummy 项目架构

本仓库是自研桌面机械臂的工作区，复刻并定制 [Dummy-Robot](https://github.com/peng-zhihui/Dummy-Robot)。
架构目标：**与上游可对照、差异可追溯、按层解耦**。

---

## 1. 系统架构

按数据流向分四层：

```
┌──────────────────────────────────────────────────────────────┐
│ 应用层  PC 上位机                                             │
│         DummyStudio(参考) / CLI-Tool / 自研控制台             │
│         连接方式：USB 虚拟串口（CDC）                          │
└──────────────────────────┬───────────────────────────────────┘
                           │ 串口协议（Dummy 协议）
┌──────────────────────────▼───────────────────────────────────┐
│ 主控层  STM32F405                                          │
│         Core-STM32F4-fw：协议解析、运动学、轨迹、IMU(MPU6050)  │
│         对外：USB/串口   对内：CAN 总线                        │
└──────────────────────────┬───────────────────────────────────┘
                           │ CAN 总线（电机控制协议）
┌──────────────────────────▼───────────────────────────────────┐
│ 驱动层  STM32F103 电机驱动板 ×N（每关节/夹爪各一）            │
│         Ctrl-Step-Driver-STM32F1-fw：FOC、电流环、编码器        │
└──────────────────────────┬───────────────────────────────────┘
                           │ 功率/反馈
┌──────────────────────────▼───────────────────────────────────┐
│ 机械层  6 轴结构 + 谐波减速器 + 42/35 步进电机 + 末端夹爪      │
└──────────────────────────────────────────────────────────────┘
```

**通信拓扑**

| 链路 | 介质 | 协议 | 说明 |
|---|---|---|---|
| PC ↔ 主控 | USB CDC / 串口 | Dummy 串口协议 | CLI-Tool 控制入口 |
| 主控 ↔ 驱动板 | CAN 总线 | Dummy CAN 协议 | 多驱动板挂同一总线 |
| 主控 ↔ IMU | SPI/I2C | MPU6050 驱动 | 姿态感知 |

---

## 2. 仓库目录架构

本仓库**不复制上游源码**（上游在 `$DUMMY_ROOT`，默认 `~/Dummy-Robot`，可用 `export DUMMY_ROOT=/path/to/Dummy-Robot` 覆盖，见 `ENV.md`），只存放：
项目级文档、对上游的补丁、工具脚本、以及本项目独有产物。

```
zenix_Dummy/
├── README.md                 # 项目首页
├── ENV.md                    # 本机开发环境（编译/烧录/权限）
├── ARCHITECTURE.md           # 本文档
├── docs/                     # 项目文档
│   ├── requirements.md       # 需求与验收标准
│   ├── roadmap.md            # 任务与里程碑（T014656）
│   └── bom.md                # 采购清单与预算跟踪（目标 3500 元）
├── firmware/
│   └── patches/              # 对上游源码的兼容修改补丁（唯一事实来源）
├── software/
│   └── cli/                  # CLI-Tool 快捷命令与自动化脚本
├── hardware/
│   └── notes/                # 选型、接线、电控方案说明
├── models/                   # 3D 打印件模型 / 结构图
└── tools/                    # 构建/烧录/测试脚本
    ├── build.sh              # 一键编译两个固件
    ├── flash.sh              # 一键烧录（ST-Link）
    └── serial_test.sh        # 串口连通性自检
```

**与上游 Dummy-Robot 的映射**

| 本仓库目录 | 职责 | 对应上游目录 |
|---|---|---|
| `docs/` | 项目文档 | `5.Docs/` |
| `firmware/patches/` | 上游修改的补丁 | `2.Firmware/` |
| `software/cli/` | 上位机封装 | `3.Software/CLI-Tool/` |
| `hardware/notes/` | 硬件方案 | `1.Hardware/` |
| `models/` | 结构模型 | `4.Model/` |
| `tools/` | 构建/烧录自动化 | —（新增） |

---

## 3. 核心原则

1. **源码不重复**：上游固件/上位机源码一律放在 `$DUMMY_ROOT`（默认 `~/Dummy-Robot`），本仓库用 `patches/` 记录对它的改动。
2. **差异可追溯**：对上游的任何修改都导出为 `.patch` 放入 `firmware/patches/`，并在文档中注明原因（如 GCC 13 兼容）。
3. **按层解耦**：机械 / 驱动 / 主控 / 应用 各层独立演进，跨层只走定义好的协议。
4. **文档即事实**：需求、BOM、路线图、接线方案都在 `docs/` 下维护，作为开发与采购依据。

---

## 4. 开发工作流

```
改上游源码 ──► 导出补丁到 firmware/patches/
      │
      ▼
tools/build.sh 编译两个固件
      │
      ▼
tools/flash.sh 经 ST-Link 烧录
      │
      ▼
tools/serial_test.sh / CLI-Tool 联调验证
      │
      ▼
更新 docs/（roadmap 勾选里程碑、记录踩坑）
```

**上游获取**（首次）：

```bash
git clone https://github.com/peng-zhihui/Dummy-Robot.git ~/Dummy-Robot && cd ~/Dummy-Robot && git checkout 84453f1
# 自定义路径：export DUMMY_ROOT=/path/to/Dummy-Robot
```

**补丁导出命令**（在 `$DUMMY_ROOT` 中）：

```bash
git diff > $DUMMY_ROOT/../zenix_Dummy/firmware/patches/xxx-<描述>.patch
# 或在 zenix_Dummy 中：make patches-export
```

---

## 5. 规范约定

- **目录**：小写，语义化命名；一层一个职责。
- **文档**：Markdown，与代码同库维护；命名小写短横线（如 `bom.md`）。
- **脚本**：Bash，`set -euo pipefail`，支持 `-h` 帮助。
- **上游版本**：记录所用上游 commit（见 `docs/roadmap.md`），保证可复现。
