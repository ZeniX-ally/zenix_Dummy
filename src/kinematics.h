#ifndef KINEMATICS_H
#define KINEMATICS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 机械臂关节数量定义 */
#define NUM_JOINTS 6

/* Dummy 机械臂标准连杆几何参数（单位：mm） */
#define DUMMY_L_BASE        109.0f   /* 基座到底座中心高度/J1轴向高度 L_BS */
#define DUMMY_D_BASE        0.0f     /* J1 径向偏移 D_BS */
#define DUMMY_L_UPPER_ARM   145.0f   /* J2 到 J3 连杆长度 L_ARM */
#define DUMMY_L_FOREARM     144.0f   /* J3 到 J5 手腕中心连杆长度 L_FOREARM */
#define DUMMY_D_ELBOW       0.0f     /* J3 肘部偏移 D_ELBOW */
#define DUMMY_L_WRIST       56.0f    /* J5 到末端夹爪有效长度 L_WRIST */

/* 关节状态结构体（各关节角单位：度 deg，夹爪单位：0.0~1.0） */
typedef struct {
    float angles[NUM_JOINTS]; /* J1, J2, J3, J4, J5, J6 (度) */
    float gripper;            /* 夹爪开合度 0.0(全闭) ~ 1.0(全开) */
} joint_state_t;

/* 机械臂末端 6D 位姿 (X,Y,Z mm, A,B,C 欧拉角 deg) */
typedef struct {
    float x;      /* mm */
    float y;      /* mm */
    float z;      /* mm */
    float roll;   /* deg (A: 绕 X 轴旋转) */
    float pitch;  /* deg (B: 绕 Y 轴旋转) */
    float yaw;    /* deg (C: 绕 Z 轴旋转) */
} pose6d_t;

/* 机械臂几何配置与多解选择 */
typedef struct {
    float l_bs;      /* 基座高度 L_BS (mm) */
    float d_bs;      /* 基座径向偏置 D_BS (mm) */
    float l_arm;     /* 大臂长度 L_ARM (mm) */
    float l_fa;      /* 前臂长度 L_FOREARM (mm) */
    float d_ew;      /* 肘部偏置 D_ELBOW (mm) */
    float l_wt;      /* 末端手部长度 L_WRIST (mm) */
    int   arm_config;/* 分支选择: 0=Elbow Up/Normal, 1=Elbow Down/Alt, -1=Auto/Closest */
} ik_config_t;

/* 逆运动学求解结果状态码 */
typedef enum {
    IK_SUCCESS            = 0,  /* 求解成功 */
    IK_ERR_OUT_OF_REACH   = -1, /* 目标超出工作空间/不可达 */
    IK_ERR_SINGULARITY    = -2, /* 遇到奇异点（手腕中心在Z轴或奇异共线） */
    IK_ERR_JOINT_LIMIT    = -3, /* 解超出关节软限位 */
    IK_ERR_INVALID_PARAM  = -4  /* 输入指针为空或参数异常 */
} ik_status_t;

/* 逆运动学 8 组解析解集合 */
typedef struct {
    joint_state_t solutions[8];
    int sol_flags[8][3]; /* [i][0]: 臂姿态有效标志, [i][1]: 肘部有效标志, [i][2]: 腕部有效标志 */
    int count;
} ik_solutions_t;

/* 关节软限位检查与保护 */
bool is_joint_within_limits(int joint_idx, float angle_deg);
void clamp_joint_angles(joint_state_t *js);
void clamp_gripper(joint_state_t *js);

/* 默认几何参数初始化 */
ik_config_t kinematics_get_default_config(void);

/* 经典简化 3-DOF 几何正运动学（用于向后兼容） */
int forward_kinematics(const joint_state_t *js, float link_len,
                       float *x, float *y, float *z);

/* 完整 6-DOF 正运动学求解 (FK)
 * 输入: joint_state_t 包含 J1~J6 (deg)
 * 输入: cfg 机械臂几何配置（为 NULL 时使用默认配置）
 * 输出: out_pose 计算得到的末端 6D 位姿
 */
int kinematics_forward_6dof(const joint_state_t *js, const ik_config_t *cfg, pose6d_t *out_pose);

/* 完整 6-DOF 解析法逆运动学解算（求出全部 8 组解析解）
 * 输入: target_pose   目标 6D 位姿
 * 输入: last_joints   上一次/当前关节状态（用于奇异点退化参考）
 * 输入: cfg           几何配置
 * 输出: out_solves    全部 8 组多解与有效状态
 */
ik_status_t kinematics_inverse_all(const pose6d_t *target_pose,
                                  const joint_state_t *last_joints,
                                  const ik_config_t *cfg,
                                  ik_solutions_t *out_solves);

/* 几何法逆运动学解算 (IK)
 * 给定目标点 (x, y, z) 与指定手腕俯仰角 pitch (deg)，解算 J1..J6
 * 支持多解分支选择与奇异点容错保护
 *
 * 输入: x, y, z       末端目标笛卡尔坐标 (mm)
 * 输入: pitch_deg     指定俯仰角 (deg)
 * 输入: yaw_deg       末端偏航角基准/方位基准 (deg)
 * 输入: cfg           求解配置
 * 输出: out_js        求解出的关节角结果 (J1..J6 deg)
 * 返回: ik_status_t   状态码
 */
ik_status_t kinematics_inverse_geometric(float x, float y, float z,
                                         float pitch_deg, float yaw_deg,
                                         const ik_config_t *cfg,
                                         joint_state_t *out_js);

/* 完整 6-DOF 位姿逆运动学求解 (IK, 自动选择最优可行解)
 * 给定完整目标位姿 (X, Y, Z, Roll, Pitch, Yaw)
 * 输入: target_pose   目标 6D 位姿
 * 输入: cfg           求解配置
 * 输出: out_js        求解出的关节角结果
 * 返回: ik_status_t   状态码
 */
ik_status_t kinematics_inverse_6dof(const pose6d_t *target_pose,
                                   const ik_config_t *cfg,
                                   joint_state_t *out_js);

#ifdef __cplusplus
}
#endif

#endif /* KINEMATICS_H */
