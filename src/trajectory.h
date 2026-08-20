#ifndef TRAJECTORY_H
#define TRAJECTORY_H

#include "kinematics.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- 1-D 梯形速度廓线 (Trapezoidal Velocity Profile) ---------- */

typedef struct {
    float dist;      /* 总位移（带符号，可为正负） */
    float vmax;      /* 设定的峰值速度上限 */
    float amax;      /* 加速度（取绝对值） */
    float t_acc;     /* 加速段时间 (s) */
    float t_cruise;  /* 匀速段时间 (s)，三角廓线时为 0 */
    float t_total;   /* 总时长 (s) */
    float v_peak;    /* 实际峰值速度（短位移无匀速段时 < vmax） */
    bool  triangle;  /* 是否为三角廓线（无匀速段） */
} traj_profile_t;

/* 根据位移/限速/限加速度构造梯形速度廓线。
 * vmax/amax <= 0 或 位移为 0 时返回零时长廓线。 */
traj_profile_t traj_profile_make(float dist, float vmax, float amax);

/* 在时刻 t 采样 1-D 廓线。
 * 返回带符号位移；out_vel 非空时输出带符号速度。
 * t >= t_total 时钳位到终点，t < 0 时按起点处理。 */
float traj_profile_sample(const traj_profile_t *p, float t, float *out_vel);

/* ---------- 关节空间点对点运动 (同步多轴) ---------- */

typedef struct {
    joint_state_t    q0;         /* 起始关节角 */
    joint_state_t    q1;         /* 目标关节角 */
    float            delta[NUM_JOINTS]; /* 各轴最短路径位移（已做 180° 环绕处理） */
    float            max_delta;  /* 最大轴位移（决定全程时长） */
    float            gripper0;   /* 起始夹爪位置 */
    float            gripper1;   /* 目标夹爪位置 */
    traj_profile_t   profile;    /* 基于 max_delta 的同步廓线 */
} traj_joint_plan_t;

/* 构造关节空间 P2P 计划：各轴按最短路径（-180°..180°）同步运动。
 * 运动时长由位移最大的轴决定，其余轴按比例跟随，保证同时启停。
 * 返回 0 成功；q0/q1 为空或 vmax/amax 非法返回 -1；
 * 起点=终点时返回 1（零时长计划，仍可安全采样）。 */
int traj_joint_plan_init(traj_joint_plan_t *plan,
                         const joint_state_t *q0, const joint_state_t *q1,
                         float vmax_deg, float amax_deg);

/* 采样关节空间计划。out_vel_deg 非空时输出 max_delta 轴的实时速度。 */
int traj_joint_plan_sample(const traj_joint_plan_t *plan, float t,
                           joint_state_t *out_js, float *out_vel_deg);

/* ---------- 笛卡尔空间直线运动 ---------- */

typedef struct {
    pose6d_t       p0;        /* 起始位姿 */
    pose6d_t       p1;        /* 目标位姿 */
    float          path_len;  /* 平移路径长度 (mm) */
    float          rot_delta[3]; /* 各欧拉角最短角位移 (deg) */
    traj_profile_t profile;   /* 基于平移路径长度的廓线 */
} traj_line_plan_t;

/* 构造笛卡尔直线计划：位置线性插值 + 欧拉角最短路径线性插值。
 * 返回 0 成功；参数非法返回 -1；两点重合返回 1。 */
int traj_line_plan_init(traj_line_plan_t *plan,
                        const pose6d_t *p0, const pose6d_t *p1,
                        float vmax_mm, float amax_mm);

/* 采样笛卡尔直线计划。out_vel_mm 非空时输出实时线速度。 */
int traj_line_plan_sample(const traj_line_plan_t *plan, float t,
                          pose6d_t *out_pose, float *out_vel_mm);

/* ---------- IK 分支连续性辅助 ---------- */

/* 角度差归一化到 (-180, 180]（用于最短路径与分支选择） */
float traj_angle_diff(float from_deg, float to_deg);

/* 逆解并选择"距 last_joints 最近"且在软限位内的分支（保证轨迹连续）。
 * last_joints 为空时退化为 kinematics_inverse_6dof() 行为。 */
ik_status_t traj_ik_closest(const pose6d_t *target,
                            const joint_state_t *last_joints,
                            const ik_config_t *cfg,
                            joint_state_t *out_js);

/* 笛卡尔直线采样 + 逆解为关节角（逐点跟踪，保持分支连续）。
 * last 为上一采样点的关节角，用于连续性约束；为 NULL 时用零位初始化。 */
int traj_line_plan_sample_joints(const traj_line_plan_t *plan, float t,
                                 const joint_state_t *last,
                                 const ik_config_t *cfg,
                                 joint_state_t *out_js);

#ifdef __cplusplus
}
#endif

#endif /* TRAJECTORY_H */