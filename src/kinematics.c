#include "kinematics.h"
#include <math.h>
#include <string.h>

/* 常量与角度工具统一取自 common.h（经 kinematics.h 引入）。
 * 文件内保留 M_PI / 角度换算的轻量别名，避免大范围改动既有表达式。 */
#ifndef M_PI
#define M_PI ZENIX_PI
#endif

#ifndef M_PI_2
#define M_PI_2 ZENIX_PI_2
#endif

#define DEG_TO_RAD(d) zenix_deg_to_rad(d)
#define RAD_TO_DEG(r) zenix_rad_to_deg(r)
#define EPSILON       ZENIX_EPS_F32

/* ------------------ 关节软限位（运行时可配置） ------------------
 * 默认限位定义在 default_joint_limits()，
 * 通过 kinematics_set_limits() 即可为不同机型/关节覆盖，无需改源码。
 * 所有限位相关接口（is_joint_within_limits / clamp_joint_angles）遵循当前值。 */

static joint_limits_t g_kin_limits;       /* 当前生效限位 */
static bool          g_kin_limits_init = false;

static joint_limits_t default_joint_limits(void) {
    static const float MIN_DEG[NUM_JOINTS] = {
        -180.0f, -100.0f, -150.0f, -180.0f, -120.0f, -360.0f
    };
    static const float MAX_DEG[NUM_JOINTS] = {
        180.0f,  100.0f, 150.0f,  180.0f,  120.0f,  360.0f
    };
    joint_limits_t lim;
    for (int i = 0; i < NUM_JOINTS; i++) {
        lim.min_deg[i] = MIN_DEG[i];
        lim.max_deg[i] = MAX_DEG[i];
    }
    return lim;
}

static const joint_limits_t *current_limits(void) {
    if (!g_kin_limits_init) {
        g_kin_limits = default_joint_limits();
        g_kin_limits_init = true;
    }
    return &g_kin_limits;
}

joint_limits_t kinematics_get_default_limits(void) {
    return default_joint_limits();
}

void kinematics_get_limits(joint_limits_t *out_limits) {
    if (out_limits) {
        *out_limits = *current_limits();
    }
}

int kinematics_set_limits(const joint_limits_t *limits) {
    if (!limits) return -1;
    g_kin_limits = *limits;
    g_kin_limits_init = true;
    return 0;
}

/* ------------------ 角度工具（统一入口） ------------------ */

float kinematics_wrap_degree(float deg) { return zenix_wrap_degree(deg); }
float kinematics_wrap_radian(float rad) { return zenix_wrap_radian(rad); }
float kinematics_deg_to_rad(float deg)  { return zenix_deg_to_rad(deg); }
float kinematics_rad_to_deg(float rad)  { return zenix_rad_to_deg(rad); }

ik_config_t kinematics_get_default_config(void) {
    ik_config_t cfg;
    cfg.l_bs = DUMMY_L_BASE;
    cfg.d_bs = DUMMY_D_BASE;
    cfg.l_arm = DUMMY_L_UPPER_ARM;
    cfg.l_fa = DUMMY_L_FOREARM;
    cfg.d_ew = DUMMY_D_ELBOW;
    cfg.l_wt = DUMMY_L_WRIST;
    cfg.arm_config = 0; /* 0: 肘上/主分支 */
    return cfg;
}

bool is_joint_within_limits(int joint_idx, float angle_deg) {
    if (joint_idx < 0 || joint_idx >= NUM_JOINTS) {
        return false;
    }
    const joint_limits_t *lim = current_limits();
    return (angle_deg >= lim->min_deg[joint_idx] - ZENIX_EPS_ANG &&
            angle_deg <= lim->max_deg[joint_idx] + ZENIX_EPS_ANG);
}

void clamp_joint_angles(joint_state_t *js) {
    if (!js) return;
    const joint_limits_t *lim = current_limits();
    for (int i = 0; i < NUM_JOINTS; i++) {
        if (js->angles[i] < lim->min_deg[i]) {
            js->angles[i] = lim->min_deg[i];
        } else if (js->angles[i] > lim->max_deg[i]) {
            js->angles[i] = lim->max_deg[i];
        }
    }
}

void clamp_gripper(joint_state_t *js) {
    if (!js) return;
    if (js->gripper < 0.0f) js->gripper = 0.0f;
    if (js->gripper > 1.0f) js->gripper = 1.0f;
}

int forward_kinematics(const joint_state_t *js, float link_len,
                       float *x, float *y, float *z) {
    if (!js || !x || !y || !z) return -1;
    float theta1 = DEG_TO_RAD(js->angles[0]);
    float theta2 = DEG_TO_RAD(js->angles[1]);
    float theta3 = DEG_TO_RAD(js->angles[2]);

    float r = link_len * (cosf(theta2) + cosf(theta2 + theta3));
    *x = r * cosf(theta1);
    *y = r * sinf(theta1);
    *z = link_len * (sinf(theta2) + sinf(theta2 + theta3));
    return 0;
}

/* ------------------ DH 参数表（单一事实来源） ------------------
 * FK / IK 都从这里构建 Dummy 6-DOF DH 表，避免两处手写不一致。
 * 匹配 Dummy Robot 6dof_kinematic 标定体系。 */
typedef struct {
    float theta0;  /* 关节零位偏移 (rad) */
    float a;       /* 连杆长度 */
    float d;       /* 连杆偏置 */
    float alpha;   /* 连杆扭转 (rad) */
} dh_link_t;

static void build_dh_table(const ik_config_t *cfg, dh_link_t dh[6]) {
    dh[0] = (dh_link_t){0.0f,            cfg->l_bs,  cfg->d_bs, -(float)M_PI_2};
    dh[1] = (dh_link_t){-(float)M_PI_2,  0.0f,       cfg->l_arm, 0.0f};
    dh[2] = (dh_link_t){(float)M_PI_2,   cfg->d_ew,  0.0f,       (float)M_PI_2};
    dh[3] = (dh_link_t){0.0f,            cfg->l_fa,  0.0f,       -(float)M_PI_2};
    dh[4] = (dh_link_t){0.0f,            0.0f,       0.0f,       (float)M_PI_2};
    dh[5] = (dh_link_t){0.0f,            cfg->l_wt,  0.0f,       0.0f};
}

/* 矩阵乘法辅助 */
static void mat_multiply_3x3(const float a[9], const float b[9], float res[9]) {
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) {
            res[r * 3 + c] = a[r * 3 + 0] * b[0 * 3 + c] +
                             a[r * 3 + 1] * b[1 * 3 + c] +
                             a[r * 3 + 2] * b[2 * 3 + c];
        }
    }
}

static void mat_multiply_3x1(const float m[9], const float v[3], float res[3]) {
    for (int r = 0; r < 3; r++) {
        res[r] = m[r * 3 + 0] * v[0] +
                 m[r * 3 + 1] * v[1] +
                 m[r * 3 + 2] * v[2];
    }
}

/* 欧拉角转旋转矩阵 (XYZ 内旋或对应机器人算法规范) */
static void euler_to_rot_mat(const float euler[3], float rot[9]) {
    float a = euler[0], b = euler[1], c = euler[2];
    float ca = cosf(a), sa = sinf(a);
    float cb = cosf(b), sb = sinf(b);
    float cc = cosf(c), sc = sinf(c);

    rot[0] = ca * cb;
    rot[1] = ca * sb * sc - sa * cc;
    rot[2] = ca * sb * cc + sa * sc;
    rot[3] = sa * cb;
    rot[4] = sa * sb * sc + ca * cc;
    rot[5] = sa * sb * cc - ca * sc;
    rot[6] = -sb;
    rot[7] = cb * sc;
    rot[8] = cb * cc;
}

/* 旋转矩阵转欧拉角 */
static void rot_mat_to_euler(const float rot[9], float euler[3]) {
    if (rot[6] >= 1.0f - 1e-6f) {
        euler[0] = 0.0f;
        euler[1] = -(float)M_PI_2;
        euler[2] = atan2f(-rot[1], rot[4]);
    } else if (rot[6] <= -1.0f + 1e-6f) {
        euler[0] = 0.0f;
        euler[1] = (float)M_PI_2;
        euler[2] = atan2f(rot[1], rot[4]);
    } else {
        euler[1] = asinf(-rot[6]);
        float cb = cosf(euler[1]);
        euler[0] = atan2f(rot[3] / cb, rot[0] / cb);
        euler[2] = atan2f(rot[7] / cb, rot[8] / cb);
    }
}

int kinematics_forward_6dof(const joint_state_t *js, const ik_config_t *cfg, pose6d_t *out_pose) {
    if (!js || !out_pose) return -1;
    ik_config_t c = cfg ? *cfg : kinematics_get_default_config();

    /* DH 参数单一来源（build_dh_table），与 IK 共用同一张表 */
    dh_link_t dh[6];
    build_dh_table(&c, dh);

    float l1_base[3] = { c.d_bs, -c.l_bs, 0.0f };
    float l2_arm[3]  = { c.l_arm, 0.0f, 0.0f };
    float l3_elbow[3]= { -c.d_ew, 0.0f, c.l_fa };
    float l6_wrist[3]= { 0.0f, 0.0f, c.l_wt };

    float q[6];
    float r_mats[6][9];
    for (int i = 0; i < 6; i++) {
        q[i] = DEG_TO_RAD(js->angles[i]) + dh[i].theta0;
        float cosq = cosf(q[i]), sinq = sinf(q[i]);
        float cosa = cosf(dh[i].alpha), sina = sinf(dh[i].alpha);

        r_mats[i][0] = cosq;
        r_mats[i][1] = -cosa * sinq;
        r_mats[i][2] = sina * sinq;
        r_mats[i][3] = sinq;
        r_mats[i][4] = cosa * cosq;
        r_mats[i][5] = -sina * cosq;
        r_mats[i][6] = 0.0f;
        r_mats[i][7] = sina;
        r_mats[i][8] = cosa;
    }

    float r02[9], r03[9], r04[9], r05[9], r06[9];
    mat_multiply_3x3(r_mats[0], r_mats[1], r02);
    mat_multiply_3x3(r02, r_mats[2], r03);
    mat_multiply_3x3(r03, r_mats[3], r04);
    mat_multiply_3x3(r04, r_mats[4], r05);
    mat_multiply_3x3(r05, r_mats[5], r06);

    float l0_bs[3], l0_se[3], l0_ew[3], l0_wt[3];
    mat_multiply_3x1(r_mats[0], l1_base, l0_bs);
    mat_multiply_3x1(r02, l2_arm, l0_se);
    mat_multiply_3x1(r03, l3_elbow, l0_ew);
    mat_multiply_3x1(r06, l6_wrist, l0_wt);

    float p06[3];
    for (int i = 0; i < 3; i++) {
        p06[i] = l0_bs[i] + l0_se[i] + l0_ew[i] + l0_wt[i];
    }

    float euler[3];
    rot_mat_to_euler(r06, euler);

    out_pose->x = p06[0];
    out_pose->y = p06[1];
    out_pose->z = p06[2];
    out_pose->roll  = RAD_TO_DEG(euler[0]);
    out_pose->pitch = RAD_TO_DEG(euler[1]);
    out_pose->yaw   = RAD_TO_DEG(euler[2]);

    return 0;
}

/* 完整解析几何逆解求解器 (求解全部 8 组解) */
ik_status_t kinematics_inverse_all(const pose6d_t *target_pose,
                                  const joint_state_t *last_joints,
                                  const ik_config_t *cfg,
                                  ik_solutions_t *out_solves) {
    if (!target_pose || !out_solves) return IK_ERR_INVALID_PARAM;

    ik_config_t c = cfg ? *cfg : kinematics_get_default_config();
    memset(out_solves, 0, sizeof(ik_solutions_t));
    out_solves->count = 8;

    /* DH 参数单一来源（build_dh_table），与 FK 共用同一张表 */
    dh_link_t dh[6];
    build_dh_table(&c, dh);

    float l1_base[3] = { c.d_bs, -c.l_bs, 0.0f };
    float l6_wrist[3]= { 0.0f, 0.0f, c.l_wt };

    float l_se = c.l_arm;
    float l_se_2 = l_se * l_se;
    float l_ew_2 = c.l_fa * c.l_fa + c.d_ew * c.d_ew;
    float l_ew = sqrtf(l_ew_2);
    float atan_e = (c.l_fa > 1e-4f) ? atanf(c.d_ew / c.l_fa) : 0.0f;

    float p06[3] = { target_pose->x, target_pose->y, target_pose->z };
    float euler[3] = {
        DEG_TO_RAD(target_pose->roll),
        DEG_TO_RAD(target_pose->pitch),
        DEG_TO_RAD(target_pose->yaw)
    };

    float r06[9];
    euler_to_rot_mat(euler, r06);

    float qs[2];
    float qa[2][2];
    float qw[2][3];

    float last_a[6] = {0};
    if (last_joints) {
        for (int i = 0; i < 6; i++) {
            last_a[i] = DEG_TO_RAD(last_joints->angles[i]);
        }
    }

    float l0_wt[3];
    mat_multiply_3x1(r06, l6_wrist, l0_wt);

    float p0_w[3];
    for (int i = 0; i < 3; i++) {
        p0_w[i] = p06[i] - l0_wt[i];
    }

    float r_xy = sqrtf(p0_w[0] * p0_w[0] + p0_w[1] * p0_w[1]);
    if (r_xy <= 1e-6f) {
        qs[0] = last_a[0];
        qs[1] = last_a[0];
        for (int i = 0; i < 4; i++) {
            out_solves->sol_flags[0 + i][0] = -1; /* 奇异点 */
            out_solves->sol_flags[4 + i][0] = -1;
        }
    } else {
        qs[0] = atan2f(p0_w[1], p0_w[0]);
        qs[1] = atan2f(-p0_w[1], -p0_w[0]);
        for (int i = 0; i < 4; i++) {
            out_solves->sol_flags[0 + i][0] = 1;
            out_solves->sol_flags[4 + i][0] = 1;
        }
    }

    for (int ind_arm = 0; ind_arm < 2; ind_arm++) {
        float cosqs = cosf(qs[ind_arm] + dh[0].theta0);
        float sinqs = sinf(qs[ind_arm] + dh[0].theta0);

        float r10[9] = {
            cosqs,  sinqs,  0.0f,
            0.0f,   0.0f,  -1.0f,
           -sinqs,  cosqs,  0.0f
        };

        float p1_w[3];
        mat_multiply_3x1(r10, p0_w, p1_w);

        float l1_sw[3];
        for (int i = 0; i < 3; i++) {
            l1_sw[i] = p1_w[i] - l1_base[i];
        }

        float l_sw_2 = l1_sw[0] * l1_sw[0] + l1_sw[1] * l1_sw[1];
        float l_sw = sqrtf(l_sw_2);

        if (fabsf(l_se + l_ew - l_sw) <= 1e-5f) {
            qa[0][0] = atan2f(l1_sw[1], l1_sw[0]);
            qa[1][0] = qa[0][0];
            qa[0][1] = 0.0f;
            qa[1][1] = 0.0f;
            int flag = (l_sw > l_se + l_ew) ? 0 : 1;
            for (int i = 0; i < 2; i++) {
                out_solves->sol_flags[4 * ind_arm + 0 + i][1] = flag;
                out_solves->sol_flags[4 * ind_arm + 2 + i][1] = flag;
            }
        } else if (fabsf(l_sw - fabsf(l_se - l_ew)) <= 1e-5f) {
            qa[0][0] = atan2f(l1_sw[1], l1_sw[0]);
            qa[1][0] = qa[0][0];
            qa[0][1] = (ind_arm == 0) ? (float)M_PI : -(float)M_PI;
            qa[1][1] = (ind_arm == 0) ? -(float)M_PI : (float)M_PI;
            int flag = (l_sw < fabsf(l_se - l_ew)) ? 0 : 1;
            for (int i = 0; i < 2; i++) {
                out_solves->sol_flags[4 * ind_arm + 0 + i][1] = flag;
                out_solves->sol_flags[4 * ind_arm + 2 + i][1] = flag;
            }
        } else {
            if (l_sw > (l_se + l_ew) || l_sw < fabsf(l_se - l_ew)) {
                for (int i = 0; i < 4; i++) {
                    out_solves->sol_flags[4 * ind_arm + i][1] = 0; /* 超出可达范围 */
                }
                continue;
            }
            float atan_a = atan2f(l1_sw[1], l1_sw[0]);
            float acos_a = 0.5f * (l_se_2 + l_sw_2 - l_ew_2) / (l_se * l_sw);
            acos_a = fmaxf(-1.0f, fminf(1.0f, acos_a));
            acos_a = acosf(acos_a);

            float acos_e = 0.5f * (l_se_2 + l_ew_2 - l_sw_2) / (l_se * l_ew);
            acos_e = fmaxf(-1.0f, fminf(1.0f, acos_e));
            acos_e = acosf(acos_e);

            if (0 == ind_arm) {
                qa[0][0] = atan_a - acos_a + (float)M_PI_2;
                qa[0][1] = atan_e - acos_e + (float)M_PI;
                qa[1][0] = atan_a + acos_a + (float)M_PI_2;
                qa[1][1] = atan_e + acos_e - (float)M_PI;
            } else {
                qa[0][0] = atan_a + acos_a + (float)M_PI_2;
                qa[0][1] = atan_e + acos_e - (float)M_PI;
                qa[1][0] = atan_a - acos_a + (float)M_PI_2;
                qa[1][1] = atan_e - acos_e + (float)M_PI;
            }
            for (int i = 0; i < 2; i++) {
                out_solves->sol_flags[4 * ind_arm + 0 + i][1] = 1;
                out_solves->sol_flags[4 * ind_arm + 2 + i][1] = 1;
            }
        }

        for (int ind_elbow = 0; ind_elbow < 2; ind_elbow++) {
            float cosqa0 = cosf(qa[ind_elbow][0] + dh[1].theta0);
            float sinqa0 = sinf(qa[ind_elbow][0] + dh[1].theta0);
            float cosqa1 = cosf(qa[ind_elbow][1] + dh[2].theta0);
            float sinqa1 = sinf(qa[ind_elbow][1] + dh[2].theta0);

            float r31[9] = {
                cosqa0 * cosqa1 - sinqa0 * sinqa1,  cosqa0 * sinqa1 + sinqa0 * cosqa1,  0.0f,
                0.0f,                               0.0f,                               1.0f,
                cosqa0 * sinqa1 + sinqa0 * cosqa1, -cosqa0 * cosqa1 + sinqa0 * sinqa1,  0.0f
            };

            float r30[9], r36[9];
            mat_multiply_3x3(r31, r10, r30);
            mat_multiply_3x3(r30, r06, r36);

            float cosqw;
            if (r36[8] >= 1.0f - 1e-6f) {
                cosqw = 1.0f;
                qw[0][1] = 0.0f;
                qw[1][1] = 0.0f;
            } else if (r36[8] <= -1.0f + 1e-6f) {
                cosqw = -1.0f;
                qw[0][1] = (ind_arm == 0) ? (float)M_PI : -(float)M_PI;
                qw[1][1] = (ind_arm == 0) ? -(float)M_PI : (float)M_PI;
            } else {
                cosqw = r36[8];
                if (0 == ind_arm) {
                    qw[0][1] = acosf(cosqw);
                    qw[1][1] = -acosf(cosqw);
                } else {
                    qw[0][1] = -acosf(cosqw);
                    qw[1][1] = acosf(cosqw);
                }
            }

            if (cosqw >= 1.0f - 1e-6f || cosqw <= -1.0f + 1e-6f) {
                /* 手腕奇异点保护 */
                if (0 == ind_arm) {
                    qw[0][0] = last_a[3];
                    float cosq3 = cosf(last_a[3] + dh[3].theta0);
                    float sinq3 = sinf(last_a[3] + dh[3].theta0);
                    qw[0][2] = atan2f(cosq3 * r36[3] - sinq3 * r36[0], cosq3 * r36[0] + sinq3 * r36[3]);
                    qw[1][2] = last_a[5];
                    float cosq5 = cosf(last_a[5] + dh[5].theta0);
                    float sinq5 = sinf(last_a[5] + dh[5].theta0);
                    qw[1][0] = atan2f(cosq5 * r36[3] - sinq5 * r36[0], cosq5 * r36[0] + sinq5 * r36[3]);
                } else {
                    qw[0][2] = last_a[5];
                    float cosq5 = cosf(last_a[5] + dh[5].theta0);
                    float sinq5 = sinf(last_a[5] + dh[5].theta0);
                    qw[0][0] = atan2f(cosq5 * r36[3] - sinq5 * r36[0], cosq5 * r36[0] + sinq5 * r36[3]);
                    qw[1][0] = last_a[3];
                    float cosq3 = cosf(last_a[3] + dh[3].theta0);
                    float sinq3 = sinf(last_a[3] + dh[3].theta0);
                    qw[1][2] = atan2f(cosq3 * r36[3] - sinq3 * r36[0], cosq3 * r36[0] + sinq3 * r36[3]);
                }
                out_solves->sol_flags[4 * ind_arm + 2 * ind_elbow + 0][2] = -1;
                out_solves->sol_flags[4 * ind_arm + 2 * ind_elbow + 1][2] = -1;
            } else {
                if (0 == ind_arm) {
                    qw[0][0] = atan2f(r36[5], r36[2]);
                    qw[1][0] = atan2f(-r36[5], -r36[2]);
                    qw[0][2] = atan2f(r36[7], -r36[6]);
                    qw[1][2] = atan2f(-r36[7], r36[6]);
                } else {
                    qw[0][0] = atan2f(-r36[5], -r36[2]);
                    qw[1][0] = atan2f(r36[5], r36[2]);
                    qw[0][2] = atan2f(-r36[7], r36[6]);
                    qw[1][2] = atan2f(r36[7], -r36[6]);
                }
                out_solves->sol_flags[4 * ind_arm + 2 * ind_elbow + 0][2] = 1;
                out_solves->sol_flags[4 * ind_arm + 2 * ind_elbow + 1][2] = 1;
            }

            for (int ind_wrist = 0; ind_wrist < 2; ind_wrist++) {
                int sol_idx = 4 * ind_arm + 2 * ind_elbow + ind_wrist;
                float *a_out = out_solves->solutions[sol_idx].angles;

                /* J1 / J2,J3 / J4,J5,J6 —— 统一用 zenix_wrap_radian 归一化到 (-PI, PI] */
                a_out[0] = RAD_TO_DEG(zenix_wrap_radian(qs[ind_arm]));
                for (int i = 0; i < 2; i++) {
                    a_out[1 + i] = RAD_TO_DEG(zenix_wrap_radian(qa[ind_elbow][i]));
                }
                for (int i = 0; i < 3; i++) {
                    a_out[3 + i] = RAD_TO_DEG(zenix_wrap_radian(qw[ind_wrist][i]));
                }

                out_solves->solutions[sol_idx].gripper = 0.0f;
            }
        }
    }

    return IK_SUCCESS;
}

/* 几何法逆运动学解算实现 */
ik_status_t kinematics_inverse_geometric(float x, float y, float z,
                                         float pitch_deg, float yaw_deg,
                                         const ik_config_t *cfg,
                                         joint_state_t *out_js) {
    if (!out_js) return IK_ERR_INVALID_PARAM;

    pose6d_t target_pose;
    target_pose.x = x;
    target_pose.y = y;
    target_pose.z = z;
    target_pose.roll = 0.0f;
    target_pose.pitch = pitch_deg;
    target_pose.yaw = yaw_deg;

    return kinematics_inverse_6dof(&target_pose, cfg, out_js);
}

/* 在 8 组解析解中按策略选取最优可行解
 * （6-DOF IK / 轨迹连续 IK 共用，新增选解策略只需加枚举与成本分支） */
ik_status_t kinematics_select_solution(const ik_solutions_t *solves,
                                       const ik_config_t *cfg,
                                       solution_selector_t selector,
                                       const joint_state_t *last_joints,
                                       joint_state_t *out_js) {
    if (!solves || !out_js) return IK_ERR_INVALID_PARAM;

    ik_config_t c = cfg ? *cfg : kinematics_get_default_config();

    /* 全部解均不可达（目标超出工作空间） */
    bool all_invalid = true;
    for (int i = 0; i < 8; i++) {
        if (solves->sol_flags[i][1] != 0) {
            all_invalid = false;
            break;
        }
    }
    if (all_invalid) return IK_ERR_OUT_OF_REACH;

    int best_idx = -1;
    float best_cost = 1e18f;

    for (int i = 0; i < 8; i++) {
        /* 过滤不可达解 */
        if (solves->sol_flags[i][1] == 0) continue;

        /* SOL_SELECT_MIN_SQUARED 遵循 cfg->arm_config 的分支限定；
         * SOL_SELECT_CLOSEST_TO_LAST 以连续性优先，不做家族限定（与历史行为一致） */
        if (selector == SOL_SELECT_MIN_SQUARED &&
            c.arm_config >= 0 && (i / 4) != c.arm_config) {
            continue;
        }

        /* 检查软限位 */
        bool within_limits = true;
        for (int j = 0; j < NUM_JOINTS; j++) {
            if (!is_joint_within_limits(j, solves->solutions[i].angles[j])) {
                within_limits = false;
                break;
            }
        }
        if (!within_limits) continue;

        /* 成本函数：按策略计算 */
        float cost = 0.0f;
        if (selector == SOL_SELECT_CLOSEST_TO_LAST && last_joints) {
            for (int j = 0; j < NUM_JOINTS; j++) {
                float d = zenix_angle_diff_deg(last_joints->angles[j],
                                               solves->solutions[i].angles[j]);
                cost += d * d;
            }
        } else {
            for (int j = 0; j < NUM_JOINTS; j++) {
                float ang = solves->solutions[i].angles[j];
                cost += ang * ang;
            }
        }

        if (cost < best_cost) {
            best_cost = cost;
            best_idx = i;
        }
    }

    if (best_idx < 0) {
        /* 全部候选被软限位过滤：退回第一个肘部有效解并标记限位错误 */
        for (int i = 0; i < 8; i++) {
            if (solves->sol_flags[i][1] != 0) {
                *out_js = solves->solutions[i];
                return IK_ERR_JOINT_LIMIT;
            }
        }
        return IK_ERR_OUT_OF_REACH;
    }

    *out_js = solves->solutions[best_idx];
    return IK_SUCCESS;
}

/* 6-DOF 最优逆运动学求解
 * 默认策略：各轴角度平方和最小，遵循 cfg->arm_config 分支限定 */
ik_status_t kinematics_inverse_6dof(const pose6d_t *target_pose,
                                   const ik_config_t *cfg,
                                   joint_state_t *out_js) {
    if (!target_pose || !out_js) return IK_ERR_INVALID_PARAM;

    ik_config_t c = cfg ? *cfg : kinematics_get_default_config();
    ik_solutions_t solves;
    ik_status_t rc = kinematics_inverse_all(target_pose, NULL, &c, &solves);
    if (rc != IK_SUCCESS) return rc;

    return kinematics_select_solution(&solves, &c, SOL_SELECT_MIN_SQUARED, NULL, out_js);
}
