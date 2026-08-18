#include "kinematics.h"
#include <stdio.h>

static void print_joints(const joint_state_t *js) {
    for (size_t i = 0; i < ARM_JOINTS; ++i) {
        printf("  J%-2zu: %7.2f deg\n", i + 1, (double)js->q[i]);
    }
}

int main(void) {
    printf("zenix_Dummy 基础运动学示例\n");
    printf("关节数: %d  限位: [%.1f, %.1f] deg\n\n",
           ARM_JOINTS, (double)JOINT_MIN_DEG, (double)JOINT_MAX_DEG);

    /* 1) 角度归一化 */
    float raw = 200.0f;
    printf("[1] 归一化 %g -> %g\n", (double)raw, (double)normalize_angle_deg(raw));

    /* 2) 构造一个合理的关节状态并校验 */
    joint_state_t js;
    for (size_t i = 0; i < ARM_JOINTS; ++i) {
        js.q[i] = (float)(10 * (int)i); /* 0,10,20,30,40,50 */
    }
    printf("\n[2] 关节状态:\n");
    print_joints(&js);
    printf("    限位校验: %s\n", joints_in_range(&js) ? "OK" : "越界");

    /* 3) 夹爪控制（带钳位） */
    gripper_state_t g;
    gripper_set(&g, 1.3f); /* 超过 1.0 应钳位 */
    printf("\n[3] 夹爪开合(输入1.3): %.2f (期望 1.00)\n", (double)g.openness);

    /* 4) 正运动学（平面近似） */
    float x, y, z;
    int rc = forward_kinematics(&js, 0.05f, &x, &y, &z);
    printf("\n[4] 正运动学: rc=%d  x=%.3f y=%.3f z=%.3f\n", rc,
           (double)x, (double)y, (double)z);

    /* 5) 越界检测演示 */
    js.q[0] = 999.0f;
    printf("\n[5] 注入越界 J1=999 后, 限位校验: %s (期望 越界)\n",
           joints_in_range(&js) ? "OK" : "越界");

    return 0;
}
