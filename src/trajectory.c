#include "trajectory.h"
#include <math.h>
#include <string.h>

/* 容差常量（与全项目统一） */
#define TRAJ_EPS ZENIX_EPS_F32

/* 角度差归一化到 (-180, 180] —— 复用 common.h 的 zenix_angle_diff_deg，
 * 保持公开符号 traj_angle_diff 兼容旧调用。 */
float traj_angle_diff(float from_deg, float to_deg) {
    return zenix_angle_diff_deg(from_deg, to_deg);
}

/* 角度归一化到 (-180, 180] */
static float normalize_angle(float deg) {
    return zenix_wrap_degree(deg);
}

/* ---------------- 1-D 梯形速度廓线 ---------------- */

traj_profile_t traj_profile_make(float dist, float vmax, float amax) {
    traj_profile_t p;
    memset(&p, 0, sizeof(p));
    p.dist = dist;
    p.vmax = fabsf(vmax);
    p.amax = fabsf(amax);

    float d = fabsf(dist);
    if (d <= TRAJ_EPS || p.vmax <= TRAJ_EPS || p.amax <= TRAJ_EPS) {
        /* 零时长/无效参数: 返回零廓线 */
        p.vmax = 0.0f;
        p.amax = 0.0f;
        p.t_total = 0.0f;
        p.v_peak = 0.0f;
        p.triangle = true;
        return p;
    }

    /* 达到 vmax 所需的位移 = vmax^2 / amax（加速+减速各半） */
    float d_full = (p.vmax * p.vmax) / p.amax;
    if (d >= d_full) {
        /* 梯形：加速-匀速-减速 */
        p.t_acc   = p.vmax / p.amax;
        p.t_cruise = (d - d_full) / p.vmax;
        p.t_total = 2.0f * p.t_acc + p.t_cruise;
        p.v_peak  = p.vmax;
        p.triangle = false;
    } else {
        /* 三角：加速后直接减速，达不到 vmax */
        p.t_acc = sqrtf(d / p.amax);
        p.t_cruise = 0.0f;
        p.t_total = 2.0f * p.t_acc;
        p.v_peak = p.amax * p.t_acc;
        p.triangle = true;
    }
    return p;
}

float traj_profile_sample(const traj_profile_t *p, float t, float *out_vel) {
    if (!p) {
        if (out_vel) *out_vel = 0.0f;
        return 0.0f;
    }

    float s = 0.0f, v = 0.0f;
    float a = p->amax;

    if (p->dist >= 0.0f) {
        if (t <= TRAJ_EPS) {
            s = 0.0f; v = 0.0f;
        } else if (t < p->t_acc) {
            s = 0.5f * a * t * t;
            v = a * t;
        } else if (t < p->t_acc + p->t_cruise) {
            s = 0.5f * a * p->t_acc * p->t_acc + p->v_peak * (t - p->t_acc);
            v = p->v_peak;
        } else if (t < p->t_total) {
            float t_rem = p->t_total - t;
            s = p->dist - 0.5f * a * t_rem * t_rem;
            v = a * t_rem;
        } else {
            s = p->dist;
            v = 0.0f;
        }
    } else {
        /* 负方向：对称翻转 */
        if (t <= TRAJ_EPS) {
            s = 0.0f; v = 0.0f;
        } else if (t < p->t_acc) {
            s = -0.5f * a * t * t;
            v = -a * t;
        } else if (t < p->t_acc + p->t_cruise) {
            s = -(0.5f * a * p->t_acc * p->t_acc + p->v_peak * (t - p->t_acc));
            v = -p->v_peak;
        } else if (t < p->t_total) {
            float t_rem = p->t_total - t;
            s = p->dist + 0.5f * a * t_rem * t_rem;
            v = -a * t_rem;
        } else {
            s = p->dist;
            v = 0.0f;
        }
    }

    if (out_vel) *out_vel = v;
    return s;
}

/* ---------------- 关节空间点对点 (同步多轴) ---------------- */

int traj_joint_plan_init(traj_joint_plan_t *plan,
                         const joint_state_t *q0, const joint_state_t *q1,
                         float vmax_deg, float amax_deg) {
    if (!plan || !q0 || !q1 || vmax_deg <= 0.0f || amax_deg <= 0.0f) {
        return -1;
    }

    memset(plan, 0, sizeof(*plan));
    plan->q0 = *q0;
    plan->q1 = *q1;
    plan->gripper0 = q0->gripper;
    plan->gripper1 = q1->gripper;

    float max_delta = 0.0f;
    for (int i = 0; i < NUM_JOINTS; i++) {
        /* 各轴走最短路径（考虑到 180° 环绕） */
        float d = traj_angle_diff(q0->angles[i], q1->angles[i]);
        plan->delta[i] = d;
        float ad = fabsf(d);
        if (ad > max_delta) max_delta = ad;
    }

    if (max_delta <= TRAJ_EPS) {
        /* 起点=终点：零时长计划 */
        plan->max_delta = 0.0f;
        plan->profile = traj_profile_make(0.0f, vmax_deg, amax_deg);
        return 1;
    }

    plan->max_delta = max_delta;
    plan->profile = traj_profile_make(max_delta, vmax_deg, amax_deg);
    return 0;
}

int traj_joint_plan_sample(const traj_joint_plan_t *plan, float t,
                           joint_state_t *out_js, float *out_vel_deg) {
    if (!plan || !out_js) return -1;

    float vel = 0.0f;
    if (plan->max_delta <= TRAJ_EPS) {
        *out_js = plan->q1;
        if (out_vel_deg) *out_vel_deg = 0.0f;
        return 0;
    }

    /* 以最大轴位移驱动同步廓线 */
    float s = traj_profile_sample(&plan->profile, t, &vel);
    float frac = s / plan->max_delta;

    for (int i = 0; i < NUM_JOINTS; i++) {
        /* 最短路径位移可能跨过 ±180° 边界（如 170°->-170°），
         * 采样后需归一化回 (-180,180] 以保证终点精确等于 q1 */
        out_js->angles[i] = normalize_angle(plan->q0.angles[i] + frac * plan->delta[i]);
    }
    out_js->gripper = plan->gripper0 + frac * (plan->gripper1 - plan->gripper0);

    if (out_vel_deg) *out_vel_deg = vel;
    return 0;
}

/* ---------------- 笛卡尔空间直线运动 ---------------- */

/* clamp_float: 复用 common.h 的 zenix_clampf */
static float clamp_float(float v, float lo, float hi) {
    return zenix_clampf(v, lo, hi);
}

int traj_line_plan_init(traj_line_plan_t *plan,
                        const pose6d_t *p0, const pose6d_t *p1,
                        float vmax_mm, float amax_mm) {
    if (!plan || !p0 || !p1 || vmax_mm <= 0.0f || amax_mm <= 0.0f) {
        return -1;
    }

    memset(plan, 0, sizeof(*plan));
    plan->p0 = *p0;
    plan->p1 = *p1;

    float dx = p1->x - p0->x;
    float dy = p1->y - p0->y;
    float dz = p1->z - p0->z;
    plan->path_len = sqrtf(dx * dx + dy * dy + dz * dz);
    if (plan->path_len <= TRAJ_EPS) {
        plan->profile = traj_profile_make(0.0f, vmax_mm, amax_mm);
        return 1;
    }

    /* 欧拉角走最短路径，避免跨 180° 时反向绕行 */
    plan->rot_delta[0] = traj_angle_diff(p0->roll,  p1->roll);
    plan->rot_delta[1] = traj_angle_diff(p0->pitch, p1->pitch);
    plan->rot_delta[2] = traj_angle_diff(p0->yaw,   p1->yaw);

    plan->profile = traj_profile_make(plan->path_len, vmax_mm, amax_mm);
    return 0;
}

int traj_line_plan_sample(const traj_line_plan_t *plan, float t,
                          pose6d_t *out_pose, float *out_vel_mm) {
    if (!plan || !out_pose) return -1;

    float vel = 0.0f;
    if (plan->path_len <= TRAJ_EPS) {
        *out_pose = plan->p1;
        if (out_vel_mm) *out_vel_mm = 0.0f;
        return 0;
    }

    float s = traj_profile_sample(&plan->profile, t, &vel);
    float frac = clamp_float(s / plan->path_len, 0.0f, 1.0f);

    out_pose->x = plan->p0.x + frac * (plan->p1.x - plan->p0.x);
    out_pose->y = plan->p0.y + frac * (plan->p1.y - plan->p0.y);
    out_pose->z = plan->p0.z + frac * (plan->p1.z - plan->p0.z);
    out_pose->roll  = plan->p0.roll  + frac * plan->rot_delta[0];
    out_pose->pitch = plan->p0.pitch + frac * plan->rot_delta[1];
    out_pose->yaw   = plan->p0.yaw   + frac * plan->rot_delta[2];

    if (out_vel_mm) *out_vel_mm = vel;
    return 0;
}

/* ---------------- IK 分支连续性辅助 ---------------- */

ik_status_t traj_ik_closest(const pose6d_t *target,
                            const joint_state_t *last_joints,
                            const ik_config_t *cfg,
                            joint_state_t *out_js) {
    if (!target || !out_js) return IK_ERR_INVALID_PARAM;

    ik_config_t c = cfg ? *cfg : kinematics_get_default_config();
    if (!last_joints) {
        /* 无历史状态：退化为默认求解器 */
        return kinematics_inverse_6dof(target, &c, out_js);
    }

    ik_solutions_t solves;
    ik_status_t rc = kinematics_inverse_all(target, last_joints, &c, &solves);
    if (rc != IK_SUCCESS) return rc;

    /* 与 6-DOF IK 共用同一套选解逻辑（策略: 距 last_joints 最近） */
    return kinematics_select_solution(&solves, &c,
                                      SOL_SELECT_CLOSEST_TO_LAST,
                                      last_joints, out_js);
}

int traj_line_plan_sample_joints(const traj_line_plan_t *plan, float t,
                                 const joint_state_t *last,
                                 const ik_config_t *cfg,
                                 joint_state_t *out_js) {
    if (!plan || !out_js) return -1;

    pose6d_t pose;
    if (traj_line_plan_sample(plan, t, &pose, NULL) != 0) return -1;

    if (last) {
        return (int)traj_ik_closest(&pose, last, cfg, out_js);
    }
    /* 无历史状态：以零位为起始（不悬垂，消除 dangling-pointer 告警） */
    joint_state_t zero;
    memset(&zero, 0, sizeof(zero));
    return (int)traj_ik_closest(&pose, &zero, cfg, out_js);
}