# MotorDriver-42 Altium 原工程导入嘉立创EDA专业版 · 完整操作指南

> 替代之前占位的 `MotorDriver-42-JLC.eprj` (FPC_16PIN 占位，已废弃)。
> 本指南在 **Linux Mint 22.3 Cinnamon + 嘉立创EDA专业版 3.2.149** 实测路径下编写，无需再生成占位 .eprj。

- 原工程根：`/home/zenix/Dummy-Robot/1.Hardware/MotorDriver-42/`
- 目标：在嘉立创EDA专业版中得到与 Altium 完全一致的 5 张原理图 + 1 张 PCB，之后本地保存为真正的 `.eprj` 或直接导出 Gerber 下单
- 对照：本目录下 `schematic.svg` / `interconnect.svg` / `preview.html` 仅作导入后人工核对用，非导入源

---

## 0. 前置检查（必做）

### 0.1 客户端版本与安装路径

| 项 | 值 |
|---|---|
| 已安装版本 | **3.2.149.88089769** (Electron 36.3.1 / Chrome 136) |
| Linux 可执行 | `/home/zenix/.local/opt/easyeda-pro/easyeda-pro` |
| 桌面入口 | `~/.local/share/applications/easyeda-pro.desktop` (`Exec=... --no-sandbox --gtk-version=3 %F`) |
| 启动方式 | 终端 ` /home/zenix/.local/opt/easyeda-pro/easyeda-pro &` 或开始菜单搜“嘉立创EDA专业版” |
| 最低建议 | ≥ 2.2.x，低于 3.0 的离线 SQLite 结构不兼容，需升级 |

> 版本校验：
> ```bash
> /home/zenix/.local/opt/easyeda-pro/easyeda-pro --version
> cat /home/zenix/.local/opt/easyeda-pro/resources/app/package.json | grep version
> ```

### 0.2 离线 vs 在线

| 模式 | 入口 | 导入结果 | 适用 |
|---|---|---|---|
| **离线导入（推荐）** | 客户端 → `文件 → 导入 → Altium Designer ...` | 本地生成完整 SQLite `.eprj`，无云端依赖，可直接 `文件 → 另存为本地工程` | Linux 本地已有全套 Altium 源文件，网络不稳定 |
| 在线导入 | 浏览器 `https://pro.lceda.cn` → 新建工程 → `文件 → 导入` | 先上云，成功后再 `文件 → 另存为 → 本地工程 (.eprj)` 下载 | 临时电脑未装客户端 |

Linux 下一律用离线客户端，路径与权限最可控。

### 0.3 备份原文件（1 分钟）

```bash
SRC="/home/zenix/Dummy-Robot/1.Hardware/MotorDriver-42"
BAK="$HOME/MotorDriver-42-backup-$(date +%Y%m%d)"
mkdir -p "$BAK" && cp -a "$SRC" "$BAK"/ && ls -lh "$BAK/MotorDriver-42"
# 可选：打 zip 供导入向导直接选中（见步骤1备选）
cd /home/zenix/Dummy-Robot/1.Hardware && zip -r /tmp/MotorDriver-42.zip MotorDriver-42
ls -lh /tmp/MotorDriver-42.zip
```

> 不要在原目录内直接改动 `.SchDoc` / `.PcbDoc`，它们是 OLE Compound 二进制（`D0 CF 11 E0`），版本不兼容时会静默损坏。

### 0.4 原工程清单确认（导入前核对）

```bash
ls -lh "/home/zenix/Dummy-Robot/1.Hardware/MotorDriver-42"
# 预期（2026-08-20 实测，与 check_altium_import.c 输出一致）：
# -rw-rw-r--  83K  Power.SchDoc      5e...  abda5b50ed75d4314d0866820bf086bb
# -rw-rw-r--  94K  MCU.SchDoc        afbdec9b0fd45ad33ea0b0761bff72ad
# -rw-rw-r--  66K  Port.SchDoc       5e37b95ac3aa31332f65105d28dffbca
# -rw-rw-r--  66K  Interface.SchDoc  0f6a120fc7e0c87eb1e4278543c5e174
# -rw-rw-r--  59K  Drive.SchDoc      5e0f79a8e5f232d51c81779d02808e89
# -rw-rw-r--  49K  Motor-42.PrjPCB   71ee436d59c97bbd79f420841016853b
# -rw-rw-r--  421  Motor-42.PrjPCBStructure
# -rw-rw-r-- 5.4M  Motor-42.PcbDoc   a7c42448a967938cf96db674df9dd110
# -rw-rw-r-- 229K  Motor-42.pdf      497207685e29646086888ee108227d6d  (6 页)
# drwxrwxr-x  Gerber/  28 文件 ~992K

# 快速校验（C 工具，见第6节）：
gcc -std=c11 -O2 -Wall -Wextra -o /tmp/check_altium_import tools/check_altium_import.c
/tmp/check_altium_import
```

| 文件 | 作用 | 导入入口 |
|---|---|---|
| `Motor-42.PrjPCB` | 工程索引，列出 5 张 SchDoc 的 `DocumentPath` | **必须选中此文件**（非单张 SchDoc） |
| `Power.SchDoc` | 电源：SS54 + 4×100uF + ME3116(5V) + LP2992(3.3V) + 6.8uH | 自动跟随 PrjPCB 导入 |
| `MCU.SchDoc` | MCU：STM32F103CBT6 LQFP48 + 12MHz | 同上 |
| `Port.SchDoc` | 接口：SN65HVD232 + SH1.0-6P×2 + 120R + 按键 + SWD | 同上 |
| `Interface.SchDoc` | OLED/编码器：MT6816 + TPS61040 + OLED-12880 | 同上（注意：文件名是 Interface，非 OLED） |
| `Drive.SchDoc` | 驱动：2×TB67H450FNG + 0.1R 采样 + NTC | 同上，PrjPCB 中为 TopLevelDocument |
| `Motor-42.PcbDoc` | PCB：5.4M 二进制，4 层 | 同 PrjPCB 一并导入 |
| `Motor-42.pdf` | 6 页 SmartPDF：P1 电源 / P2 接口(CAN) / P3 MCU / P4 编码器OLED / P5 驱动×2 / P6 叠层报告 | 导入后人工逐页对照 |
| `Gerber/` | 28 文件，已可直接下单（GG1/G1/G2/GTO/GBL 等） | 导入后与导出 Gerber 二次比对 |

> `Motor-42.PrjPCBStructure` 内容（唯一层次）：
> ```
> Record=TopLevelDocument|FileName=Drive.SchDoc|SheetNumber=1
> Record=NoMainPathDocument|SourceDocument=Drive.SchDoc|FileName=Interface.SchDoc|SheetNumber=1
> Record=NoMainPathDocument|SourceDocument=Drive.SchDoc|FileName=MCU.SchDoc|SheetNumber=1
> Record=NoMainPathDocument|SourceDocument=Drive.SchDoc|FileName=Port.SchDoc|SheetNumber=1
> Record=NoMainPathDocument|SourceDocument=Drive.SchDoc|FileName=Power.SchDoc|SheetNumber=1
> ```
> 导入后在嘉立创工程树中应看到 5 张原理图平级（非层次化），顺序与上表一致。

---

## 1. 步骤1 — 发起导入

### 1.1 正常路径（选 PrjPCB）

1. 启动嘉立创EDA专业版：
   ```bash
   /home/zenix/.local/opt/easyeda-pro/easyeda-pro --no-sandbox --gtk-version=3 &
   ```
   或 Cinnamon 菜单 → 搜索“嘉立创EDA专业版” → 点击图标。
2. 登录（离线导入也建议先登录一次以同步库，之后可离线）。
3. 顶部菜单：**文件 → 导入 → Altium Designer…**（英文版 *File → Import → Altium…*）。
4. 文件选择框中，导航至：
   ```
   /home/zenix/Dummy-Robot/1.Hardware/MotorDriver-42/
   ```
   选中 **`Motor-42.PrjPCB`** → 打开。
   - Linux 路径栏可直接粘贴：`Ctrl+L` 切路径输入 → 粘贴上述路径 → 回车。
   - 若对话框默认过滤 `*.PrjPcb` 未显示，切换过滤器为 `All Files` 或 `*.PrjPCB`。

### 1.2 备选路径（打 zip 再导入）

若客户端提示“缺少关联的 SchDoc/PcbDoc”或在沙盒中无法跨目录读取：

```bash
cd /home/zenix/Dummy-Robot/1.Hardware
zip -r /tmp/MotorDriver-42.zip MotorDriver-42
# 结果 /tmp/MotorDriver-42.zip 内含 PrjPCB + 5 SchDoc + PcbDoc
```

再在导入对话框中选择 **`/tmp/MotorDriver-42.zip`**。客户端会自动解压并识别 `Motor-42.PrjPCB` 为入口。

> 不要单独只选某一张 `.SchDoc`；必须以 `.PrjPCB` 或含 PrjPCB 的 zip 为入口，否则只导入单页。

---

## 2. 步骤2 — 导入向导选项

导入后弹出向导，按以下选择（每项均有原因）：

### 2.1 文件映射确认

| 向导项 | 应显示 | 操作 |
|---|---|---|
| 检测到的原理图 | 5 张：`Drive.SchDoc` / `Interface.SchDoc` / `MCU.SchDoc` / `Port.SchDoc` / `Power.SchDoc` | 全部勾选 |
| 检测到的 PCB | `Motor-42.PcbDoc` (5.4M) | 勾选 |
| 工程名 | `Motor-42` 或 `MotorDriver-42` | 可改为 `MotorDriver-42-JLC` |
| 输出位置 | 本地目录，如 `~/Documents/EasyEDA/MotorDriver-42-JLC/` | 记下路径，后续导出 Gerber 在此 |

若只显示 1 张或 0 张，点“添加文件”手动把 5 张补齐，或改用 zip 路径重来。

### 2.2 编码（中文乱码关键）

- **原理图字符编码**：选 **`GB2312 / GBK / CP936`**（三者等价，Altium 中文注释常用）。
  - 选 UTF-8 会导致作者“彭志辉”及元件注释出现 `ÅíÖÉêÍ` 类乱码（见 `pdfinfo Author`）。
  - 若导入后仍乱码：关闭工程 → 重新导入 → 仅切换此项为 GBK → 不改其他。
- **PCB 编码**：保持默认 `UTF-8`。

### 2.3 单位与网格

| 项 | 建议 | 原因 |
|---|---|---|
| 原理图单位 | `mil`（或保持原 `mil`） | Altium 原工程默认 mil，网络标签对齐依赖此 |
| PCB 单位 | `mil` 或 `mm`（导入后可切换，`视图 → 切换单位`） | 推荐先 `mil`，便于核对 40mil/5mil 规则 |
| 网格 | 默认 10mil / 1mm | 不影响导入，导入后按需改 `属性 → 网格` |

### 2.4 其他勾选

- `导入封装` / `Import Footprints`：**勾选**
- `导入 3D 模型`：可选（本板无关键 3D，勾不勾均可）
- `创建新工程` / `覆盖`：选“创建新工程”，避免污染已有工程

点 **开始导入 / Import**，等待进度条（PcbDoc 5.4M 需 20–60 秒，取决于硬盘）。

---

## 3. 步骤3 — 库映射（最耗时，需手动）

Altium 库与嘉立创立创商城库并非同名，导入后会提示“未找到库/封装”。按此表在**封装管理器 / 器件管理器**中批量替换。

### 3.1 核心器件映射表（对照本目录 `README.md` §2 BOM 封装列）

| 位号 | 原 Altium `LibReference` | 原 `Comment` | 嘉立创库搜索关键词 | 推荐立创封装 | 立创优选 | 备注 |
|---|---|---|---|---|---|---|
| **U6** | `ME3116` | ME3116 | `ME3116` 或 `ME3116AM6G` | `SOT-23-6` | 是 | DCDC 4.5–26V→5V，L1 6.8uH 4×4mm |
| **U7** | `LP2992` | LP2992 3V3 | `LP2992IM5-3.3` / `LP2992AIM5-3.3` | `SOT-23-5` | 是 | 5V→3.3V 250mA，C21 0.1u + C22 10u |
| **U3** | `STM32F103CBT6` | STM32F103CBT6 | `STM32F103CBT6` | `LQFP-48_7x7x05P` | 是 | 锁库存，替代 `STM32F103C8T6` 不可用（64KB 不够） |
| **Y1** | (晶振) | 12MHz | `12MHz HC49S` 或 `12.000MHz` | `HC49S` | 是 | 负载 15p×2 |
| **U5** | `SN65HVD232D` | SN65HVD232D | `SN65HVD232DR` / `SN65HVD230DR` (兼容) | `SOIC-8_3.9x4.9x05P` | 是 | 3.3V CAN，120R 需外接 R12 |
| **U1/U2** | `TB67H450FNG` | TB67H450FNG | `TB67H450FNG` 或 `TB67H450` | `QFN-32_5x5x0.85P-EP` (需带露铜) | 否（嘉立创需自建） | 若搜不到，用 `QFN-32` 通用封装+手动加散热焊盘 |
| **U8** | `MT6816` | MT6816 | `MT6816CT` | `QFN-8_2x2x0.5P` | 否 | 无现成则用通用 QFN8 自建，SPI: PA15/PB3-5 |
| **D1** | `Diode_SMA` | SS54 | `SS54` | `SMA (DO-214AC)` | 是 | 5A/40V 防反 |
| **D2–D4** | `SMBJ28CA` | SMBJ24A 实际 | `SMBJ24A` | `SMB (DO-214AA)` | 是 | 原 `SMBJ28CA` 丝印偏高，用 24V 更贴 24V 母线 |
| **C11–C14** | `201612` 等 | 100uF/35V | `100uF 35V` | `1210` (3528) | 是 | ≥400uF 母线，四颗并联 |
| **R12/R11** | `-0402` | 120R | `120R 1%` | `0402` | 是 | 终端，J1 跳线 |
| **Rs** | `-1206` | 0.1R | `0.1R 1% 1W` | `1206` | 是 | 低侧采样 R301/R302 并联 |
| **P2/P3** | `SH1.0-6P` | SH1.0-6P | `SH1.0-6P` / `B6B-SH-1.0` | `SH1.0-6P_卧贴/立贴` | 是 | 菊花链 CAN+电源 |
| **P4** | `SH1.0-6P` | 同上 | `SH1.0-6P` | 同上 | 是 | AUX 编码器 SPI1 复用 |
| **P5** | `SH1.0-3P` | SH1.0-3P | `SH1.0-3P` | `SH1.0-3P` | 是 | SWD PA13/14 |
| **P1** | `SH1.0-5P` | SH1.0-5P | `SH1.0-5P` | `SH1.0-5P` | 是 | UART1 PB6/7 + ADC_ENC PA6 |
| **K1/K2** | `ButtonTACT_3x4x2_180` | 轻触 | `3x4 轻触` | `3x4 SMD` | 是 | PB12/PB2 |
| **L1** | 6.8uH 功率电感 | 6.8uH | `6.8uH 功率电感 Isat 2A` | `4x4mm` | 是 | SW 节点大电流 |
| **NTC** | `NCP15XH103F03RC` | NTC 10K | `NCP15XH103F03RC` | `0402` | 是 | 温度 PA1 |
| **OLED** | `OLED-12880` | OLED | `SSD1306 0.96` | `FPC-13P` | 否 | 可裁剪，DNP |

### 3.2 封装替换操作（批量）

1. 导入后若报 `封装未找到 / Footprint not found`：顶部 **设计 → 封装管理器**（或左侧工程树右键任一器件 → `编辑封装`）。
2. 筛选 `Footprint == 空` 或 `FPC_16PIN`（占位遗留）的器件 → 全选。
3. 右侧 **搜索封装库**：粘贴上表“嘉立创库搜索关键词” → 选中正确封装 → **应用**。
4. 对 TB67H450 / MT6816 等无立创封装的：点 **新建封装 → 向导**，按数据手册焊盘尺寸手建（QFN32 0.5mm pitch，中心 3.5×3.5mm 露铜 + 过孔）。
5. 完成后 **工具 → 更新 PCB**（或 `设计 → 导入变更`）→ 全部勾选 → 执行。

> 技巧：嘉立创封装名常带尺寸，如 `SOT-23-6_2.9x1.6x1.45P`、`LQFP-48_7x7x05P`，搜前缀 `SOT-23-6` / `LQFP-48` 即可命中。

### 3.3 DNP / 可裁剪区

- `TPS61040DBV` (U11) + `L2 2.2uH` + `D6 SS14` + `93.1K/13.3K` + OLED 13P 为 **调试显示专用**，在嘉立创 BOM 中设为 `DNP (不贴)`，下单时勾选“不贴”可省 8 料。
- 本目录 `schematic.svg` 中虚线框即 DNP 区，导入后建议在原理图加文字 `DNP_仅调试`。

---

## 4. 步骤4 — 导入后检查（逐项打勾）

### 4.1 原理图 6 页是否完整

Altium 导出 `Motor-42.pdf` 为 6 页（实 5 张原理图 + 1 张叠层报告）。在嘉立创中应看到 5 张原理图页（若把叠层算作第 6 页，部分版本会单独生成 PCB 打印页）。

| PDF 页 | 对应 SchDoc | 特征元件 | 在嘉立创中检查点 |
|---|---|---|---|
| P1 | `Power.SchDoc` | D1 SS54, C11–C14 100uF×4, U6 ME3116, U7 LP2992, L1 6.8uH | VCC_IN → 5V → 3.3V 两级降压完整，C18 0.1u / C19 10u / C20 10u 均在 |
| P2 | `Port.SchDoc` | U5 SN65HVD232, R12 120R, P2/P3 SH1.0-6P, K1/K2 | CAN_H/L 经 R11/R12 到 P2/P3，SWD P5 (PA13/14)，UART P1 (PB6/7) |
| P3 | `MCU.SchDoc` | U3 STM32F103CBT6, Y1 12MHz, C6/7/8 0.1u | HSE 12MHz + 15p×2，BOOT0→GND，NRST 10K+0.1u，VBAT→3.3V |
| P4 | `Interface.SchDoc` | U8 MT6816, TPS61040, OLED | SPI1 PB3/4/5 + PA15 CS，SPI2 PB13-15 + PA8/9 OLED |
| P5 | `Drive.SchDoc` | U1/U2 TB67H450, Rs 0.1R×2, NTC | IN_AP/AM/BP/BM (PA2-5), PWM PB10/11, VREF 分压，OUT_A/B 到电机端子 |
| P6 | (叠层/PCB) | 板厚/层叠 | 在 PCB 编辑器 `设计 → 层叠管理器` 核对 4 层 1oz |

> 对照工具：
> ```bash
> # 本目录 SVG 预览（导入前先看目标长什么样）
> xdg-open hardware/notes/MotorDriver-42-JLC/schematic.svg
> xdg-open hardware/notes/MotorDriver-42-JLC/interconnect.svg
> xdg-open hardware/notes/MotorDriver-42-JLC/preview.html
> # 原板 PDF
> xdg-open "/home/zenix/Dummy-Robot/1.Hardware/MotorDriver-42/Motor-42.pdf"
> ```

逐页数标题块 `Title / Revision / Author` 是否乱码（应为中文或英文，非 `Åí`）。

### 4.2 网络 38 条是否正确

在嘉立创：`设计 → 网络管理器` 或 `工具 → 设计规则检查 → 网络`，应看到 ≥38 条网络（本包 `MotorDriver-42-JLC.json` 含 41 条含备用）。重点核对：

| 类别 | 必有网络 | 检查方法 |
|---|---|---|
| 电源 | `VCC_IN(24V)`, `VCC_5V`, `VCC_3V3`, `GND`, `VDDA`, `VSSA` | 电源符号是否连通，GND 是否全板铺铜 |
| CAN | `CAN_H`, `CAN_L`, `CAN_TX(PB9→U5 D)`, `CAN_RX(U5 R→PB8)` | 差分对是否成对，120R 是否跨在 CAN_H/L |
| 调试 | `SWDIO(PA13)`, `SWCLK(PA14)`, `UART1_TX(PB6)`, `UART1_RX(PB7)` | 是否到 P5/P1 连接器 |
| 编码器 | `SPI1_SCK(PB3)`, `SPI1_MISO(PB4)`, `SPI1_MOSI(PB5)`, `MT6816_CS(PA15)`, `AUX_CS(PA7)` | 是否到 U8 与 P4 |
| OLED | `SPI2_SCK(PB13)`, `SPI2_MISO(PB14)`, `SPI2_MOSI(PB15)`, `OLED_DC(PA8)`, `OLED_RES(PA9)` | DNP 区是否孤立（不影响主网络） |
| 驱动 | `IN_AP(PA5)`, `IN_AM(PA4)`, `IN_BP(PA3)`, `IN_BM(PA2)`, `IN_PWM_A(PB11)`, `IN_PWM_B(PB10)`, `VREF_A/B`, `OUT_A+/A-/OUT_B+/B-` | 是否到 U1/U2 控制脚与输出焊盘 |
| 模拟 | `POWER_U(PA0)`, `TEMP(PA1)`, `ADC_ENC(PA6)` | 是否到分压/NTC |

若网络数少于 35，多为接口页 Port/Interface 未导入 → 返回步骤2勾选全部 SchDoc。

### 4.3 PCB 5.4M 是否乱位

- PCB 原文件 5.4M 为 4 层板（含大量铺铜与丝印），导入后应无“元件飞出板外”“走线成直线”现象。
- 检查：
  1. 板框 `100×80mm` 左右（量 `编辑 → 测量`）。
  2. U3 LQFP48 居中，U1/U2 QFN32 在板底两侧对称，U6/U7 在左上电源区，U5 靠近 P2/P3。
  3. 天线区无铜，TVS D2–D4 靠近电源入口 D1。
  4. 若错位：尝试 `编辑 → 撤销`，或删除已导入 PCB → 重新执行 `文件 → 导入 → Altium PCB → 选 Motor-42.PcbDoc` 单独再导一次。

### 4.4 DRC / 线宽 / 层叠 设置

| 项 | 值 | 在嘉立创中的位置 |
|---|---|---|
| 层叠 | 4 层，1oz 铜厚，板厚 1.6mm | `设计 → 层叠管理器` → `4 Layer`，铜厚填 `1oz` (35µm) |
| 最小线宽 | 5mil (0.127mm) | `设计 → 规则 → 线宽`，走线 `5mil`，电源 `VM 40mil` |
| 电源大电流 | `VCC_IN / VM` ≥ **40mil** (1.02mm) | 同上，网络类 `POWER` 设 40mil |
| CAN 差分 | 间距 **5mil** (0.127mm)，等长误差 < 2mm | `设计 → 规则 → 差分对`，对 `CAN_H/CAN_L` 设 `5mil/5mil` |
| 丝印 | 宽 6mil，高 40mil | `设计 → 规则 → 丝印` |
| 安全间距 | 5mil | `设计 → 规则 → 间距` |

设置后执行 **工具 → 设计规则检查 (DRC)** → 应 0 错误（若有封装未绑定会报 `未找到封装`，先回步骤3）。

---

## 5. 步骤5 — 导出与保存

### 5.1 导出 Gerber 下单（立创）

1. `文件 → 导出 → Gerber...`（或 `制造 → Gerber`）。
2. 选项：`RS-274X`，精度 `2:5`，零省略 `Leading`，单位 `mm`，包含 `钻孔` + `拼板`（本板无需拼板）。
3. 输出目录：如 `~/Documents/EasyEDA/MotorDriver-42-JLC/Gerber/`。
4. 核对层文件（与原 `Gerber/` 28 文件对比）：
   ```
   .GTO  顶层丝印
   .GTL  顶层铜
   .G1/.G2 中间层
   .GBL  底层铜
   .GBO  底层丝印
   .GTS/.GTP 阻焊
   .GKO  板框
   .TXT  钻孔
   ```
5. 打包：`zip -r MotorDriver-42-JLC_Gerber.zip Gerber/`
6. 浏览器打开 `https://order.jlc.com` → `PCB 下单 → 添加 Gerber 文件` → 上传 zip → 选 4 层 / 1oz / 线宽 5mil → 下单。

> 原工程 `Gerber/28 文件` 已可直接下单（`Motor-42.GTL/GBL/GTO/GBO/G1/G2/...`），导入后重导出的 Gerber 应与其逐层比对（可用 `gerbv` 或立创 CAM 预览）。

### 5.2 保存为本地 .eprj（离线存档）

1. `文件 → 另存为 → 本地工程`（*Save as Local Project*）。
2. 选路径：
   ```
   /home/zenix/zenix_Dummy/hardware/notes/MotorDriver-42-JLC/MotorDriver-42-JLC-imported.eprj
   ```
   或你自己的 `~/Documents/EasyEDA/`。
3. 校验：
   ```bash
   file MotorDriver-42-JLC-imported.eprj
   # → SQLite 3.x database
   sqlite3 MotorDriver-42-JLC-imported.eprj "SELECT name,boards FROM projects;"
   ls -lh MotorDriver-42-JLC-imported.eprj
   # 预期 2–10M（取决于是否含 3D 模型）
   ```
4. 此后双击 `.eprj` 即可离线打开，无需再走导入流程。本目录下占位 `.eprj` 已废弃，切勿覆盖此新文件。

### 5.3 可选：投板前再出一份 PDF

`文件 → 导出 → PDF` → 选 `原理图 + PCB` → 存为 `MotorDriver-42-JLC.pdf`，与原 `Motor-42.pdf` 并排放 `xdg-open` 对比。

---

## 6. 常见坑与解法

| 现象 | 原因 | 解法 |
|---|---|---|
| **中文乱码** `ÅíÖÉêÍ` / `□□□` | 向导编码选成 UTF-8，Altium 原文件为 GBK | 关闭工程，重新 `文件 → 导入 → Altium`，向导第二步编码切为 `GB2312/GBK` |
| **封装属性有误 `$1I389...` / `FPC_16PIN`** | 旧占位 `.eprj` 残留 FPC_16PIN 封装 ID 与新器件不匹配 | 本指南已废弃占位工程；若仍报错，`封装管理器` 全选 → 清空 Footprint → 重新按 §3 表绑定，或直接用新导入的工程（勿打开旧 `MotorDriver-42-JLC.eprj`） |
| **PcbDoc 导入后元件叠在一起 / 乱飞** | Altium PCB 版本过新（Binary 6.x）或坐标溢出 | 单独 `文件 → 导入 → Altium PCB` 选 `Motor-42.PcbDoc`，单位选 `mil`；仍乱则在 Altium 中先 `File → Save As → Binary 5.0` 再导（需借 Windows 机器） |
| **原理图只有 1 页** | 未选 `Motor-42.PrjPCB` 而是单张 SchDoc | 重新导入，务必选 `Motor-42.PrjPCB` 或含它的 zip，且向导勾选全部 5 张 |
| **网络只剩十几条** | Port/Interface 两张未勾选 | 同上，检查向导页“检测到的文档”是否为 5 张 |
| **TB67H450 / MT6816 搜不到** | 立创商城无此型号 | 用通用 `QFN-32` / `QFN-8` 自建封装；BOM 中标记 `替代料`，打板时手动购买 |
| **SH1.0 连接器立创无卧贴** | 嘉立创库命名为 `B**B-SH-A` (JST) | 搜 `SH1.0` 或 `SH-6P`，选 `SH1.0-6A` 立贴/卧贴均可，位号 P2/P3/P4 通用 |
| **DRC 报线宽不足** | 默认规则 6mil，VM 却要求 40mil | `设计 → 规则` 新建网络类 `POWER= VCC_IN,VM` 设 40mil，优先级置顶 |
| **Linux 客户端白屏 / 无法启动** | 未加 `--no-sandbox` | 用完整启动命令：`/home/zenix/.local/opt/easyeda-pro/easyeda-pro --no-sandbox --gtk-version=3`；或重装后删除 `~/.config/EasyEDA-Pro/cache*` |
| **Gerber 下单提示“检测到飞线”** | 导入后网络断开 | 回原理图检查网络标签（如 `CAN_H` 大小写一致），再 `设计 → 更新 PCB` 重新同步 |

### SH1.0 立创优选替代清单

| 原位号 | 原型号 | 立创替代（有库存） | 链接关键词 |
|---|---|---|---|
| P2/P3 | SH1.0-6P 立贴 | `B6B-SH-A` (JST) 或 `SH1.0-6A` | `SH1.0 6P` |
| P4 | SH1.0-6P | 同上 | 同上 |
| P1 | SH1.0-5P | `B5B-SH-A` | `SH1.0 5P` |
| P5 | SH1.0-3P | `B3B-SH-A` | `SH1.0 3P` |
| 配套线 | SH1.0 6P 线 | `SH1.0 对线 100mm` | `SH1.0 线` |

> 购买时备注“立贴/卧贴按板位”，本板 P2/P3 为菊花链直通，卧贴更省高度。

---

## 7. Linux 预览与校验

### 7.1 导入后产物的预览

```bash
# 新导出的 .eprj 快速校验（SQLite）
file ~/Documents/EasyEDA/MotorDriver-42-JLC/MotorDriver-42-JLC-imported.eprj
sqlite3 ~/Documents/EasyEDA/MotorDriver-42-JLC/MotorDriver-42-JLC-imported.eprj "SELECT name,boards FROM projects; SELECT title,docType FROM documents;"

# 导出的 PDF（与原板对比）
xdg-open "/home/zenix/Dummy-Robot/1.Hardware/MotorDriver-42/Motor-42.pdf" &
xdg-open ~/Documents/EasyEDA/MotorDriver-42-JLC/MotorDriver-42-JLC.pdf &

# 导出的 Gerber（立创 CAM 预览或 gerbv）
# 方式A：直接用浏览器打开立创 CAM https://upreview.jlc.com
# 方式B：本地 gerbv（若已装）
gerbv ~/Documents/EasyEDA/MotorDriver-42-JLC/Gerber/*.GTL &
gerbv "/home/zenix/Dummy-Robot/1.Hardware/MotorDriver-42/Gerber/Motor-42.GTL" &

# 本目录 SVG 对照（永不失效的离线预览）
xdg-open hardware/notes/MotorDriver-42-JLC/schematic.svg &
xdg-open hardware/notes/MotorDriver-42-JLC/interconnect.svg &
xdg-open hardware/notes/MotorDriver-42-JLC/preview.html &
# 无桌面时用 firefox
firefox hardware/notes/MotorDriver-42-JLC/preview.html &
# 或纯校验
xmllint --noout hardware/notes/MotorDriver-42-JLC/*.svg && echo "svg ok"
```

### 7.2 导入前后的 md5 对比（防文件被改）

```bash
# 编译并运行 C 校验工具（C11，零依赖 -lcrypto）
gcc -std=c11 -O2 -Wall -Wextra -o /tmp/check_altium_import tools/check_altium_import.c
/tmp/check_altium_import
# 指定路径
/tmp/check_altium_import /home/zenix/Dummy-Robot/1.Hardware/MotorDriver-42
# 输出形如：
# [OK] Power.SchDoc  84480 bytes  md5=abda5b50ed75d4314d0866820bf086bb
# [OK] MCU.SchDoc    95744 bytes  md5=afbdec9b0fd45ad33ea0b0761bff72ad
# ...
```

若某文件 md5 变化，说明原工程被误改，应从备份 `~/MotorDriver-42-backup-*` 恢复后再导入。

### 7.3 目录自检

```bash
ls -lh hardware/notes/MotorDriver-42-JLC/
# 应含：IMPORT_ALTIUM.md  README.md  schematic.svg  interconnect.svg  preview.html  tools/
# 不应再含可直接打开的占位 MotorDriver-42-JLC.eprj（已移至 *.deprecated*）
ls -lh hardware/notes/MotorDriver-42-JLC/*.eprj* 2>&1 | cat
ls -lh tools/check_altium_import.c
```

---

## 8. 废弃说明

- 2026-08-20 之前生成的 `hardware/notes/MotorDriver-42-JLC/MotorDriver-42-JLC.eprj`（含 `.eprj2` / `.eprj_backup`）为 **FPC_16PIN 占位工程**，仅含 8 个 FPC 符号 + 2 根演示连线，无真实 TB67/MT6816 封装，已被用户截图吐槽。
- 现作废处理：文件已重命名为 `*.deprecated*`，不再作为导入源。请严格按本指南 **步骤1 选 `Motor-42.PrjPCB`** 重新导入。
- 本目录 `schematic.svg` / `interconnect.svg` 保留作为导入后人工对照，与新导入结果像素级比对。

---

## 9. 一键核验清单（导入后逐项勾）

- [ ] 5 张 SchDoc 均在工程树（Drive/Interface/MCU/Port/Power）
- [ ] 中文无乱码，Title 块可读
- [ ] 网络 ≥38 条，CAN_TX/RX 与 SPI1/2 均在
- [ ] U3/U6/U7/U5/U1/U2/U8/P2/P3 已绑定正确封装（非 FPC_16PIN）
- [ ] PCB 无乱飞，板框 100×80mm，4 层 1oz
- [ ] DRC 0 错误，VM 40mil / CAN 差分 5mil 已设
- [ ] Gerber 28 层与原 Gerber 逐层一致（GTO/GTL/GBL/GBO 等）
- [ ] 已另存为 `MotorDriver-42-JLC-imported.eprj` 并 `file` 校验为 SQLite

---

*维护：zenix · 2026-08-20 · 客户端 3.2.149 (Linux Mint 22.3) · 原工程 5.4M PcbDoc + 6 页 PDF · 约束：只用 C/C# 写程序，Markdown 直写*
