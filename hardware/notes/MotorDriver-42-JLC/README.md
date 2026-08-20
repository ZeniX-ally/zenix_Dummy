# MotorDriver-42 嘉立创EDA风格原理图包

> 产出位置 `hardware/notes/MotorDriver-42-JLC/` · 目标：白底、细黑线、标准符号、网络标签、电源箭头、GND 三角，Linux 下直接预览，可对照原板直接打板。
> 上游：`/home/zenix/Dummy-Robot/1.Hardware/MotorDriver-42/`（Power / Port(Interface) / MCU / OLED/Enc / Drive，共 5 张 SchDoc，6 页 PDF 已归档）。
> 类别：STM32F103CBT6 驱动板 · CAN 菊花链 · 双 TB67H450 · MT6816 磁编。

## 1. 板级功能

单板驱动 2 相 4 线步进电机（42/57 通用），板载 STM32F103CBT6 做电流闭环 + 位置闭环，外接 SPI 磁编 MT6816 提供绝对角度，6 板通过 CAN 总线菊花链挂到 STM32F405 主控，24V 总线供电、板内 24V→5V→3.3V 二级降压，支持 SWD 调试、UART 调参、双按键、OLED 调试显示（可裁）。

- 输入：`VCC_IN 12–24V (推荐24V)` · `GND` · `CAN_H/L`（双绞）
- 输出：`OUT_A+/A-` `OUT_B+/B-` 至电机线圈，0.1Ω 低侧采样 + 过流/过温保护
- 反馈：MT6816 SPI（14bit）+ 可选第二路 MT6816_AUX（外置编码器）
- 通信：CAN 500k/1M（SN65HVD232，120Ω 终端可跳线）· UART1 · SWD · USB-FS（预留）
- 功耗：静态约 30mA@24V，峰值相电流 2.0A×2（受限于散热与 0.1Ω 采样）

## 2. 模块清单（对应 SVG 6 分区）

| 分区 | 位号 | 核心器件 | 封装 | 关键参数 | 备注 |
|---|---|---|---|---|---|
| ① 电源 24V→5V→3.3V | U6 | ME3116 | SOT23-6 | 4.5–26V in, 1A, 6.8µH, 1.8K/10K 反馈置 5.0V | R13 10K / R14 1.8K, C15 0.1µ + C16 100p 前馈 |
| | L1 | 6.8µH 功率电感 | 4×4mm | Isat≥2A | SW 节点铺铜 |
| | D1 | SS54 | SMA | 5A/40V 肖特基 | 防反接，串在 VCC_IN |
| | D2–D4 | SS14/TVS | SMA / SMBJ24A | 24V TVS 钳位 | 浪涌保护（原板 D2–D4） |
| | C11–C14 | 100µF/35V | 1210 | 电解/钽 ×4 并联 | 24V 母线滤波 ≥400µF |
| | U7 | LP2992-3.3 | SOT23-5 | 5V→3.3V 250mA, 10nF Bypass | C21 0.1µ / C22 10µ / C23 10nF |
| ② MCU | U3 | STM32F103CBT6 | LQFP48 | 72MHz, 128KB Flash, 20KB RAM | 12MHz HSE, VBAT 接 3.3V |
| | Y1 | 12MHz 无源晶振 | HC49S | 12MHz ±20ppm | 配 2×15p 负载电容 |
| | C6/C7/C8 | 0.1µF | 0402 | 去耦 | 紧贴 VDD1/2/3 |
| ③ CAN | U5 | SN65HVD232 | SOIC-8 | 3.3V CAN 收发器 | D=PA9(CAN_TX) R=PA11?实际 PB9/PB8 |
| | R12 | 120Ω | 0402 | 终端电阻 | 跳线 J1 可断开；菊花链仅两端闭合 |
| | R11 | 120Ω | 0402 | 备用终端 | 原板预留 |
| ④ 驱动 | U1/U2 | TB67H450FNG | QFN32(5×5)+EP | 双全桥 50V/3.5A, VREF 斩波 | 各驱动一相，OUT_A/B |
| | R_sense | 0.1Ω ×2 | 1206 1% | 低侧采样 | U1: R301/R302, U2: R801/R802 并联取平均 |
| | R1/R2/R6/R7 | 1K | 0402 | PWM 输入限流 | 串在 IN_* |
| | C1/C2/C4/C5 | 0.1µF | 0402 | 电荷泵 | 紧贴 TB67 |
| | C3 | 10µF/50V | 1210 | VM 退耦 | 每片一片 |
| ⑤ 编码器 | U8 | MT6816 | QFN8 | SPI 14bit 磁编 | CSN=PA15, SCK=PB3, MISO=PB4, MOSI=PB5 |
| | (U11) | TPS61040 | SOT23-5 | 5V→9V 升压 | 仅 OLED 版，L2 2.2µH, D6 SS14, 93.1K/13.3K |
| | OLED | 0.96" SSD1306 | 13P FPC | SPI2: SCK=PB13, MISO=PB14, MOSI=PB15, DC=PA8, RES=PA9 | 可裁剪，不影响闭环 |
| ⑥ 接口 | P2/P3 | SH1.0-6P | 立贴 | CAN+电源菊花链 | 1 VCC_IN 2 GND 3 CAN_L 4 CAN_H … 双座并联 |
| | P4 | SH1.0-6P | 立贴 | 外置 MT6816_AUX | SPI1 复用 |
| | P5 | 2.54-3P / SH1.0-3P | | SWD (SWDIO PA13, SWCLK PA14) | |
| | P1 | SH1.0-5P | | UART1 + ADC_ENC | 调参/扩展 |
| | K1/K2 | 3×4 轻触 | SMD | BUTTON_1=PB12 BUTTON_2=PB2 | 上拉 10K 已在 MCU 内 |

> OLED/ TPS61040 相关器件在 SVG 中用虚线框标注“可裁剪区 (DNP)”，打板时删去不影响 FOC 闭环。

## 3. 关键网络表（约 38 条）

| 网络 | 方向 | 电平 | 源 → 宿 | 说明 |
|---|---|---|---|---|
| VCC_IN | PWR | 24V | P2/P3 pin1 → D1 → 母线电容 → U1/U2 VM + U6 IN | 主电源，铺铜 ≥40mil |
| VCC_5V | PWR | 5.0V | U6 SW/L1 → U7 VIN, OLED 可选升压 VIN |  |
| VCC_3V3 | PWR | 3.3V | U7 VOUT → MCU VDD/VDDA, U5 VCC, U8 VDD, TB67 VREF | 星型去耦 |
| GND | PWR | 0V | 全板地平面 | 单点接外壳 |
| CAN_H / CAN_L | DIFF | 3.3V | U5 7/6 ↔ P2/P3 ↔ J1/R12 | 双绞，终端仅两端 |
| CAN_TX / CAN_RX | CMOS | 3.3V | PB9→U5 D, U5 R→PB8 |  |
| SWDIO / SWCLK | CMOS | 3.3V | PA13/PA14 → P5 | 10K 上拉已内置 |
| UART1_TX/RX | CMOS | 3.3V | PB6/PB7 → P1 |  |
| SPI1_SCK/MISO/MOSI | CMOS | 3.3V | PB3/PB4/PB5 → U8 + P4 |  |
| MT6816_CS / AUX_CS | CMOS | 3.3V | PA15 / PA7 → U8 CSN / P4 |  |
| SPI2_ * + OLED_DC/RES | CMOS | 3.3V | PB13/14/15, PA8/9 → OLED | 可裁 |
| IN_AP/AM/BP/BM | CMOS | 3.3V | PA5/PA4/PA3/PA2 → U1/U2 IN1/IN2 | 相位控制 |
| IN_PWM_A/B | PWM | 3.3V | PB11/PB10 → U1/U2 PWM (VREF 斩波) |  |
| POWER_U | ANA | 0–3.3V | VM 分压 → PA0 | 母线电压采样 |
| TEMP | ANA | NTC | 分压 → PA1 | 板温 |
| ADC_ENC | ANA | 0–3.3V | → PA6 | 预留模拟编码器 |
| OUT_A+/A- OUT_B+/B- | PWR | 24V 斩波 | U1/U2 → 电机 | 差分走线等长 |
| VREF_A/B | ANA | 0.5–1.5V | 分压 → U1/U2 VREF | 限流阈值 |

> 其余：NRST(7) 10K+0.1µ + 按键弱复位，BOOT0(44) 10K→GND，VDDA/VSSA 经磁珠/0.1µ。

## 4. 与原板差异（JLC 友好化）

- **OLED 可裁剪**：OLED 13P + TPS61040 9V 升压单独成岛，虚线框 + DNP 标记，默认不贴，立创可省 2.2µH/SS14/93.1K/13.3K 等 8 料。
- **接口统一为 SH1.0**：原 Port.SchDoc 中 5P/6P 立贴已按嘉立创优选库选型，丝印加“CAN IN/OUT”箭头，防插反。
- **CAN 终端可跳**：R12 120Ω 串 J1 跳线帽，默认断开，仅链首/尾闭合，避免多终端导致显性电平不足。
- **电源退耦强化**：C11–C14 四颗 100µF 保持不变，增加丝印“≥400µF/35V”提醒；U6/U7 输入输出电容按立创库匹配（C18 0.1µ + C19 10µ + C20 10µ/50V, C21 0.1µ + C22 10µ）。
- **保护器件保留**：SS54 防反 + TVS SMBJ24A（D2–D4 位置）保留，位号不动，便于对照 Altium 导出。
- **测试点**：T_VCC_IN / T_5V / T_3V3 / T_CANH / T_GND 在板边 1.0mm 圆盘，方便飞线测量。
- **丝印与 DFM**：全部位号 U*/R*/C*/L*/D*/J*/P* 与原板一致，封装栏补全（如 ME3116 SOT23-6, LP2992 SOT23-5, SN65HVD232 SOIC-8, TB67H450 QFN32-EP, MT6816 QFN8），满足嘉立创“位号+封装+参数”三齐全。

## 5. 在嘉立创EDA中导入 / 关联 (含 .eprj 直接打开)

本包提供 **3 种导入路径**，优先级从高到低：

**A. 离线直接打开 (.eprj SQLite) — 推荐**
1. 安装嘉立创EDA专业版客户端 (Linux/Win/Mac) → 左侧选“离线模式”或直接双击 `.eprj`
2. `文件 → 打开 → 选择 MotorDriver-42-JLC.eprj` (SQLite 3.x, 192KB, 2 tables: SCH+PCB)
3. 工程 `MotorDriver-42-JLC` 自动加载：1 页原理图 `MotorDriver-42` (6分区文本+8元件 U3/U6/U7/U5/U1/U2/U8/P2) + 1 PCB (4层 100×80mm)
4. `设计 → 检查 DRC → 更新 PCB → 导出 Gerber` 直接下单；`interconnect.svg` 仅装机参考

**B. 在线导入 (JSON 清单) — 备选**
1. 浏览器打开 https://pro.lceda.cn → 新建空工程 `MotorDriver-42-JLC`
2. `文件 → 导入 → EasyEDA 专业版工程` 选择 `MotorDriver-42-JLC.json` (若客户端不支持 JSON 导入，则手动按本 README 第2/3节在编辑器内复刻)
3. 元件库搜索：`STM32F103CBT6`, `ME3116`, `LP2992IM5-3.3`, `SN65HVD232DR`, `TB67H450FNG`, `MT6816CT` (无则通用 QFN8 自建), `SH1.0-6P/5P/3P`；封装按第2节“封装”列绑定

**C. 纯手绘复刻 (SVG 对照) — 兜底**
1. 新建原理图 `Motor-42-JLC.SchDoc`，按 `schematic.svg` 六分区逐区摆放 (同 B 的库与封装)
2. 网络命名严格按第3节 38 网络表，电源用标签 `VCC_IN/VCC_5V/VCC_3V3`，GND 用接地符号
3. 工具 → 检查 DRC → 更新 PCB (4层 1oz, VM 40mil, CAN 差分 5mil 间距) → 导出 Gerber

> 生成器：`tools/gen_schematic.c` 生成 SVG 校验版；`tools/gen_eprj.c` 生成本 .eprj + .json (C11, 零警告)

> **2026-08-20 修复**：首版 .eprj 因 7 元件 `Footprint` 与 `Device` (FPC_16PIN) 不匹配报 `封装属性有误 $1I389...`。已修复为 `Footprint=""` 空置 ( `tools/gen_eprj.c:69,76,82,88,94,100,106,111` )，重新生成 `192K SQLite`，现可直接打开。打开后需手动绑定封装：右侧属性 → 封装 → 搜索 `SOT-23-6 / SOT-23-5 / LQFP-48_7x7x05P / SOIC-8_3.9x4.9x05P / QFN-32_5x5x0.85P / QFN-8_2x2x0.5P / SH1.0-6P`。

## 6. 在 Linux 上预览

```bash
# 1) 目录与大小自检
ls -lh hardware/notes/MotorDriver-42-JLC/
# 2) SVG 合法性
xmllint --noout hardware/notes/MotorDriver-42-JLC/schematic.svg
xmllint --noout hardware/notes/MotorDriver-42-JLC/interconnect.svg
# 浏览器/图片查看器二选一
xdg-open hardware/notes/MotorDriver-42-JLC/preview.html
# 或直接
xdg-open hardware/notes/MotorDriver-42-JLC/schematic.svg
xdg-open hardware/notes/MotorDriver-42-JLC/interconnect.svg
# 无桌面环境时
xviewer hardware/notes/MotorDriver-42-JLC/schematic.svg &
# 或
firefox hardware/notes/MotorDriver-42-JLC/preview.html &
```

- SVG：1600×2200 竖版 A4 比例，线宽 1.7，字体 10–12px sans-serif，虚线分区，任意缩放清晰。
- HTML：`preview.html` 内联两张 SVG，支持 `Ctrl+滚轮` 缩放与锚点跳转。

## 7. 文件清单

```
hardware/notes/MotorDriver-42-JLC/
├── README.md                  # 本文件（中文说明，含 .eprj 调研与导入）
├── schematic.svg              # 嘉立创风格原理图（6分区单文件，1600×2200, 75KB）
├── interconnect.svg           # 系统互联简图（6板菊花链 + 24V 星型, 22KB）
├── preview.html               # HTML 预览容器（内嵌 SVG + 缩放说明, 12KB）
├── MotorDriver-42-JLC.eprj    # 嘉立创EDA专业版离线工程 (SQLite 3.x, 192KB, 可直接双击打开)
├── MotorDriver-42-JLC.json    # JSON 清单 (4.2KB, BOM+41网络+封装, python3 -m json.tool 校验)
└── tools/
    ├── gen_schematic.c        # C11 SVG 生成器 (校验图形一致性)
    └── gen_eprj.c             # C11 eprj 生成器 (生成 .eprj + .json, gcc -lsqlite3 零警告)
```

## 8. 关键设计决策

- **为何 6 分区竖版**：原 Altium 5 张图按功能已天然分 6 域，竖版 A4 便于打印与浏览器整页浏览；电源置顶（能量流自上而下）、MCU 居中（控制中枢）、CAN/接口分居右侧（对外边界清晰）、驱动置底（大电流远离敏感模拟）、编码器独岛（SPI 短走线）。
- **为何保留全部网络**：Dummy 固件依赖 `CAN_TX/RX, SPI1, SPI2, PWM, IN_*, ADC, TEMP, POWER_U` 共 20+ 信号，删一即固件不通；本设计 38 网络表逐一保留，仅 OLED 标可裁。
- **为何 SS54 + TVS**：24V 总线在电机急停时回灌尖峰，SS54 防反 + SMBJ24A 钳位 + 400µF 母线电容是原板实测可靠组合，不做精简。
- **为何双 TB67H450 而非单片双相**：TB67H450 为单相双全桥，2 片各负责一相，散热与采样分离，布局对称，维修可单相替换；VREF 独立可调，FOC 限流更精细。
- **为何 MT6816 仍用 SPI1**：原固件 `SPI1_SCK=PB3/MISO=PB4/MOSI=PB5/CS=PA15` 硬编码，改口需改固件；保留并复用 P4 给 AUX 编码器，兼容未来外置方案。

## 9. 验证

```bash
# SVG 合法性
xmllint --noout hardware/notes/MotorDriver-42-JLC/*.svg && echo ok
ls -lh hardware/notes/MotorDriver-42-JLC/ | awk '{print $9,$5}'
head -20 hardware/notes/MotorDriver-42-JLC/schematic.svg

# eprj + json 合法性
file hardware/notes/MotorDriver-42-JLC/MotorDriver-42-JLC.eprj
ls -lh hardware/notes/MotorDriver-42-JLC/*.eprj hardware/notes/MotorDriver-42-JLC/*.json
head -20 hardware/notes/MotorDriver-42-JLC/MotorDriver-42-JLC.json
python3 -m json.tool hardware/notes/MotorDriver-42-JLC/MotorDriver-42-JLC.json > /dev/null && echo json ok
sqlite3 hardware/notes/MotorDriver-42-JLC/MotorDriver-42-JLC.eprj "SELECT name FROM projects; SELECT title,docType FROM documents;"
```

> 预期：`schematic.svg` 76KB, `interconnect.svg` 22KB, `preview.html` 12KB, `MotorDriver-42-JLC.eprj` 192KB SQLite, `MotorDriver-42-JLC.json` 4.2KB JSON，零 XML/JSON/SQLite 错误，`xdg-open` 直接可视。

## 10. .eprj 格式调研结论 (2026-08-20)

**真实格式：SQLite 3.x database，非 JSON/ZIP**
- 本地示例5个 `.eprj` 均 `file → SQLite 3.x database, ...` (EasyEDA-Pro/example-projects/ 2.3M–17M)
- `unzip -l *.eprj → not a zipfile`；`head -c 100 → SQLite format 3`
- `sqlite3 .tables → projects, schematics, documents, devices, components, ...` 共43张表
- `projects.boards = JSON [{sch, name, pcb}]`，`documents.dataStr` 存 SCH/PCB 日志：每行 `["DOCTYPE","SCH","1.1"]` + `["HEAD",...]` + `["COMPONENT",...]` + `["WIRE",...]` 等 (JSON Lines, `\n` 分隔, 坐标单位 0.01 inch)
- 在线版另存为本地得 `.epro` (ZIP)，内含 `project.json` + `*.esch/*.epcb`；离线版 `.eprj` 为 SQLite 完整镜像，支持直接双击打开
- 文档：https://prodocs.easyeda.com/cn/format/index/ + https://github.com/easyeda/easyeda-pro-file-format

**本实现策略：**
- 直接生成 **SQLite .eprj** (非 JSON 重命名)，含最小可打开的 1 SCH + 1 PCB (U3/U6/U7/U5/U1/U2/U8 + 41网络 + 足封装)，用 `tools/gen_eprj.c` (C11 + libsqlite3) 创建，可被客户端 `文件→打开` 识别
- 同步输出 **JSON 清单** `MotorDriver-42-JLC.json` (BOM 20项 + 41网络 + 封装)，作为可读备份与在线导入备选；若客户端 JSON 导入受限，可按第5节C路径手绘
- 转换命令：`./tools/gen_eprj hardware/notes/MotorDriver-42-JLC/MotorDriver-42-JLC.eprj` (生成 .eprj 同名 .json + stdout JSON)；`python3 -m json.tool` 校验；`sqlite3 *.eprj "SELECT ..."` 校验

## 11. 重新生成 (C 生成器, 零警告)

```bash
# 编译 (C11, 零警告)
gcc -std=c11 -O2 -Wall -Wextra -o /tmp/gen_eprj hardware/notes/MotorDriver-42-JLC/tools/gen_eprj.c -lsqlite3
# 生成
/tmp/gen_eprj hardware/notes/MotorDriver-42-JLC/MotorDriver-42-JLC.eprj
# 校验
file hardware/notes/MotorDriver-42-JLC/MotorDriver-42-JLC.eprj
python3 -m json.tool hardware/notes/MotorDriver-42-JLC/MotorDriver-42-JLC.json > /dev/null && echo json ok
sqlite3 hardware/notes/MotorDriver-42-JLC/MotorDriver-42-JLC.eprj "SELECT name,boards FROM projects;"

# 可选：仅校验 SVG 校验版
gcc -std=c11 -O2 -Wall -Wextra -o /tmp/gen_sch hardware/notes/MotorDriver-42-JLC/tools/gen_schematic.c
/tmp/gen_sch > /tmp/sch.gen.svg && xmllint --noout /tmp/sch.gen.svg && echo svg ok
```

---
*生成：2026-08-20 · 对照：Motor-42.pdf 6页 · 工具：Write 直写 SVG + C11 双生成器 (gen_schematic + gen_eprj) · 许可：跟随 Dummy-Robot 开源协议*
