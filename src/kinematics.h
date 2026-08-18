#ifndef ZENIX_DUMMY_KINEMATICS_H
#define ZENIX_DUMMY_DUMMY_KINEMATICS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 机械臂关节数量（6 轴 + 末端夹爪，夹爪不计入角度自由度） */
#define ARM_JOINTS 6

/* 单关节角度范围（度） */
#define JOINT_MIN_DEG -150.0f
#define JOINT_MAX_DEG  150.0f

/* 关节角度数组类型 */
typedef struct {
    float q[ARM_JOINTS]; /* 各关节角度（度） */
} joint_state_t;

/* 把角度归一化到 [-180, 180) 区间 */
float normalize_angle_deg(float deg);

/* 校验所有关节是否在限位内；返回 1=全部合法，0=有越界 */
int joints_in_range(const joint_state_t *js);

/* 夹爪开合状态（0.0=全闭, 1.0=全开） */
typedef struct {
    float openness; /* 0..1 */
} gripper_state_t;

/* 设置夹爪开合，自动钳位到 [0,1] */
void gripper_set(gripper_state_t *g, float openness);

/* 简单正运动学：把 6 个关节角映射到末端 (x,y,z) 估计（平面近似，仅作基础示例）
   返回 0 成功，-1 越界 */
int forward_kinematics(const joint_state_t *js, float link_len,
                       float *x, float *y, float *z);

#ifdef __cplusplus
}
#endif

#endif /* ZENIX_DUMMY_KINEMATICS_H */
