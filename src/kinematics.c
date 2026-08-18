#include "kinematics.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define DEG2RAD (M_PI / 180.0f)

float normalize_angle_deg(float deg) {
    while (deg >= 180.0f) deg -= 360.0f;
    while (deg < -180.0f) deg += 360.0f;
    return deg;
}

int joints_in_range(const joint_state_t *js) {
    if (js == NULL) return 0;
    for (size_t i = 0; i < ARM_JOINTS; ++i) {
        if (js->q[i] < JOINT_MIN_DEG || js->q[i] > JOINT_MAX_DEG) {
            return 0;
        }
    }
    return 1;
}

void gripper_set(gripper_state_t *g, float openness) {
    if (g == NULL) return;
    if (openness < 0.0f) openness = 0.0f;
    if (openness > 1.0f) openness = 1.0f;
    g->openness = openness;
}

int forward_kinematics(const joint_state_t *js, float link_len,
                       float *x, float *y, float *z) {
    if (js == NULL || x == NULL || y == NULL || z == NULL) return -1;
    if (!joints_in_range(js)) return -1;

    float yaw = 0.0f;
    for (size_t i = 0; i < ARM_JOINTS; ++i) {
        yaw += js->q[i] * DEG2RAD;
    }

    float reach = link_len * (float)ARM_JOINTS;
    *x = reach * cosf(yaw);
    *y = reach * sinf(yaw);
    *z = link_len * sinf(js->q[2] * DEG2RAD);
    return 0;
}
