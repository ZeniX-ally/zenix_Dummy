# 扩展指南（Extending Guide）

本文说明如何在当前 `src/` 代码库上做**低侵入、可追溯**的扩展。
所有能力点都对应一组可直接调用的 API，且被单元测试（`make ctest`）覆盖。

## 0. 代码组织与依赖

```
src/
├── common.h      公共常量 + 纯数学工具（角度换算/环绕/钳位），header-only
├── kinematics.*  运动学：DH 参数(FK/IK 单一来源)、8 分支逆解、限位、统一选解
├── protocol.*    通信协议：帧打包/流式解析、边界安全序列化流、命令登记表
├── trajectory.*  轨迹：梯形廓线、关节 P2P、笛卡尔直线 + 连续 IK
└── main.c        单元测试（当前 10 组，make run / make ctest）
```

依赖方向：`trajectory -> kinematics -> common`；`protocol -> kinematics -> common`。
新模块只需 `#include` 需要的上层头文件，并自然被 `src/Makefile` 的 `wildcard` 收集。

## 1. 换一台机械臂（改 DH 参数 / 关节限位）

**改几何参数**（编译期）
- 连杆长度宏：`src/kinematics.h` 顶部的 `DUMMY_L_*` 系列。
- 运行时：填好 `ik_config_t` 传入 FK/IK 即可，无需改源码。

**改关节软限位**（运行时，推荐）
```c
joint_limits_t lim = kinematics_get_default_limits();
lim.max_deg[2] = 170.0f;              // 例如放开 J3 上限
kinematics_set_limits(&lim);           // 全局生效，免改源码
```
- `is_joint_within_limits()` 与 `clamp_joint_angles()` 都遵循当前限位。
- **DH 参数表只有一份**：`kinematics.c` 的 `build_dh_table()`，FK/IK 共用，
  改机械结构只需动这一个函数。

## 2. 新增一条协议命令

命令登记表（`protocol.c` 的 `PROTOCOL_CMD_TABLE`）是协议扩展点，三步接入：

1. **枚举**：在 `protocol.h` 的 `protocol_cmd_t` 增加命令 ID。
2. **实现解包钩子**：签名 `int (*)(const protocol_frame_t *, void *)`，
   内部可用序列化流 `proto_reader_*` 安全读取；若暂时不提供置 `NULL`。
3. **登记**：在表里加一行 `{ CMD_X, "name", min_payload, handler }`。

之后 `protocol_unpack_command(&frame, &out)` 会自动分发到新命令。
返回码约定：`0` 成功；`-1` 未登记；`-2` 载荷过短；`-3` 已登记但未实现钩子。

> 反序列化请用 `proto_reader_t`，打包请用 `proto_writer_t`
> （自带边界检查，payload 结构演化不会越界踩内存）。

## 3. 改变某个命令的 payload 结构

用序列化流（`proto_writer_*` / `proto_reader_*`）读写字段：

- 字段以各自 `*_init` 起的游标顺序编解码，新的老版本协议均可校验 `min_payload`。
- **大端 u16** 与 float 均有独占 API；读写越界会返回 `-1` 而不是越界访问。

## 4. 新增一种"多解择优"策略

在 `kinematics.h` 的 `solution_selector_t` 增加枚举值，
再到 `kinematics.c` 的 `kinematics_select_solution()` 成本分支里加一种算法即可。
`kinematics_inverse_6dof()` 与 `traj_ik_closest()` 已共用这一入口，
新策略对两者同时生效。

## 5. 新增一种轨迹类型

照 `trajectory.*` 现有模式添加：

- 头文件里定义 `traj_xxx_plan_t` 计划结构 + `init/sample` 两个接口；
- 实现：`init` 用 `traj_profile_make()` 构造廓线，`sample` 用 `traj_profile_sample()`
  采样位移并按比例插值；
- 需要保持 IK 连续性时，复用 `traj_ik_closest()` / `kinematics_select_solution()`。

## 6. 新增一个独立模块

1. 新增 `mymod.c / mymod.h`，`#include` 所需的上层头文件；
2. 在 `main.c` 里加测试函数并挂进 `main()`（见第 7 节）；
3. **不需要改 Makefile**：`wildcard` 已自动收集全部 `.c/.h`。

## 7. 测试与验证

```bash
make check        # 环境自检
make ctest        # 编译并运行 src/ 全部单元测试（等价 cd src && make run）
```

新增功能请遵循一个惯例：**每个扩展点配一个测试用例**（如新增命令后，
在 `main.c` 补充打包→分发→断言的用例），保证 `ALL 10 TEST SUITES PASSED`
始终成立。

## 8. 编码约定

- 纯 C（C11），兼容 C++ 头文件（`extern "C"`）。
- 工具函数放 `common.h`（`static inline`，不引额外符号）。
- 角度统一"度(deg)"，换算走 `zenix_deg_to_rad / zenix_rad_to_deg`。
- 公开函数自带文档注释；新类型冒泡到对应 `.h`。
- 编译保持 `-Wall -Wextra -pedantic` 无警告，描述性 `git commit` 提交信息（如 `feat: ...`）。
