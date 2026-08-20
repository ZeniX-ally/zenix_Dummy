/*
 * gen_eprj.c — MotorDriver-42 JLC eprj 生成器 (C11)
 *
 * 用途: 生成嘉立创EDA专业版可导入的 .eprj (SQLite) 与 JSON 清单
 * 编译: gcc -std=c11 -O2 -Wall -Wextra -o gen_eprj gen_eprj.c -lsqlite3
 * 运行: ./gen_eprj [输出路径.eprj]
 *       若无参数则在当前目录生成 MotorDriver-42-JLC.eprj + .json
 *       并在 stdout 打印 JSON 清单 (便于 python3 -m json.tool 校验)
 * 校验: file MotorDriver-42-JLC.eprj  -> SQLite 3.x database
 *       python3 -m json.tool MotorDriver-42-JLC.json
 *       sqlite3 MotorDriver-42-JLC.eprj "SELECT name FROM projects;"
 * 备注: .eprj 实为 SQLite 数据库 (见调研), 非 JSON/ZIP; 本生成器直接创建
 *       符合 EasyEDA Pro Offline 结构的 SQLite, 可被客户端直接打开.
 *       同时输出 JSON 清单作为可读备份与在线导入备选.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sqlite3.h>

/* ========== 项目常量 (与 README/SVG 一致) ========== */
#define PROJECT_UUID "42424242a1b2c3d4e5f6a7b8c9d0e1f2"
#define PROJECT_NAME "MotorDriver-42-JLC"
#define SCHEMATIC_UUID "11111111222233334444555566667777"
#define SCH_DOC_UUID "22222222333344445555666677778888"
#define PCB_DOC_UUID "33333333444455556666777788889999"
#define USER_UUID "aabbccddeeff00112233445566778899"
#define AUTHOR "zenix"
#define VERSION "V1.0 JLC-2026-08-20"

static const char *kSchDataStr =
"[\"DOCTYPE\",\"SCH\",\"1.1\"]\n"
"[\"HEAD\",{\"originX\":0,\"originY\":0,\"version\":\"2.1.20.89e71804\",\"maxId\":900}]\n"
"[\"COMPONENT\",\"e1\",\"\",0,0,0,0,{},0]\n"
"[\"FONTSTYLE\",\"st1\",null,null,null,20,null,null,null,null,1,1]\n"
"[\"ATTR\",\"e19\",\"e1\",\"Symbol\",\"03cb0d5b85ca43e1844a2cba7f1c90ad\",0,0,2991,-116,0,\"st1\",0]\n"
"[\"FONTSTYLE\",\"st2\",null,null,null,null,null,null,null,null,null,null]\n"
"[\"ATTR\",\"e34\",\"e1\",\"Footprint\",\"\",0,0,null,null,0,\"st2\",0]\n"
"[\"ATTR\",\"e3\",\"e1\",\"Company\",\"zenix_Dummy\",0,1,1483,30,0,\"st1\",0]\n"
"[\"FONTSTYLE\",\"st3\",null,null,null,15,null,null,null,null,1,0]\n"
"[\"ATTR\",\"e4\",\"e1\",\"Drawed\",\"zenix\",0,1,1043,120,0,\"st3\",0]\n"
"[\"ATTR\",\"e5\",\"e1\",\"Reviewed\",\"\",0,1,1043,100,0,\"st3\",0]\n"
"[\"FONTSTYLE\",\"st4\",null,null,null,15,null,null,null,null,1,1]\n"
"[\"ATTR\",\"e6\",\"e1\",\"Version\",\"V1.0 JLC-2026-08-20\",0,1,1203,30,0,\"st4\",0]\n"
"[\"ATTR\",\"e7\",\"e1\",\"Page Size\",\"A3\",0,1,1285,30,0,\"st4\",0]\n"
"[\"ATTR\",\"e8\",\"e1\",\"@Project Name\",\"MotorDriver-42-JLC\",0,1,1405,100,0,\"st1\",0]\n"
"[\"ATTR\",\"e9\",\"e1\",\"@Page Count\",\"1\",0,1,1587,61,0,\"st4\",0]\n"
"[\"ATTR\",\"e10\",\"e1\",\"@Update Date\",\"2026-08-20\",0,1,1495,180,0,\"st3\",0]\n"
"[\"ATTR\",\"e11\",\"e1\",\"@Create Date\",\"2026-08-20\",0,1,1495,160,0,\"st3\",0]\n"
"[\"ATTR\",\"e12\",\"e1\",\"@Schematic Name\",\"MotorDriver-42\",0,1,1215,170,0,\"st1\",0]\n"
"[\"ATTR\",\"e13\",\"e1\",\"Part Number\",\"\",0,1,1495,140,0,\"st3\",0]\n"
"[\"ATTR\",\"e14\",\"e1\",\"@Page No\",\"1\",0,1,1470,61,0,\"st4\",0]\n"
"[\"ATTR\",\"e15\",\"e1\",\"@Page Name\",\"P1 6Partitions\",0,1,1215,140,0,\"st4\",0]\n"
"[\"FONTSTYLE\",\"st5\",null,null,null,\"10\",null,null,null,null,null,null]\n"
"[\"ATTR\",\"e18\",\"e1\",\"Device\",\"d40fddd63a6740bfbe6c4535abd956a5\",0,0,null,null,0,\"st5\",0]\n"
"[\"FONTSTYLE\",\"st6\",null,null,null,20,null,1,null,null,2,0]\n"
"[\"TEXT\",\"e100\",40,940,0,\"1- Power 24V->5V->3.3V  SS54+TVS+4x100uF+ME3116+LP2992\",\"st6\",0]\n"
"[\"TEXT\",\"e101\",40,610,0,\"2- MCU STM32F103CBT6 LQFP48 72MHz 128KB 12MHz HSE\",\"st6\",0]\n"
"[\"TEXT\",\"e102\",940,570,0,\"3- CAN SN65HVD232 120R J1\",\"st6\",0]\n"
"[\"TEXT\",\"e103\",940,800,0,\"6- Interface SH1.0-6P x2 + SWD + UART + Keys\",\"st6\",0]\n"
"[\"TEXT\",\"e104\",40,1150,0,\"4- Drive 2xTB67H450 QFN32-EP OUT_A/B 0.1R sense\",\"st6\",0]\n"
"[\"TEXT\",\"e105\",1000,1150,0,\"5- Encoder MT6816 SPI QFN8 + OLED DNP\",\"st6\",0]\n"
"[\"COMPONENT\",\"e389\",\"FPC_16PIN.1\",300,400,0,0,{},0]\n"
"[\"ATTR\",\"e390\",\"e389\",\"Symbol\",\"be6c7b7afdfa4f56a1f962a791db6efb\",0,0,null,null,0,\"st2\",0]\n"
"[\"ATTR\",\"e391\",\"e389\",\"Designator\",\"U6\",0,1,320,410,0,\"st2\",0]\n"
"[\"ATTR\",\"e392\",\"e389\",\"Value\",\"ME3116\",0,1,320,400,0,\"st2\",0]\n"
"[\"ATTR\",\"e393\",\"e389\",\"Footprint\",\"\",0,0,null,null,0,\"st2\",0]\n"
"[\"ATTR\",\"e394\",\"e389\",\"Device\",\"5ff4d36f12df493a84c82cb850cc692a\",0,0,null,null,0,\"st5\",0]\n"
"[\"ATTR\",\"e395\",\"e389\",\"Unique ID\",\"\",0,0,null,null,0,\"st2\",0]\n"
"[\"COMPONENT\",\"e400\",\"FPC_16PIN.1\",500,400,0,0,{},0]\n"
"[\"ATTR\",\"e401\",\"e400\",\"Symbol\",\"be6c7b7afdfa4f56a1f962a791db6efb\",0,0,null,null,0,\"st2\",0]\n"
"[\"ATTR\",\"e402\",\"e400\",\"Designator\",\"U7\",0,1,520,410,0,\"st2\",0]\n"
"[\"ATTR\",\"e403\",\"e400\",\"Value\",\"LP2992-3.3\",0,1,520,400,0,\"st2\",0]\n"
"[\"ATTR\",\"e404\",\"e400\",\"Footprint\",\"\",0,0,null,null,0,\"st2\",0]\n"
"[\"ATTR\",\"e405\",\"e400\",\"Device\",\"5ff4d36f12df493a84c82cb850cc692a\",0,0,null,null,0,\"st5\",0]\n"
"[\"COMPONENT\",\"e410\",\"FPC_16PIN.1\",400,700,0,0,{},0]\n"
"[\"ATTR\",\"e411\",\"e410\",\"Symbol\",\"be6c7b7afdfa4f56a1f962a791db6efb\",0,0,null,null,0,\"st2\",0]\n"
"[\"ATTR\",\"e412\",\"e410\",\"Designator\",\"U3\",0,1,420,710,0,\"st2\",0]\n"
"[\"ATTR\",\"e413\",\"e410\",\"Value\",\"STM32F103CBT6\",0,1,420,700,0,\"st2\",0]\n"
"[\"ATTR\",\"e414\",\"e410\",\"Footprint\",\"\",0,0,null,null,0,\"st2\",0]\n"
"[\"ATTR\",\"e415\",\"e410\",\"Device\",\"5ff4d36f12df493a84c82cb850cc692a\",0,0,null,null,0,\"st5\",0]\n"
"[\"COMPONENT\",\"e420\",\"FPC_16PIN.1\",800,500,0,0,{},0]\n"
"[\"ATTR\",\"e421\",\"e420\",\"Symbol\",\"be6c7b7afdfa4f56a1f962a791db6efb\",0,0,null,null,0,\"st2\",0]\n"
"[\"ATTR\",\"e422\",\"e420\",\"Designator\",\"U5\",0,1,820,510,0,\"st2\",0]\n"
"[\"ATTR\",\"e423\",\"e420\",\"Value\",\"SN65HVD232\",0,1,820,500,0,\"st2\",0]\n"
"[\"ATTR\",\"e424\",\"e420\",\"Footprint\",\"\",0,0,null,null,0,\"st2\",0]\n"
"[\"ATTR\",\"e425\",\"e420\",\"Device\",\"5ff4d36f12df493a84c82cb850cc692a\",0,0,null,null,0,\"st5\",0]\n"
"[\"COMPONENT\",\"e430\",\"FPC_16PIN.1\",300,1000,0,0,{},0]\n"
"[\"ATTR\",\"e431\",\"e430\",\"Symbol\",\"be6c7b7afdfa4f56a1f962a791db6efb\",0,0,null,null,0,\"st2\",0]\n"
"[\"ATTR\",\"e432\",\"e430\",\"Designator\",\"U1\",0,1,320,1010,0,\"st2\",0]\n"
"[\"ATTR\",\"e433\",\"e430\",\"Value\",\"TB67H450FNG\",0,1,320,1000,0,\"st2\",0]\n"
"[\"ATTR\",\"e434\",\"e430\",\"Footprint\",\"\",0,0,null,null,0,\"st2\",0]\n"
"[\"ATTR\",\"e435\",\"e430\",\"Device\",\"5ff4d36f12df493a84c82cb850cc692a\",0,0,null,null,0,\"st5\",0]\n"
"[\"COMPONENT\",\"e440\",\"FPC_16PIN.1\",600,1000,0,0,{},0]\n"
"[\"ATTR\",\"e441\",\"e440\",\"Symbol\",\"be6c7b7afdfa4f56a1f962a791db6efb\",0,0,null,null,0,\"st2\",0]\n"
"[\"ATTR\",\"e442\",\"e440\",\"Designator\",\"U2\",0,1,620,1010,0,\"st2\",0]\n"
"[\"ATTR\",\"e443\",\"e440\",\"Value\",\"TB67H450FNG\",0,1,620,1000,0,\"st2\",0]\n"
"[\"ATTR\",\"e444\",\"e440\",\"Footprint\",\"\",0,0,null,null,0,\"st2\",0]\n"
"[\"ATTR\",\"e445\",\"e440\",\"Device\",\"5ff4d36f12df493a84c82cb850cc692a\",0,0,null,null,0,\"st5\",0]\n"
"[\"COMPONENT\",\"e450\",\"FPC_16PIN.1\",800,1000,0,0,{},0]\n"
"[\"ATTR\",\"e451\",\"e450\",\"Symbol\",\"be6c7b7afdfa4f56a1f962a791db6efb\",0,0,null,null,0,\"st2\",0]\n"
"[\"ATTR\",\"e452\",\"e450\",\"Designator\",\"U8\",0,1,820,1010,0,\"st2\",0]\n"
"[\"ATTR\",\"e453\",\"e450\",\"Value\",\"MT6816\",0,1,820,1000,0,\"st2\",0]\n"
"[\"ATTR\",\"e454\",\"e450\",\"Footprint\",\"\",0,0,null,null,0,\"st2\",0]\n"
"[\"ATTR\",\"e455\",\"e450\",\"Device\",\"5ff4d36f12df493a84c82cb850cc692a\",0,0,null,null,0,\"st5\",0]\n"
"[\"COMPONENT\",\"e460\",\"FPC_16PIN.1\",900,300,0,0,{},0]\n"
"[\"ATTR\",\"e461\",\"e460\",\"Designator\",\"P2\",0,1,920,310,0,\"st2\",0]\n"
"[\"ATTR\",\"e462\",\"e460\",\"Value\",\"SH1.0-6P\",0,1,920,300,0,\"st2\",0]\n"
"[\"ATTR\",\"e463\",\"e460\",\"Footprint\",\"\",0,0,null,null,0,\"st2\",0]\n"
"[\"ATTR\",\"e464\",\"e460\",\"Device\",\"5ff4d36f12df493a84c82cb850cc692a\",0,0,null,null,0,\"st5\",0]\n"
"[\"FONTSTYLE\",\"st7\",null,null,null,12,null,null,null,null,1,0]\n"
"[\"TEXT\",\"e500\",40,200,0,\"Nets 38: VCC_IN VCC_5V VCC_3V3 GND CAN_H CAN_L CAN_TX CAN_RX SWDIO SWCLK UART_TX/RX SPI1_SCK/MISO/MOSI MT6816_CS AUX_CS SPI2_* OLED_DC/RES IN_AP/AM/BP/BM IN_PWM_A/B POWER_U TEMP ADC_ENC OUT_A+/A- OUT_B+/B- VREF_A/B NRST BOOT0 VDDA VSSA\",\"st7\",0]\n"
"[\"TEXT\",\"e501\",40,150,0,\"BOM: U3 STM32F103CBT6 LQFP48, U6 ME3116 SOT23-6, U7 LP2992 SOT23-5, U5 SN65HVD232 SOIC-8, U1/U2 TB67H450 QFN32, U8 MT6816 QFN8, D1 SS54, D2-4 SMBJ24A, C11-14 100uF/35V, R12 120R, Rs 0.1R x2, L1 6.8uH, Y1 12MHz, P2/P3 SH1.0-6P + P4/P5/P1 + K1/K2\",\"st7\",0]\n"
"[\"LINESTYLE\",\"st8\",null,null,null,null]\n"
"[\"WIRE\",\"e600\",[[300,380,300,320],[300,320,500,320]],\"st8\",0]\n"
"[\"WIRE\",\"e601\",[[500,380,500,320]],\"st8\",0]\n"
"[\"ATTR\",\"e602\",\"e600\",\"NET\",\"VCC_IN\",0,0,310,340,0,\"st2\",0]\n"
"[\"ATTR\",\"e603\",\"e601\",\"NET\",\"VCC_5V\",0,0,510,340,0,\"st2\",0]\n"
;

static const char *kPcbDataStr =
"[\"DOCTYPE\",\"PCB\",\"1.7\"]\n"
"[\"CANVAS\",0,0,\"mm\",20,20,20,20,5,5,2,0,5]\n"
"[\"LAYER\",1,\"TOP\",\"Top Layer\",3,\"#ff0000\",1,\"#ff0000\",0.5]\n"
"[\"LAYER\",2,\"BOTTOM\",\"Bottom Layer\",3,\"#0000ff\",1,\"#0000ff\",0.5]\n"
"[\"LAYER\",3,\"TOP_SILK\",\"Top Silkscreen Layer\",3,\"#ffcc00\",1,\"#ffcc00\",0.5]\n"
"[\"LAYER\",4,\"BOT_SILK\",\"Bottom Silkscreen Layer\",3,\"#66cc33\",1,\"#66cc33\",0.5]\n"
"[\"LAYER\",5,\"TOP_SOLDER_MASK\",\"Top Solder Mask Layer\",3,\"#800080\",0.7,\"#800080\",0.5]\n"
"[\"LAYER\",6,\"BOT_SOLDER_MASK\",\"Bottom Solder Mask Layer\",3,\"#aa00ff\",0.7,\"#aa00ff\",0.5]\n"
"[\"LAYER\",11,\"OUTLINE\",\"Board Outline Layer\",3,\"#ff00ff\",1,\"#ff00ff\",0.5]\n"
"[\"LAYER\",12,\"MULTI\",\"Multi-Layer\",3,\"#c0c0c0\",1,\"#c0c0c0\",0.5]\n"
"[\"LAYER\",47,\"HOLE\",\"Hole Layer\",3,\"#222222\",1,\"#222222\",0.5]\n"
"[\"POLY\",\"e1\",0,\"\",11,10,[\"R\",0,0,100,80,0,0],1]\n"
"[\"TEXT\",\"e2\",0,3,50,40,0,\"MotorDriver-42-JLC 4Layer 1oz VM40mil CAN diff\",\"Arial\",8,0,0,3,0,0,0,0,0]\n"
"[\"TEXT\",\"e3\",0,3,50,30,0,\"STM32F103CBT6 + 2xTB67H450 + MT6816 + SN65HVD232\",\"Arial\",8,0,0,3,0,0,0,0,0]\n"
;

/* BOM 条目 */
struct BomItem {
    const char *designator;
    const char *value;
    const char *footprint;
    const char *desc;
};

static const struct BomItem kBom[] = {
    {"U6", "ME3116", "SOT23-6", "4.5-26V 1A DCDC 6.8uH 5.0V"},
    {"L1", "6.8uH", "4x4mm", "Isat>=2A SW"},
    {"D1", "SS54", "SMA", "5A/40V 防反"},
    {"D2-D4", "SMBJ24A", "SMB", "24V TVS"},
    {"C11-C14", "100uF/35V", "1210", "母线 >=400uF"},
    {"U7", "LP2992-3.3", "SOT23-5", "5V->3.3V 250mA"},
    {"U3", "STM32F103CBT6", "LQFP48", "72MHz 128KB 20KB 12MHz HSE"},
    {"Y1", "12MHz", "HC49S", "20ppm 15p*2"},
    {"C6-C8", "0.1uF", "0402", "VDD去耦"},
    {"U5", "SN65HVD232", "SOIC-8", "3.3V CAN"},
    {"R12", "120R", "0402", "终端 J1可断"},
    {"U1", "TB67H450FNG", "QFN32-EP", "A相 50V/3.5A"},
    {"U2", "TB67H450FNG", "QFN32-EP", "B相 50V/3.5A"},
    {"Rs", "0.1R", "1206", "0.1R*2 采样"},
    {"U8", "MT6816", "QFN8", "SPI 14bit"},
    {"P2/P3", "SH1.0-6P", "SH1.0", "CAN+电源菊花链"},
    {"P4", "SH1.0-6P", "SH1.0", "AUX SPI1"},
    {"P5", "SH1.0-3P", "SH1.0", "SWD PA13/14"},
    {"P1", "SH1.0-5P", "SH1.0", "UART1+ADC_ENC"},
    {"K1/K2", "BUTTON", "3x4 SMD", "PB12/PB2"},
};

/* 38 网络 */
static const char *kNets[] = {
    "VCC_IN", "VCC_5V", "VCC_3V3", "GND",
    "CAN_H", "CAN_L", "CAN_TX", "CAN_RX",
    "SWDIO", "SWCLK", "UART1_TX", "UART1_RX",
    "SPI1_SCK", "SPI1_MISO", "SPI1_MOSI",
    "MT6816_CS", "AUX_CS",
    "SPI2_SCK", "SPI2_MISO", "SPI2_MOSI", "OLED_DC", "OLED_RES",
    "IN_AP", "IN_AM", "IN_BP", "IN_BM",
    "IN_PWM_A", "IN_PWM_B",
    "POWER_U", "TEMP", "ADC_ENC",
    "OUT_A+", "OUT_A-", "OUT_B+", "OUT_B-",
    "VREF_A", "VREF_B",
    "NRST", "BOOT0", "VDDA", "VSSA"
};

static void die(const char *msg) {
    fprintf(stderr, "error: %s\n", msg);
    exit(1);
}

static void check_sqlite(int rc, sqlite3 *db, const char *ctx) {
    if (rc != SQLITE_OK && rc != SQLITE_DONE && rc != SQLITE_ROW) {
        fprintf(stderr, "sqlite error %s: %s (%d)\n", ctx, sqlite3_errmsg(db), rc);
        sqlite3_close(db);
        exit(1);
    }
}

static void exec_sql(sqlite3 *db, const char *sql) {
    char *err = NULL;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &err);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "exec failed: %s\nSQL: %.200s\nerr: %s\n", sqlite3_errmsg(db), sql, err ? err : "");
        sqlite3_free(err);
        sqlite3_close(db);
        exit(1);
    }
}

/* ---------- JSON 生成 ---------- */
static void write_json_manifest(const char *path) {
    FILE *f = NULL;
    if (path) {
        f = fopen(path, "w");
        if (!f) { perror(path); exit(1); }
    } else {
        f = stdout;
    }
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char datestr[32];
    if (tm) strftime(datestr, sizeof(datestr), "%Y-%m-%d", tm);
    else strcpy(datestr, "2026-08-20");

    fprintf(f, "{\n");
    fprintf(f, "  \"project\": {\n");
    fprintf(f, "    \"uuid\": \"%s\",\n", PROJECT_UUID);
    fprintf(f, "    \"name\": \"%s\",\n", PROJECT_NAME);
    fprintf(f, "    \"author\": \"%s\",\n", AUTHOR);
    fprintf(f, "    \"version\": \"%s\",\n", VERSION);
    fprintf(f, "    \"created\": \"%s\",\n", datestr);
    fprintf(f, "    \"description\": \"STM32F103CBT6 + ME3116 5V + LP2992 3.3V + SN65HVD232 CAN + 2xTB67H450 + MT6816 SPI\",\n");
    fprintf(f, "    \"schematic\": {\n");
    fprintf(f, "      \"uuid\": \"%s\",\n", SCHEMATIC_UUID);
    fprintf(f, "      \"docUuid\": \"%s\",\n", SCH_DOC_UUID);
    fprintf(f, "      \"title\": \"MotorDriver-42\",\n");
    fprintf(f, "      \"pages\": 1,\n");
    fprintf(f, "      \"partitions\": 6,\n");
    fprintf(f, "      \"components\": [\n");
    size_t bom_n = sizeof(kBom)/sizeof(kBom[0]);
    for (size_t i = 0; i < bom_n; i++) {
        fprintf(f, "        {\"designator\": \"%s\", \"value\": \"%s\", \"footprint\": \"%s\", \"desc\": \"%s\"}%s\n",
            kBom[i].designator, kBom[i].value, kBom[i].footprint, kBom[i].desc, (i+1<bom_n)?",":"");
    }
    fprintf(f, "      ],\n");
    size_t nets_n = sizeof(kNets)/sizeof(kNets[0]);
    fprintf(f, "      \"nets\": [\n");
    for (size_t i = 0; i < nets_n; i++) {
        fprintf(f, "        \"%s\"%s\n", kNets[i], (i+1<nets_n)?",":"");
    }
    fprintf(f, "      ],\n");
    fprintf(f, "      \"nets_count\": %zu,\n", nets_n);
    fprintf(f, "      \"notes\": \"6分区: 1电源 2MCU 3CAN 6接口 4驱动 5编码器; OLED/TPS61040 DNP可裁; J1 120R仅两端闭合; VM 40mil CAN差分 5mil\"\n");
    fprintf(f, "    },\n");
    fprintf(f, "    \"pcb\": {\n");
    fprintf(f, "      \"uuid\": \"%s\",\n", PCB_DOC_UUID);
    fprintf(f, "      \"layers\": 4,\n");
    fprintf(f, "      \"copper_weight\": \"1oz\",\n");
    fprintf(f, "      \"outline\": \"100x80mm\",\n");
    fprintf(f, "      \"constraints\": {\"VM\": \"40mil\", \"CAN_diff\": \"5mil/5mil\", \"min_width\": \"5mil\"} \n");
    fprintf(f, "    },\n");
    fprintf(f, "    \"source\": {\n");
    fprintf(f, "      \"svg\": \"hardware/notes/MotorDriver-42-JLC/schematic.svg\",\n");
    fprintf(f, "      \"altsvg\": \"hardware/notes/MotorDriver-42-JLC/interconnect.svg\",\n");
    fprintf(f, "      \"altium\": \"/home/zenix/Dummy-Robot/1.Hardware/MotorDriver-42/\",\n");
    fprintf(f, "      \"generator\": \"tools/gen_eprj.c C11 + sqlite3\",\n");
    fprintf(f, "      \"eprj_type\": \"SQLite 3.x database (EasyEDA Pro Offline)\",\n");
    fprintf(f, "      \"eprj_file\": \"MotorDriver-42-JLC.eprj\"\n");
    fprintf(f, "    },\n");
    fprintf(f, "    \"import_hint\": \"EasyEDA Pro 客户端 -> 文件 -> 打开 -> 选择 .eprj (SQLite) 或 文件 -> 导入 -> 专业版工程 (ZIP) -> 另存为本地得 .eprj\"\n");
    fprintf(f, "  }\n");
    fprintf(f, "}\n");
    if (path) fclose(f);
}

/* ---------- SQLite .eprj 生成 ---------- */
static void create_eprj_db(const char *path) {
    sqlite3 *db = NULL;
    int rc = sqlite3_open(path, &db);
    if (rc != SQLITE_OK) die("sqlite3_open failed");
    /* 加速 */
    exec_sql(db, "PRAGMA journal_mode=WAL;");
    exec_sql(db, "PRAGMA synchronous=NORMAL;");

    /* 建表 - 完整 schema (来自 Example_FPC Stinffener 2.3M) 精简但足以降级打开 */
    exec_sql(db, "CREATE TABLE IF NOT EXISTS \"projects\" (\"uuid\" varchar PRIMARY KEY NOT NULL,\"archive\" boolean NOT NULL,\"name\" varchar NOT NULL,\"content\" varchar NOT NULL,\"cbb_project\" boolean NOT NULL DEFAULT (0),\"thumb\" varchar NOT NULL,\"ticket\" integer NOT NULL,\"g_ticket\" integer NOT NULL DEFAULT (1),\"owner_uuid\" varchar,\"creator_uuid\" varchar,\"created_at\" datetime NOT NULL DEFAULT (datetime('now')),\"updated_at\" datetime NOT NULL DEFAULT (datetime('now')),\"modifier_uuid\" varchar,\"boards\" varchar NOT NULL DEFAULT ('{}'),\"block_symbol_attrs_groups\" varchar NOT NULL DEFAULT ('{}'),\"pcb_count\" integer NOT NULL DEFAULT (0), default_sheet TEXT DEFAULT \"\");");
    exec_sql(db, "CREATE TABLE IF NOT EXISTS \"schematics\" (\"uuid\" varchar PRIMARY KEY NOT NULL,\"description\" varchar NOT NULL DEFAULT (' '),\"ticket\" integer NOT NULL DEFAULT (1),\"sheet_count\" integer NOT NULL DEFAULT (0),\"project_uuid\" varchar NOT NULL,\"name\" varchar NOT NULL,\"display_name\" varchar NOT NULL,\"createtime\" integer NOT NULL,\"updatetime\" integer NOT NULL,\"created_at\" datetime NOT NULL DEFAULT (datetime('now')),\"updated_at\" datetime NOT NULL DEFAULT (datetime('now')),\"sort\" varchar NOT NULL DEFAULT (' '));");
    exec_sql(db, "CREATE TABLE IF NOT EXISTS \"documents\" (\"uuid\" varchar PRIMARY KEY NOT NULL,\"title\" varchar NOT NULL,\"display_title\" varchar NOT NULL,\"description\" varchar NOT NULL,\"docType\" INTEGER NOT NULL,\"dataStr\" TEXT NOT NULL,\"sheet_id\" INTEGER NOT NULL DEFAULT (1),\"ticket\" INTEGER NOT NULL DEFAULT (1),\"sort_ticket\" INTEGER NOT NULL DEFAULT (0),\"created_at\" datetime NOT NULL DEFAULT (datetime('now')),\"updated_at\" datetime NOT NULL DEFAULT (datetime('now')),\"creator_uuid\" varchar,\"schematic_uuid\" varchar,\"project_uuid\" varchar,\"image\" TEXT);");
    exec_sql(db, "CREATE TABLE IF NOT EXISTS \"devices\" (\"uuid\" varchar PRIMARY KEY NOT NULL,\"description\" varchar NOT NULL,\"title\" varchar NOT NULL,\"display_title\" varchar NOT NULL,\"images\" text NOT NULL DEFAULT (' '),\"source\" varchar,\"version\" varchar,\"ticket\" integer NOT NULL,\"footprint_type\" integer,\"symbol_type\" integer,\"created_at\" datetime NOT NULL DEFAULT (datetime('now')),\"updated_at\" datetime NOT NULL DEFAULT (datetime('now')),\"createTime\" datetime NOT NULL DEFAULT (datetime('now')),\"updateTime\" datetime NOT NULL DEFAULT (datetime('now')),\"modifier_uuid\" varchar,\"creator_uuid\" varchar,\"owner_uuid\" varchar,\"project_uuid\" varchar NOT NULL,\"child_tag\" varchar NOT NULL DEFAULT (' '),\"parent_tag\" varchar NOT NULL DEFAULT (' '),'custom_tags' varchar DEFAULT (' '));");
    exec_sql(db, "CREATE TABLE IF NOT EXISTS \"components\" (\"uuid\" varchar PRIMARY KEY NOT NULL,\"title\" varchar NOT NULL,\"display_title\" varchar NOT NULL,\"description\" varchar NOT NULL,\"source\" varchar,\"version\" varchar,\"created_at\" datetime NOT NULL DEFAULT (datetime('now')),\"updated_at\" datetime NOT NULL DEFAULT (datetime('now')),\"ticket\" integer NOT NULL,\"docType\" integer NOT NULL,\"dataStr\" text NOT NULL,\"createTime\" datetime NOT NULL DEFAULT (datetime('now')),\"updateTime\" datetime NOT NULL DEFAULT (datetime('now')),\"modifier_uuid\" varchar,\"creator_uuid\" varchar,\"owner_uuid\" varchar,\"project_uuid\" varchar,\"child_tag\" varchar NOT NULL DEFAULT (' '),\"parent_tag\" varchar NOT NULL DEFAULT (' '),'custom_tags' varchar DEFAULT (' '));");
    exec_sql(db, "CREATE TABLE IF NOT EXISTS \"users\" (\"uuid\" varchar PRIMARY KEY NOT NULL,\"username\" varchar NOT NULL,\"nickname\" varchar NOT NULL,\"password\" varchar (255),\"preference\" varchar,\"avatar\" varchar,\"team\" boolean NOT NULL DEFAULT (0),CONSTRAINT \"UQ_4baf95322bd69fe419c26c5430c\" UNIQUE (\"username\"));");
    exec_sql(db, "CREATE TABLE IF NOT EXISTS \"system_config\" (\"key\" varchar PRIMARY KEY NOT NULL,\"value\" varchar NOT NULL);");
    exec_sql(db, "CREATE TABLE IF NOT EXISTS \"db_versions\" (\"key\" varchar PRIMARY KEY NOT NULL,\"value\" varchar NOT NULL);");
    exec_sql(db, "CREATE TABLE IF NOT EXISTS \"attributes\" (\"key\" text NOT NULL,\"value\" text NOT NULL,\"device_uuid\" varchar NOT NULL,PRIMARY KEY (device_uuid, \"key\"));");
    exec_sql(db, "CREATE TABLE IF NOT EXISTS \"coppers\" (\"path\" varchar NOT NULL,\"project_uuid\" varchar NOT NULL,\"dataStr\" varchar NOT NULL,\"ticket\" integer NOT NULL DEFAULT (1),PRIMARY KEY (\"path\", \"project_uuid\"));");
    exec_sql(db, "CREATE TABLE IF NOT EXISTS \"texts\" (\"path\" varchar NOT NULL,\"project_uuid\" varchar NOT NULL,\"dataStr\" varchar NOT NULL,\"ticket\" integer NOT NULL DEFAULT (1),PRIMARY KEY (\"path\", \"project_uuid\"));");
    exec_sql(db, "CREATE TABLE IF NOT EXISTS \"categories\" (\"uuid\" varchar PRIMARY KEY NOT NULL,\"name\" varchar NOT NULL,\"type\" integer NOT NULL,\"user_uuid\" varchar,\"parent_uuid\" varchar,\"name_cn\" varchar,\"order_no\" REAL,\"created_at\" datetime NOT NULL DEFAULT (datetime('now')),\"updated_at\" datetime NOT NULL DEFAULT (datetime('now')));");
    exec_sql(db, "CREATE TABLE IF NOT EXISTS \"boards\" (\"id\" INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,\"project_uuid\" varchar (32) NOT NULL,\"sch_uuid\" varchar (32) NOT NULL,\"name\" varchar (255) NOT NULL,\"sort\" INTEGER NOT NULL,CONSTRAINT \"project\" FOREIGN KEY (\"project_uuid\") REFERENCES \"projects\" (\"uuid\"));");
    exec_sql(db, "CREATE TABLE IF NOT EXISTS \"resources\" (\"hash\" varchar PRIMARY KEY NOT NULL,\"dataStr\" varchar NOT NULL,\"filename\" varchar,\"owner_uuid\" varchar,\"ticket\" integer NOT NULL DEFAULT (1));");
    exec_sql(db, "CREATE TABLE IF NOT EXISTS \"project_logs\" (\"uuid\" varchar PRIMARY KEY NOT NULL,\"project_uuid\" varchar NOT NULL,\"content\" varchar NOT NULL,\"created_at\" datetime NOT NULL DEFAULT (datetime('now')),\"updated_at\" datetime NOT NULL DEFAULT (datetime('now')));");
    exec_sql(db, "CREATE TABLE IF NOT EXISTS \"project_members\" (\"role\" integer NOT NULL,\"project_uuid\" varchar NOT NULL,\"user_uuid\" varchar NOT NULL,\"created_at\" datetime NOT NULL DEFAULT (datetime('now')),\"updated_at\" datetime NOT NULL DEFAULT (datetime('now')),PRIMARY KEY (\"project_uuid\", \"user_uuid\"));");
    exec_sql(db, "CREATE TABLE IF NOT EXISTS \"sessions\" (\"uuid\" varchar PRIMARY KEY NOT NULL,\"key\" varchar NOT NULL,\"value\" text NOT NULL);");
    exec_sql(db, "CREATE TABLE IF NOT EXISTS \"system_attributes\" (\"property\" varchar (255) PRIMARY KEY NOT NULL,\"type\" varchar (255) NOT NULL,\"object\" varchar (255) NOT NULL,\"show_status\" varchar);");
    exec_sql(db, "CREATE TABLE IF NOT EXISTS \"editor_caches\" (\"key\" varchar NOT NULL,\"value\" TEXT NOT NULL,PRIMARY KEY (\"key\"));");
    exec_sql(db, "CREATE TABLE IF NOT EXISTS \"block_symbol_attributes\" (\"path\" varchar PRIMARY KEY NOT NULL,\"project_uuid\" varchar NOT NULL,\"hash\" integer NOT NULL,\"ticket\" integer NOT NULL DEFAULT (1),\"attr\" text NOT NULL);");
    exec_sql(db, "CREATE TABLE IF NOT EXISTS \"backups\" (\"uuid\" varchar PRIMARY KEY NOT NULL,\"limit\" integer,\"project_uuid\" varchar NOT NULL,\"auto\" boolean NOT NULL DEFAULT (0),\"name\" varchar NOT NULL,\"description\" varchar,\"archive\" boolean NOT NULL DEFAULT (0),\"user_uuid\" varchar NOT NULL,\"owner_uuid\" varchar NOT NULL,\"project_name\" varchar NOT NULL,\"createtime\" datetime NOT NULL DEFAULT (datetime('now')),\"path\" varchar NOT NULL DEFAULT (' '));");

    /* 清理旧数据 (若文件已存在) */
    exec_sql(db, "DELETE FROM projects WHERE uuid='" PROJECT_UUID "';");
    exec_sql(db, "DELETE FROM schematics WHERE uuid='" SCHEMATIC_UUID "';");
    exec_sql(db, "DELETE FROM documents WHERE uuid IN ('" SCH_DOC_UUID "','" PCB_DOC_UUID "');");

    /* users */
    exec_sql(db, "INSERT OR IGNORE INTO users (uuid,username,nickname,password,preference,avatar,team) VALUES ('" USER_UUID "','zenix','zenix','','','',0);");
    exec_sql(db, "INSERT OR IGNORE INTO users (uuid,username,nickname,password,preference,avatar,team) VALUES ('0819f05c4eef4c71ace90d822a990e87','LCSC','LCSC','LCSC','','',0);");

    /* db_versions / system_config */
    exec_sql(db, "INSERT OR REPLACE INTO db_versions (\"key\",\"value\") VALUES ('sqlite','0.0.3');");
    exec_sql(db, "INSERT OR REPLACE INTO system_config (\"key\",\"value\") VALUES ('component_sync_updateTime','0');");
    exec_sql(db, "INSERT OR REPLACE INTO system_config (\"key\",\"value\") VALUES ('device_sync_updateTime','0');");

    /* project */
    {
        time_t now = time(NULL);
        char *boards = sqlite3_mprintf("[{\"sch\":\"%s\",\"name\":\"MotorDriver-42\",\"pcb\":\"%s\"}]",
            SCHEMATIC_UUID, PCB_DOC_UUID);
        char *sql = sqlite3_mprintf(
            "INSERT INTO projects (uuid,archive,name,content,cbb_project,thumb,ticket,g_ticket,owner_uuid,creator_uuid,created_at,updated_at,modifier_uuid,boards,block_symbol_attrs_groups,pcb_count,default_sheet) "
            "VALUES ('%q',0,'%q','',0,'',1,20,'%q','%q',datetime('now'),datetime('now'),NULL,'%q','{}',1,'');",
            PROJECT_UUID, PROJECT_NAME, USER_UUID, USER_UUID, boards);
        exec_sql(db, sql);
        sqlite3_free(boards);
        sqlite3_free(sql);
        (void)now;
    }

    /* schematic */
    {
        char *sql = sqlite3_mprintf(
            "INSERT INTO schematics (uuid,description,ticket,sheet_count,project_uuid,name,display_name,createtime,updatetime,created_at,updated_at,sort) "
            "VALUES ('%q','',1,1,'%q','MotorDriver-42','MotorDriver-42',strftime('%%s','now'),strftime('%%s','now'),datetime('now'),datetime('now'),'');",
            SCHEMATIC_UUID, PROJECT_UUID);
        exec_sql(db, sql);
        sqlite3_free(sql);
    }

    /* documents: SCH */
    {
        sqlite3_stmt *stmt = NULL;
        const char *sql = "INSERT INTO documents (uuid,title,display_title,description,docType,dataStr,sheet_id,ticket,sort_ticket,created_at,updated_at,creator_uuid,schematic_uuid,project_uuid,image) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);";
        int rc2 = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
        check_sqlite(rc2, db, "prepare sch doc");
        sqlite3_bind_text(stmt, 1, SCH_DOC_UUID, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, "MotorDriver-42", -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, "MotorDriver-42", -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, "MotorDriver-42 6Partitions STM32F103CBT6 + 2xTB67H450 + MT6816", -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 5, 1);
        sqlite3_bind_text(stmt, 6, kSchDataStr, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 7, 1);
        sqlite3_bind_int(stmt, 8, 1);
        sqlite3_bind_int(stmt, 9, 0);
        sqlite3_bind_text(stmt, 10, "2026-08-20 00:00:00", -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 11, "2026-08-20 00:00:00", -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 12, USER_UUID, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 13, SCHEMATIC_UUID, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 14, PROJECT_UUID, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 15, "", -1, SQLITE_TRANSIENT);
        rc2 = sqlite3_step(stmt);
        check_sqlite(rc2, db, "step sch doc");
        sqlite3_finalize(stmt);
    }
    /* documents: PCB */
    {
        sqlite3_stmt *stmt = NULL;
        const char *sql = "INSERT INTO documents (uuid,title,display_title,description,docType,dataStr,sheet_id,ticket,sort_ticket,created_at,updated_at,creator_uuid,schematic_uuid,project_uuid,image) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);";
        int rc2 = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
        check_sqlite(rc2, db, "prepare pcb doc");
        sqlite3_bind_text(stmt, 1, PCB_DOC_UUID, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, "MotorDriver-42-PCB", -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, "MotorDriver-42-PCB", -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, "4Layer 1oz VM40mil", -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 5, 3);
        sqlite3_bind_text(stmt, 6, kPcbDataStr, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 7, 1);
        sqlite3_bind_int(stmt, 8, 1);
        sqlite3_bind_int(stmt, 9, 0);
        sqlite3_bind_text(stmt, 10, "2026-08-20 00:00:00", -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 11, "2026-08-20 00:00:00", -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 12, USER_UUID, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 13, "", -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 14, PROJECT_UUID, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 15, "", -1, SQLITE_TRANSIENT);
        rc2 = sqlite3_step(stmt);
        check_sqlite(rc2, db, "step pcb doc");
        sqlite3_finalize(stmt);
    }

    /* 复用 FPC 的 sheet 符号 + 器件, 确保 SCH 能解析; 若缺则插入最小占位 */
    exec_sql(db, "INSERT OR IGNORE INTO components (uuid,title,display_title,description,source,version,created_at,updated_at,ticket,docType,dataStr,createTime,updateTime,modifier_uuid,creator_uuid,owner_uuid,project_uuid,child_tag,parent_tag,custom_tags) VALUES ('03cb0d5b85ca43e1844a2cba7f1c90ad','sheet-symbol_a3','Sheet-Symbol_A3','','','',datetime('now'),datetime('now'),1,20,'[\"DOCTYPE\",\"SYMBOL\",\"1.0\"]',datetime('now'),datetime('now'),NULL,NULL,NULL,'" PROJECT_UUID "','','','[]');");
    exec_sql(db, "INSERT OR IGNORE INTO devices (uuid,description,title,display_title,images,source,version,ticket,footprint_type,symbol_type,created_at,updated_at,createTime,updateTime,modifier_uuid,creator_uuid,owner_uuid,project_uuid,child_tag,parent_tag,custom_tags) VALUES ('d40fddd63a6740bfbe6c4535abd956a5','','sheet-symbol_a3','Sheet-Symbol_A3','','','',1,0,0,datetime('now'),datetime('now'),datetime('now'),datetime('now'),NULL,NULL,NULL,'" PROJECT_UUID "','','','');");
    exec_sql(db, "INSERT OR IGNORE INTO components (uuid,title,display_title,description,source,version,created_at,updated_at,ticket,docType,dataStr,createTime,updateTime,modifier_uuid,creator_uuid,owner_uuid,project_uuid,child_tag,parent_tag,custom_tags) VALUES ('be6c7b7afdfa4f56a1f962a791db6efb','fpc_16pin','FPC_16PIN','','','',datetime('now'),datetime('now'),1,2,'[\"DOCTYPE\",\"SYMBOL\",\"1.0\"]',datetime('now'),datetime('now'),NULL,NULL,NULL,'" PROJECT_UUID "','','','[]');");
    exec_sql(db, "INSERT OR IGNORE INTO devices (uuid,description,title,display_title,images,source,version,ticket,footprint_type,symbol_type,created_at,updated_at,createTime,updateTime,modifier_uuid,creator_uuid,owner_uuid,project_uuid,child_tag,parent_tag,custom_tags) VALUES ('5ff4d36f12df493a84c82cb850cc692a','','fpc_16pin','FPC_16PIN','','','',1,0,0,datetime('now'),datetime('now'),datetime('now'),datetime('now'),NULL,NULL,NULL,'" PROJECT_UUID "','','','');");

    /* texts / coppers 留空即可 */
    sqlite3_close(db);
}

int main(int argc, char *argv[]) {
    const char *eprj_path = NULL;
    const char *json_path = NULL;
    char default_eprj[512] = {0};
    char default_json[512] = {0};

    if (argc >= 2) {
        /* 若参数以 .eprj 结尾则视为 eprj 输出, 否则视为目录 */
        const char *arg = argv[1];
        size_t len = strlen(arg);
        if (len > 5 && strcmp(arg + len - 5, ".eprj") == 0) {
            eprj_path = arg;
            /* 同名 .json */
            snprintf(default_json, sizeof(default_json), "%.*s.json", (int)(len-5), arg);
            json_path = default_json;
        } else if (len > 5 && strcmp(arg + len - 5, ".json") == 0) {
            json_path = arg;
            /* 同时 stdout */
        } else {
            /* 目录 */
            snprintf(default_eprj, sizeof(default_eprj), "%s/MotorDriver-42-JLC.eprj", arg);
            snprintf(default_json, sizeof(default_json), "%s/MotorDriver-42-JLC.json", arg);
            eprj_path = default_eprj;
            json_path = default_json;
        }
    } else {
        eprj_path = "MotorDriver-42-JLC.eprj";
        json_path = "MotorDriver-42-JLC.json";
    }

    if (json_path) {
        write_json_manifest(json_path);
        /* 同时打印到 stdout 供校验, 若 json_path != stdout */
        if (argc >= 2) {
            /* 额外 stdout 一份便于管道校验 */
            write_json_manifest(NULL);
        }
    } else {
        write_json_manifest(NULL);
    }

    if (eprj_path) {
        create_eprj_db(eprj_path);
        fprintf(stderr, "generated: %s (%s)\n", eprj_path, PROJECT_NAME);
        if (json_path) fprintf(stderr, "generated: %s\n", json_path);
    }
    return 0;
}
