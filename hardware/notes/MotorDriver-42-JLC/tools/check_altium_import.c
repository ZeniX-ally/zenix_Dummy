/*
 * check_altium_import.c — MotorDriver-42 原工程一致性校验 (C11)
 *
 * 用途: 列出 /home/zenix/Dummy-Robot/1.Hardware/MotorDriver-42/ 下
 *       全部 Altium 源文件大小 + md5，供导入嘉立创前后对比，防误改。
 * 编译: gcc -std=c11 -O2 -Wall -Wextra -o check_altium_import check_altium_import.c
 * 运行: ./check_altium_import [原工程路径]
 *       默认路径 /home/zenix/Dummy-Robot/1.Hardware/MotorDriver-42
 * 校验: md5  via `md5sum` (coreutils)，大小 via stat
 * 退出码: 0 全 OK，1 有文件缺失或 mismatch
 *
 * 约束: 仅 C 标准库 + POSIX popen/stat，不依赖 openssl。
 */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <limits.h>

#define DEFAULT_SRC "/home/zenix/Dummy-Robot/1.Hardware/MotorDriver-42"

struct Expected {
    const char *rel;
    long size;
    const char *md5;
};

/* 2026-08-20 实测值 (du -b + md5sum) */
static const struct Expected kExpected[] = {
    {"Motor-42.PrjPCB",          49541, "71ee436d59c97bbd79f420841016853b"},
    {"Motor-42.PrjPCBStructure",   421, "7f33a8700860b265354adcf52abada57"},
    {"Power.SchDoc",             84480, "abda5b50ed75d4314d0866820bf086bb"},
    {"MCU.SchDoc",               95744, "afbdec9b0fd45ad33ea0b0761bff72ad"},
    {"Port.SchDoc",              66048, "5e37b95ac3aa31332f65105d28dffbca"},
    {"Interface.SchDoc",         67584, "0f6a120fc7e0c87eb1e4278543c5e174"},
    {"Drive.SchDoc",             59904, "5e0f79a8e5f232d51c81779d02808e89"},
    {"Motor-42.PcbDoc",        5576704, "a7c42448a967938cf96db674df9dd110"},
    {"Motor-42.pdf",            234364, "497207685e29646086888ee108227d6d"},
};

/* 格式化 bytes → human (1024) */
static void human_size(long bytes, char *out, size_t n) {
    const char *units[] = {"B","K","M","G"};
    double v = (double)bytes;
    int u = 0;
    while (v >= 1024 && u < 3) { v /= 1024; u++; }
    if (u == 0) snprintf(out, n, "%ld B", bytes);
    else if (v < 10) snprintf(out, n, "%.1f%s", v, units[u]);
    else snprintf(out, n, "%.0f%s", v, units[u]);
}

/* 取 md5 via md5sum，无文件返回 "" */
static void md5_of(const char *path, char *out, size_t n) {
    char cmd[PATH_MAX + 32];
    snprintf(cmd, sizeof(cmd), "md5sum \"%s\" 2>/dev/null", path);
    FILE *fp = popen(cmd, "r");
    if (!fp) { out[0] = '\0'; return; }
    char buf[128] = {0};
    if (fgets(buf, sizeof(buf), fp)) {
        buf[32] = '\0';
        strncpy(out, buf, n-1);
        out[n-1] = '\0';
    } else {
        out[0] = '\0';
    }
    pclose(fp);
}

static int check_one(const char *base, const struct Expected *e, int *mismatch) {
    char path[PATH_MAX + 64];
    snprintf(path, sizeof(path), "%s/%s", base, e->rel);
    struct stat st;
    if (stat(path, &st) != 0) {
        printf("[MISS] %-24s  (not found: %s)\n", e->rel, path);
        (*mismatch)++;
        return -1;
    }
    long sz = (long)st.st_size;
    char md5[64] = {0};
    md5_of(path, md5, sizeof(md5));
    char hsz[32], hexp[32];
    human_size(sz, hsz, sizeof(hsz));
    human_size(e->size, hexp, sizeof(hexp));

    int sz_ok = (sz == e->size);
    int md5_ok = (md5[0] && e->md5[0] && strcmp(md5, e->md5) == 0);

    const char *tag = (sz_ok && md5_ok) ? "[ OK ]" : "[DIFF]";
    if (!sz_ok || !md5_ok) (*mismatch)++;

    printf("%s %-24s %8ld bytes (%5s)  md5=%s  expected %s (%s) %s%s\n",
           tag, e->rel, sz, hsz, md5[0]?md5:"(no md5)",
           e->md5, hexp,
           sz_ok ? "" : " SIZE!",
           md5_ok ? "" : " MD5!");

    /* 额外提示 PcbDoc 二进制头 */
    if (strcmp(e->rel, "Motor-42.PcbDoc")==0 || strcmp(e->rel, "Power.SchDoc")==0) {
        if (sz > 0) {
            FILE *f = fopen(path, "rb");
            if (f) {
                unsigned char hdr[8];
                if (fread(hdr,1,8,f)==8) {
                    int is_ole = (hdr[0]==0xD0 && hdr[1]==0xCF && hdr[2]==0x11 && hdr[3]==0xE0);
                    if (!is_ole) printf("       ^ not OLE compound (expected D0 CF 11 E0)\n");
                }
                fclose(f);
            }
        }
    }
    return 0;
}

static void list_gerber(const char *base, int *mismatch) {
    char gpath[PATH_MAX];
    snprintf(gpath, sizeof(gpath), "%s/Gerber", base);
    struct stat st;
    if (stat(gpath, &st)!=0 || !S_ISDIR(st.st_mode)) {
        printf("[MISS] Gerber/  (not found)\n");
        (*mismatch)++;
        return;
    }
    DIR *d = opendir(gpath);
    if (!d) { printf("[MISS] Gerber/ opendir failed\n"); (*mismatch)++; return; }
    struct dirent *ent;
    int count = 0;
    long total = 0;
    printf("\n--- Gerber/ (28 files expected) ---\n");
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0]=='.') continue;
        char fpath[PATH_MAX + 512];
        snprintf(fpath, sizeof(fpath), "%s/%s", gpath, ent->d_name);
        struct stat fst;
        if (stat(fpath, &fst)!=0) continue;
        if (!S_ISREG(fst.st_mode)) continue;
        char md5[64]={0};
        md5_of(fpath, md5, sizeof(md5));
        char hsz[32];
        human_size((long)fst.st_size, hsz, sizeof(hsz));
        printf("  %-28s %8ld (%5s)  md5=%.8s...\n", ent->d_name, (long)fst.st_size, hsz, md5);
        count++; total += (long)fst.st_size;
    }
    closedir(d);
    char htot[32];
    human_size(total, htot, sizeof(htot));
    printf("  => %d files, total %ld bytes (%s)  (expected ~28 files, ~992K)\n", count, total, htot);
    if (count < 20) {
        printf("  [WARN] Gerber file count low, may be incomplete export\n");
    }
}

int main(int argc, char *argv[]) {
    const char *base = (argc >= 2) ? argv[1] : DEFAULT_SRC;

    printf("MotorDriver-42 Altium 原工程校验\n");
    printf(" base: %s\n", base);
    printf(" date: 2026-08-20  (expected values baked in)\n");
    printf(" tool: check_altium_import.c  C11  gcc -std=c11 -O2 -Wall -Wextra\n");
    printf("------------------------------------------------------------\n");

    struct stat st;
    if (stat(base, &st)!=0 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "error: base not a directory: %s\n", base);
        fprintf(stderr, "usage: %s [path/to/MotorDriver-42]\n", argv[0]);
        return 1;
    }

    int mismatch = 0;
    size_t n = sizeof(kExpected)/sizeof(kExpected[0]);
    for (size_t i=0;i<n;i++) {
        check_one(base, &kExpected[i], &mismatch);
    }
    list_gerber(base, &mismatch);

    printf("------------------------------------------------------------\n");
    if (mismatch==0) {
        printf("Result: ALL OK (%zu files + Gerber). 原工程与 2026-08-20 基准一致，可放心导入。\n", n);
        printf("Next: 嘉立创EDA专业版 -> 文件 -> 导入 -> Altium Designer -> 选 %s/Motor-42.PrjPCB\n", base);
    } else {
        printf("Result: %d DIFF/MISS. 若为 size/md5 变化，说明原文件被改动；请从备份恢复后再导入。\n", mismatch);
        printf("Backup example: cp -a %s ~/MotorDriver-42-backup-$(date +%%Y%%m%%d)/\n", base);
    }
    printf("Ref: hardware/notes/MotorDriver-42-JLC/IMPORT_ALTIUM.md\n");
    return mismatch ? 1 : 0;
}
