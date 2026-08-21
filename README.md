# zenix_Dummy

机械臂项目：模仿稚晖君 Dummy-Robot。

## 目标

- 6 轴桌面机械臂
- 能运动 + 末端夹爪抓取
- 预算约 3500 元

## 技术路线

- 结构：淘宝代打 3D 件 / 亚克力，不用 CNC
- 减速器：国产谐波减速器
- 电机：42/35 步进电机
- 控制：Dummy 开源固件 + STM32 REF 主控 + 电机驱动板

## 参考

- 官方仓库：https://github.com/peng-zhihui/Dummy-Robot
- 本地源码：`$DUMMY_ROOT`（默认 `~/Dummy-Robot`，可用 `export DUMMY_ROOT=/path/to/Dummy-Robot` 覆盖，见 `ENV.md`）
- 上游基线：`84453f1`（见 `docs/roadmap.md`）
- 任务：T014656

> 首次使用：`git clone https://github.com/peng-zhihui/Dummy-Robot.git ~/Dummy-Robot && cd ~/Dummy-Robot && git checkout 84453f1`
> 或自定义路径：`git clone https://github.com/peng-zhihui/Dummy-Robot.git /path/to/Dummy-Robot && export DUMMY_ROOT=/path/to/Dummy-Robot`

## 文档与架构

- [ARCHITECTURE.md](ARCHITECTURE.md) — 系统/仓库架构总览（开发必读）
- [ENV.md](ENV.md) — 本机开发环境
- [docs/requirements.md](docs/requirements.md) — 需求规格
- [docs/roadmap.md](docs/roadmap.md) — 任务与里程碑
- [docs/bom.md](docs/bom.md) — 采购清单与预算跟踪
- [docs/troubleshooting.md](docs/troubleshooting.md) — 排错手册
- [docs/extending.md](docs/extending.md) — 代码扩展指南（换机型/加命令/加模块…）

## 常用命令

统一入口（`make help` 查看全部）：

| 命令 | 作用 |
|---|---|
| `make check` | 环境自检（工具链/上游/串口权限） |
| `make build` | 一键编译两个固件 |
| `make flash` | ST-Link 烧录固件 |
| `make serial` | 串口连通性自检 |
| `make patches-export` | 导出上游修改为补丁 |
| `make patches-apply` | 应用补丁到上游 |
| `make clean` | 清理构建产物 |
| `make ctest` | 编译并运行基础 C 程序（src/） |

## 基础 C 程序

位于 `src/`，是一个桌面机械臂运动学/通信/轨迹基础示例（与项目主题贴合）：

- `src/common.h`：跨模块公共常量与纯数学工具（角度换算/环绕/钳位）
- `src/kinematics.h` / `src/kinematics.c`：6-DOF 正运动学、8 分支解析逆运动学（含奇异点保护）、**运行时可配置关节限位**、**统一多解择优**（默认/最近/可扩展）
- `src/protocol.h` / `src/protocol.c`：二进制帧打包/解包（Sum8/CRC16 校验）、流式状态机解析器、**边界安全序列化流**、**命令登记表与分发中心**（加命令只需登记一行）、ASCII 命令编码
- `src/trajectory.h` / `src/trajectory.c`：梯形速度廓线、关节空间点对点同步运动、笛卡尔直线运动 + 连续 IK 分支跟踪
- `src/main.c`：10 组单元测试（FK/IK 闭环、多解/奇异、协议校验、流式解析、ASCII、廓线、P2P、直线+IK、扩展 API）

编译运行：

```bash
make ctest          # 等价 cd src && make run
# 或直接
cd src && make run
```

> 扩展方法（换机型限位、新增协议命令、新增轨迹类型、新增模块等）见 [docs/extending.md](docs/extending.md)。

## 目录结构

```
docs/       项目文档（需求/路线图/BOM/排错）
src/        基础 C 程序（运动学示例）
firmware/   对上游源码的补丁
software/   上位机封装
hardware/   硬件方案
models/     3D 模型
tools/      构建/烧录/测试/补丁脚本
```
