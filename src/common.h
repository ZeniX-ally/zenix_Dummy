#ifndef ZENIX_COMMON_H
#define ZENIX_COMMON_H

/*
 * common.h — 跨模块公共常量与纯数学工具（header-only）
 *
 * 设计目的：
 *   - 统一浮点容差 / 角度转换 / 环绕归一化 / 钳位等基础操作，
 *     避免 kinematics / trajectory / protocol 各自重复实现；
 *   - 新模块通过 #include "common.h" 即可复用，无需额外依赖。
 *
 * 约定：
 *   - 所有函数为 static inline，不产生额外符号，可放心在单文件程序里使用。
 *   - 涉及角度统一使用"度(deg)"或"弧度(rad)"的有意义命名，避免混淆。
 */

#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 圆周率与浮点容差（单精度 float 建议用 ~1e-6 量级） */
#define ZENIX_PI        3.14159265358979323846
#define ZENIX_PI_2      (ZENIX_PI / 2.0)
#define ZENIX_EPS_F32   1e-6f
#define ZENIX_EPS_ANG   1e-3f   /* 角度容差（度） */

/* 角度单位换算 */
static inline float zenix_deg_to_rad(float deg) {
    return (float)((deg) * (ZENIX_PI / 180.0));
}

static inline float zenix_rad_to_deg(float rad) {
    return (float)((rad) * (180.0 / ZENIX_PI));
}

/* 把角度(度)环绕归一化到 (-180, 180] 区间 */
static inline float zenix_wrap_degree(float deg) {
    while (deg > 180.0f) deg -= 360.0f;
    while (deg <= -180.0f) deg += 360.0f;
    return deg;
}

/* 把角度(弧度)环绕归一化到 (-PI, PI] 区间 */
static inline float zenix_wrap_radian(float rad) {
    while (rad > (float)ZENIX_PI) rad -= (float)(2.0 * ZENIX_PI);
    while (rad <= -(float)ZENIX_PI) rad += (float)(2.0 * ZENIX_PI);
    return rad;
}

/* 角度差(度)归一化到 (-180, 180]，即"从 from 到 to 的最短角位移" */
static inline float zenix_angle_diff_deg(float from_deg, float to_deg) {
    return zenix_wrap_degree(to_deg - from_deg);
}

/* 通用浮点钳位 */
static inline float zenix_clampf(float v, float lo, float hi) {
    return (v < lo) ? lo : ((v > hi) ? hi : v);
}

#ifdef __cplusplus
}
#endif

#endif /* ZENIX_COMMON_H */
