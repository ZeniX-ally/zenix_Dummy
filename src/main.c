#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "kinematics.h"
#include "protocol.h"
#include "trajectory.h"

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("[FAIL] %s:%d: %s\n", __FILE__, __LINE__, msg); \
        return 1; \
    } \
} while(0)

/* 测试 1: 基础正运动学与逆运动学解算闭环测试 */
static int test_kinematics_fk_ik_closure(void) {
    printf("--- [TEST 1] Kinematics FK / IK Geometric Closure Test ---\n");

    ik_config_t cfg = kinematics_get_default_config();

    /* 测试几个典型关节角度配置下的正逆运动学一致性 */
    joint_state_t test_joints[] = {
        { .angles = {10.0f, 20.0f, -30.0f, 15.0f, 25.0f, -10.0f}, .gripper = 0.5f },
        { .angles = {30.0f, 30.0f, -40.0f, 0.0f, 20.0f, 0.0f},    .gripper = 0.0f },
        { .angles = {-45.0f, 45.0f, -60.0f, 10.0f, 15.0f, 20.0f}, .gripper = 1.0f },
        { .angles = {60.0f, 15.0f, -30.0f, 0.0f, 40.0f, 0.0f},    .gripper = 0.2f }
    };

    size_t num_tests = sizeof(test_joints) / sizeof(test_joints[0]);
    for (size_t i = 0; i < num_tests; i++) {
        pose6d_t pose;
        int fk_rc = kinematics_forward_6dof(&test_joints[i], &cfg, &pose);
        TEST_ASSERT(fk_rc == 0, "FK calculation failed");

        printf("  Case %zu: Target Joint=[%.1f, %.1f, %.1f, %.1f, %.1f, %.1f]\n",
               i + 1,
               test_joints[i].angles[0], test_joints[i].angles[1], test_joints[i].angles[2],
               test_joints[i].angles[3], test_joints[i].angles[4], test_joints[i].angles[5]);
        printf("         -> FK Pose: X=%.2f, Y=%.2f, Z=%.2f mm, Roll=%.2f, Pitch=%.2f, Yaw=%.2f deg\n",
               pose.x, pose.y, pose.z, pose.roll, pose.pitch, pose.yaw);

        /* 验证 6-DOF 位姿逆解 */
        joint_state_t ik_js;
        ik_status_t ik_rc = kinematics_inverse_6dof(&pose, &cfg, &ik_js);
        TEST_ASSERT(ik_rc == IK_SUCCESS, "IK 6DOF calculation failed for reachable pose");

        /* 再由 IK 解做 FK 验证空间位置误差在容差范围内 (< 0.1 mm) */
        pose6d_t verify_pose;
        kinematics_forward_6dof(&ik_js, &cfg, &verify_pose);

        float dx = verify_pose.x - pose.x;
        float dy = verify_pose.y - pose.y;
        float dz = verify_pose.z - pose.z;
        float pos_err = sqrtf(dx*dx + dy*dy + dz*dz);

        printf("         -> Solved Joint=[%.1f, %.1f, %.1f, %.1f, %.1f, %.1f], Pos Error = %.4f mm\n",
               ik_js.angles[0], ik_js.angles[1], ik_js.angles[2],
               ik_js.angles[3], ik_js.angles[4], ik_js.angles[5], pos_err);

        TEST_ASSERT(pos_err < 0.1f, "Position closure error exceeds threshold");

        /* 验证几何法逆解接口 kinematics_inverse_geometric */
        joint_state_t geom_js;
        ik_status_t geom_rc = kinematics_inverse_geometric(pose.x, pose.y, pose.z, pose.pitch, pose.yaw, &cfg, &geom_js);
        TEST_ASSERT(geom_rc == IK_SUCCESS || geom_rc == IK_ERR_JOINT_LIMIT, "Geometric IK calculation executed");
    }

    printf("  -> Kinematics FK/IK closure test PASSED.\n\n");
    return 0;
}

/* 测试 2: 逆运动学 8 组多解分支与奇异点保护 */
static int test_kinematics_multi_solution_and_singularity(void) {
    printf("--- [TEST 2] Multi-solution 8-Branch Solver & Singularity Protection ---\n");

    ik_config_t cfg = kinematics_get_default_config();
    pose6d_t pose = {
        .x = 180.0f,
        .y = 80.0f,
        .z = 150.0f,
        .roll = 10.0f,
        .pitch = 20.0f,
        .yaw = 30.0f
    };

    ik_solutions_t solves;
    ik_status_t rc = kinematics_inverse_all(&pose, NULL, &cfg, &solves);
    TEST_ASSERT(rc == IK_SUCCESS, "kinematics_inverse_all failed");

    printf("  Target 6D Pose: (X=%.1f, Y=%.1f, Z=%.1f, R=%.1f, P=%.1f, Y=%.1f)\n",
           pose.x, pose.y, pose.z, pose.roll, pose.pitch, pose.yaw);
    printf("  Found %d solutions:\n", solves.count);

    int valid_count = 0;
    for (int i = 0; i < solves.count; i++) {
        if (solves.sol_flags[i][1] != 0) {
            valid_count++;
            printf("    Sol[%d]: J1=%.1f, J2=%.1f, J3=%.1f, J4=%.1f, J5=%.1f, J6=%.1f (Flags: arm=%d, elbow=%d, wrist=%d)\n",
                   i,
                   solves.solutions[i].angles[0], solves.solutions[i].angles[1], solves.solutions[i].angles[2],
                   solves.solutions[i].angles[3], solves.solutions[i].angles[4], solves.solutions[i].angles[5],
                   solves.sol_flags[i][0], solves.sol_flags[i][1], solves.sol_flags[i][2]);
        }
    }
    TEST_ASSERT(valid_count > 0, "At least one valid solution should exist");

    /* 奇异点保护测试 1: 手腕中心在 Z 轴正上方 (r_xy -> 0) */
    pose6d_t sing_pose = {
        .x = 0.0f,
        .y = 0.0f,
        .z = 250.0f,
        .roll = 0.0f,
        .pitch = 0.0f,
        .yaw = 0.0f
    };
    joint_state_t last_js = { .angles = {45.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f} };
    ik_solutions_t sing_solves;
    ik_status_t rc_sing = kinematics_inverse_all(&sing_pose, &last_js, &cfg, &sing_solves);
    TEST_ASSERT(rc_sing == IK_SUCCESS, "Singularity on Z axis should be safely handled with fallback last angle");
    printf("  Z-axis Singularity (X=0, Y=0): flag=%d, fallback J1=%.1f deg\n",
           sing_solves.sol_flags[0][0], sing_solves.solutions[0].angles[0]);
    TEST_ASSERT(sing_solves.sol_flags[0][0] == -1, "Z axis singularity flag should be set to -1");

    /* 超出工作空间超限保护测试 */
    pose6d_t unreachable_pose = {
        .x = 800.0f,
        .y = 800.0f,
        .z = 800.0f,
        .roll = 0.0f,
        .pitch = 0.0f,
        .yaw = 0.0f
    };
    joint_state_t js_out;
    ik_status_t rc_out = kinematics_inverse_6dof(&unreachable_pose, &cfg, &js_out);
    printf("  Out-of-reach target (800,800,800) -> Status: %d (IK_ERR_OUT_OF_REACH)\n", rc_out);
    TEST_ASSERT(rc_out == IK_ERR_OUT_OF_REACH, "Out of reach target should return IK_ERR_OUT_OF_REACH");

    printf("  -> Multi-solution & Singularity test PASSED.\n\n");
    return 0;
}

/* 测试 3: 通信协议封包、解包与 CRC16 / Checksum 校验测试 */
static int test_protocol_pack_unpack(void) {
    printf("--- [TEST 3] Protocol Frame Pack / Unpack & Checksums ---\n");

    uint8_t buffer[PROTOCOL_FRAME_MAX_LEN + 16];

    /* 1. 关节控制包打包与解析 (CRC16) */
    cmd_set_joint_pos_t tx_joint_cmd = {
        .angles = { 10.5f, -20.3f, 45.0f, -90.0f, 30.0f, 180.0f },
        .speed = 25.0f,
        .gripper = 0.85f
    };

    int packed_len = protocol_pack_set_joint_pos(0x12, &tx_joint_cmd, CHECKSUM_CRC16, buffer, sizeof(buffer));
    TEST_ASSERT(packed_len > 0, "Joint cmd pack failed");
    printf("  Packed Joint Cmd Frame: len=%d bytes, Header=[0x%02X 0x%02X], Seq=0x%02X, Cmd=0x%02X\n",
           packed_len, buffer[0], buffer[1], buffer[2], buffer[3]);

    protocol_frame_t rx_frame;
    int unpack_rc = protocol_unpack_frame(buffer, packed_len, CHECKSUM_CRC16, &rx_frame);
    TEST_ASSERT(unpack_rc == 0, "Unpack frame failed");
    TEST_ASSERT(rx_frame.cmd == CMD_SET_JOINT_POS, "Cmd ID mismatch");
    TEST_ASSERT(rx_frame.seq == 0x12, "Seq mismatch");

    cmd_set_joint_pos_t rx_joint_cmd;
    protocol_unpack_set_joint_pos(&rx_frame, &rx_joint_cmd);
    TEST_ASSERT(fabsf(rx_joint_cmd.angles[0] - 10.5f) < 1e-4f, "Angle J1 mismatch");
    TEST_ASSERT(fabsf(rx_joint_cmd.gripper - 0.85f) < 1e-4f, "Gripper mismatch");

    /* 2. 状态回传包打包与解析 (Sum8 校验) */
    report_status_payload_t tx_status = {
        .robot_state = ROBOT_STATUS_MOVING,
        .error_code = 0,
        .voltage_mv = 24150,
        .current_angles = {0.0f, 15.0f, -30.0f, 0.0f, 45.0f, 0.0f},
        .current_pose = {.x = 150.0f, .y = 0.0f, .z = 200.0f, .roll = 0.0f, .pitch = 15.0f, .yaw = 0.0f},
        .gripper_pos = 0.5f
    };

    packed_len = protocol_pack_status_report(0x34, &tx_status, CHECKSUM_SUM8, buffer, sizeof(buffer));
    TEST_ASSERT(packed_len > 0, "Status report pack failed");

    unpack_rc = protocol_unpack_frame(buffer, packed_len, CHECKSUM_SUM8, &rx_frame);
    TEST_ASSERT(unpack_rc == 0, "Unpack status frame failed");
    report_status_payload_t rx_status;
    protocol_unpack_status_report(&rx_frame, &rx_status);
    TEST_ASSERT(rx_status.voltage_mv == 24150, "Voltage mismatch");
    TEST_ASSERT(rx_status.robot_state == ROBOT_STATUS_MOVING, "Robot state mismatch");

    /* 3. 数据损坏与校验失败测试 */
    buffer[packed_len - 3] ^= 0x55; /* 篡改载荷中的一个字节 */
    int corrupted_rc = protocol_unpack_frame(buffer, packed_len, CHECKSUM_SUM8, &rx_frame);
    TEST_ASSERT(corrupted_rc == -2, "Corrupted frame should fail checksum validation");
    printf("  Corrupted packet check: detected checksum error as expected (rc=%d)\n", corrupted_rc);

    printf("  -> Protocol pack/unpack & checksum test PASSED.\n\n");
    return 0;
}

/* 测试 4: 逐字节流式状态机解析器测试 */
static int test_protocol_streaming_parser(void) {
    printf("--- [TEST 4] Protocol Streaming State-Machine Parser Test ---\n");

    protocol_parser_t parser;
    protocol_parser_init(&parser, CHECKSUM_CRC16);

    cmd_set_cartesian_pos_t tx_cpos = {
        .pose = {.x = 120.0f, .y = -50.0f, .z = 210.0f, .roll = 0.0f, .pitch = -30.0f, .yaw = 15.0f},
        .speed = 100.0f,
        .gripper = 1.0f
    };

    uint8_t raw_bytes[64];
    int frame_len = protocol_pack_set_cartesian_pos(0xAB, &tx_cpos, CHECKSUM_CRC16, raw_bytes, sizeof(raw_bytes));
    TEST_ASSERT(frame_len > 0, "Cartesian cmd pack failed");

    /* 在真实通信中模拟前导噪声、垃圾字符，然后传入完整数据帧 */
    uint8_t noise[] = { 0xFF, 0x00, 0xAA, 0x11, 0x55, 0xAA }; /* 包含不完整头 */
    protocol_frame_t parsed_frame;
    for (size_t i = 0; i < sizeof(noise); i++) {
        bool frame_ready = protocol_parse_byte(&parser, noise[i], &parsed_frame);
        TEST_ASSERT(!frame_ready, "Noise should not trigger frame completion");
    }

    bool detected = false;
    for (int i = 0; i < frame_len; i++) {
        if (protocol_parse_byte(&parser, raw_bytes[i], &parsed_frame)) {
            detected = true;
            TEST_ASSERT(i == frame_len - 1, "Frame should complete exactly at the last byte");
        }
    }

    TEST_ASSERT(detected, "Streaming parser should successfully detect and decode frame");
    TEST_ASSERT(parsed_frame.cmd == CMD_SET_CARTESIAN_POS, "Parsed CMD mismatch");
    TEST_ASSERT(parsed_frame.seq == 0xAB, "Parsed Seq mismatch");

    printf("  Stream Parser successfully synced past noise and decoded Cartesian cmd frame.\n");
    printf("  -> Streaming parser test PASSED.\n\n");
    return 0;
}

/* 测试 6: 梯形速度廓线正确性测试 */
static int test_trajectory_profile(void) {
    printf("--- [TEST 6] Trapezoidal Velocity Profile Correctness ---\n");

    /* 1. 典型梯形廓线（具备匀速段） */
    traj_profile_t p = traj_profile_make(100.0f, 50.0f, 100.0f);
    TEST_ASSERT(p.triangle == false, "Long move should be trapezoid");
    TEST_ASSERT(fabsf(p.v_peak - 50.0f) < 1e-4f, "Peak velocity should reach vmax");
    /* t_acc = v/a = 0.5s; t_cruise = (d - v^2/a)/v = (100-25)/50 = 1.5s; total = 2.5s */
    TEST_ASSERT(fabsf(p.t_acc - 0.5f) < 1e-4f, "Accel time mismatch");
    TEST_ASSERT(fabsf(p.t_cruise - 1.5f) < 1e-4f, "Cruise time mismatch");
    TEST_ASSERT(fabsf(p.t_total - 2.5f) < 1e-4f, "Total time mismatch");

    /* 端点正确性: 起点 0 终点 dist, 初末速度 0 */
    float v = 0.0f;
    float s0 = traj_profile_sample(&p, 0.0f, &v);
    TEST_ASSERT(fabsf(s0) < 1e-4f && fabsf(v) < 1e-4f, "Start position/velocity should be 0");
    float s_end = traj_profile_sample(&p, 10.0f, &v);
    TEST_ASSERT(fabsf(s_end - 100.0f) < 1e-3f, "Endpoint position mismatch");
    TEST_ASSERT(fabsf(v) < 1e-3f, "Endpoint velocity should be 0");

    /* 峰值速度不超限 & 位移单调递增 */
    float prev = 0.0f, vmax_seen = 0.0f;
    for (int i = 1; i <= 50; i++) {
        float t = i * 0.05f;
        float sp = traj_profile_sample(&p, t, &v);
        TEST_ASSERT(sp >= prev - 1e-3f, "Position should be monotonic");
        if (fabsf(v) > vmax_seen) vmax_seen = fabsf(v);
        prev = sp;
    }
    TEST_ASSERT(vmax_seen <= 50.0f + 1e-3f, "Velocity should never exceed vmax");

    /* 2. 短位移三角廓线（达不到 vmax） */
    traj_profile_t t2 = traj_profile_make(5.0f, 50.0f, 100.0f);
    TEST_ASSERT(t2.triangle == true, "Short move should be triangle");
    TEST_ASSERT(t2.v_peak < 50.0f, "Peak below vmax for triangle profile");
    float s2_end = traj_profile_sample(&t2, 10.0f, &v);
    TEST_ASSERT(fabsf(s2_end - 5.0f) < 1e-3f, "Triangle endpoint mismatch");
    TEST_ASSERT(fabsf(v) < 1e-3f, "Triangle endpoint velocity should be 0");

    /* 3. 负方向位移 */
    traj_profile_t pn = traj_profile_make(-80.0f, 40.0f, 80.0f);
    float sn_end = traj_profile_sample(&pn, 10.0f, &v);
    TEST_ASSERT(fabsf(sn_end + 80.0f) < 1e-3f, "Negative endpoint mismatch");
    TEST_ASSERT(fabsf(v) < 1e-3f, "Negative endpoint velocity should be 0");

    printf("  Trapezoid: d=100 v=50 a=100 -> t_acc=%.3f t_cruise=%.3f t_total=%.3f (peak v=%.1f)\n",
           p.t_acc, p.t_cruise, p.t_total, p.v_peak);
    printf("  Triangle:  d=5   v=50 a=100 -> t_total=%.3f (peak v=%.1f)\n",
           t2.t_total, t2.v_peak);
    printf("  -> Trapezoidal profile test PASSED.\n\n");
    return 0;
}

/* 测试 7: 关节空间点对点同步运动 */
static int test_trajectory_joint_p2p(void) {
    printf("--- [TEST 7] Joint-Space P2P Synchronized Motion ---\n");

    /* 包含 170° -> -170° 的跨 180° 最短路径 (应走 +20° 而非 -340°) */
    joint_state_t q0 = { .angles = {10.0f, 20.0f, -30.0f, 0.0f, 15.0f, 170.0f}, .gripper = 0.0f };
    joint_state_t q1 = { .angles = {50.0f, 20.0f, -30.0f, 0.0f, 15.0f, -170.0f}, .gripper = 1.0f };

    traj_joint_plan_t plan;
    int rc = traj_joint_plan_init(&plan, &q0, &q1, 60.0f, 120.0f);
    TEST_ASSERT(rc == 0, "Joint P2P plan init failed");

    /* max_delta = J1: 40 deg; J6 最短路径 = +20 deg -> 关键轴为 J1 */
    TEST_ASSERT(fabsf(plan.max_delta - 40.0f) < 1e-3f, "Critical axis should be J1 (40 deg)");
    TEST_ASSERT(fabsf(plan.delta[5] - 20.0f) < 1e-3f, "J6 should take +20 deg shortcut");

    /* 采样全程: 每个关节同步到达终点, 夹爪同步开合 */
    const int N = 10;
    joint_state_t prev;
    float vel;
    for (int i = 0; i <= N; i++) {
        float t = plan.profile.t_total * i / N;
        joint_state_t js;
        traj_joint_plan_sample(&plan, t, &js, &vel);

        if (i == 0) {
            for (int j = 0; j < NUM_JOINTS; j++) {
                TEST_ASSERT(fabsf(js.angles[j] - q0.angles[j]) < 1e-3f, "Start joints mismatch");
            }
            TEST_ASSERT(fabsf(js.gripper - 0.0f) < 1e-3f, "Start gripper mismatch");
        }
        if (i == N) {
            for (int j = 0; j < NUM_JOINTS; j++) {
                TEST_ASSERT(fabsf(js.angles[j] - q1.angles[j]) < 1e-3f, "End joints mismatch");
            }
            TEST_ASSERT(fabsf(js.gripper - 1.0f) < 1e-3f, "End gripper mismatch");
            TEST_ASSERT(fabsf(vel) < 1e-3f, "End velocity should be 0");
        } else if (i > 0) {
            /* 检查连续性: 相邻采样关节差应远小于直接跳跃（用环绕差度量） */
            float step = 0.0f;
            for (int j = 0; j < NUM_JOINTS; j++) {
                float d = fabsf(traj_angle_diff(prev.angles[j], js.angles[j]));
                if (d > step) step = d;
            }
            TEST_ASSERT(step < 12.0f, "Joint step should be bounded");
        }
        prev = js;
    }

    /* 同步性验证: 某时刻所有已移动分数相等（用环绕差归一化相位） */
    joint_state_t mid;
    traj_joint_plan_sample(&plan, plan.profile.t_total * 0.5f, &mid, NULL);
    float f_j1 = traj_angle_diff(q0.angles[0], mid.angles[0]) / 40.0f;
    float f_j6 = traj_angle_diff(q0.angles[5], mid.angles[5]) / 20.0f;
    TEST_ASSERT(fabsf(f_j1 - f_j6) < 0.01f, "All joints should move in sync");

    printf("  P2P: J1 10->50 deg (40), J6 170->-170 deg (shortest +20), t_total=%.3fs\n",
           plan.profile.t_total);
    printf("  Mid-sampling: J1 frac=%.3f, J6 frac=%.3f (synced)\n", f_j1, f_j6);
    printf("  -> Joint P2P synchronized motion test PASSED.\n\n");
    return 0;
}

/* 测试 8: 笛卡尔直线运动 + 连续 IK 逆解 */
static int test_trajectory_line_ik(void) {
    printf("--- [TEST 8] Cartesian Line Motion + Continuous IK ---\n");

    ik_config_t cfg = kinematics_get_default_config();

    /* 从一非奇异姿态出发（home 全零会导致手腕中心落在 Z 轴奇异区） */
    joint_state_t home = { .angles = {15.0f, 35.0f, -45.0f, 10.0f, 30.0f, 20.0f}, .gripper = 0.0f };
    pose6d_t p_start, p_end;
    kinematics_forward_6dof(&home, &cfg, &p_start);

    p_end = p_start;
    p_end.x += 60.0f;
    p_end.y += 30.0f;
    p_end.z -= 40.0f;   /* 向下压 */
    p_end.pitch += 15.0f;

    traj_line_plan_t plan;
    int rc = traj_line_plan_init(&plan, &p_start, &p_end, 120.0f, 240.0f);
    TEST_ASSERT(rc == 0, "Line plan init failed");
    float path_len = plan.path_len;
    TEST_ASSERT(fabsf(path_len - sqrtf(60*60 + 30*30 + 40*40)) < 1e-2f,
                "Path length mismatch");

    /* 遍历采样点: 每个点逆解 -> 正解, 验证沿直线且分支连续 */
    joint_state_t last = home;
    const int N = 20;
    float max_pos_err = 0.0f, max_jump = 0.0f;

    for (int i = 0; i <= N; i++) {
        float t = plan.profile.t_total * i / N;
        pose6d_t target;
        traj_line_plan_sample(&plan, t, &target, NULL);

        joint_state_t js;
        ik_status_t ik_rc = traj_ik_closest(&target, &last, &cfg, &js);
        TEST_ASSERT(ik_rc == IK_SUCCESS, "IK should succeed along reachable line");

        /* 正解验证: 末端位置应贴住直线 */
        pose6d_t fk;
        kinematics_forward_6dof(&js, &cfg, &fk);
        float dx = fk.x - target.x, dy = fk.y - target.y, dz = fk.z - target.z;
        float err = sqrtf(dx*dx + dy*dy + dz*dz);
        if (err > max_pos_err) max_pos_err = err;

        if (i > 0) {
            float jump = 0.0f;
            for (int j = 0; j < NUM_JOINTS; j++) {
                float d = fabsf(traj_angle_diff(last.angles[j], js.angles[j]));
                if (d > jump) jump = d;
            }
            if (jump > max_jump) max_jump = jump;
        }
        last = js;
    }

    printf("  Line: (%.1f,%.1f,%.1f) -> (%.1f,%.1f,%.1f), len=%.1f mm, t_total=%.3fs\n",
           p_start.x, p_start.y, p_start.z, p_end.x, p_end.y, p_end.z, path_len,
           plan.profile.t_total);
    printf("  Max FK position error along line: %.4f mm\n", max_pos_err);
    printf("  Max joint jump between samples: %.3f deg\n", max_jump);

    TEST_ASSERT(max_pos_err < 0.5f, "FK should track straight line within 0.5 mm");
    TEST_ASSERT(max_jump < 10.0f, "IK branch should stay continuous along line");
    TEST_ASSERT(plan.profile.v_peak <= 120.0f + 1e-3f, "Line speed should honor vmax");

    printf("  -> Cartesian line + continuous IK test PASSED.\n\n");
    return 0;
}

/* 测试 5: ASCII 命令行与上位机协议互通测试 */
static int test_protocol_ascii(void) {
    printf("--- [TEST 5] Protocol ASCII String Encoding & Parsing Test ---\n");

    char buf[128];
    joint_state_t js = { .angles = { 10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f }, .gripper = 0.0f };
    protocol_ascii_format_move_joint(buf, sizeof(buf), &js, 15.0f);
    printf("  Formatted ASCII Joint Cmd: %s", buf);

    protocol_cmd_t cmd;
    cmd_set_joint_pos_t parsed_jpos;
    int rc = protocol_ascii_parse_command(buf, &cmd, &parsed_jpos);
    TEST_ASSERT(rc == 0 && cmd == CMD_SET_JOINT_POS, "ASCII parse joint cmd failed");
    TEST_ASSERT(fabsf(parsed_jpos.angles[2] - 30.0f) < 1e-3f, "ASCII parsed J3 angle mismatch");
    TEST_ASSERT(fabsf(parsed_jpos.speed - 15.0f) < 1e-3f, "ASCII parsed speed mismatch");

    /* 测试紧急停止命令 */
    rc = protocol_ascii_parse_command("!", &cmd, NULL);
    TEST_ASSERT(rc == 0 && cmd == CMD_EMERGENCY_STOP, "ASCII emergency stop parse failed");

    printf("  -> ASCII protocol parsing test PASSED.\n\n");
    return 0;
}

int main(void) {
    printf("====================================================\n");
    printf("   Dummy Robot Kinematics & Protocol Unit Tests    \n");
    printf("====================================================\n\n");

    if (test_kinematics_fk_ik_closure() != 0) return 1;
    if (test_kinematics_multi_solution_and_singularity() != 0) return 1;
    if (test_protocol_pack_unpack() != 0) return 1;
    if (test_protocol_streaming_parser() != 0) return 1;
    if (test_protocol_ascii() != 0) return 1;
    if (test_trajectory_profile() != 0) return 1;
    if (test_trajectory_joint_p2p() != 0) return 1;
    if (test_trajectory_line_ik() != 0) return 1;

    printf("====================================================\n");
    printf("   ALL 8 TEST SUITES PASSED SUCCESSFULLY! (100%%)   \n");
    printf("====================================================\n");
    return 0;
}
