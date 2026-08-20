#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "kinematics.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 协议帧同步头定义 */
#define PROTOCOL_FRAME_HEADER_H  0xAA
#define PROTOCOL_FRAME_HEADER_L  0x55
#define PROTOCOL_FRAME_MAX_LEN   128

/* 协议功能字 / 命令 ID */
typedef enum {
    CMD_PING             = 0x01,  /* 心跳/握手 */
    CMD_EMERGENCY_STOP   = 0x02,  /* 急停 */
    CMD_SET_ENABLE       = 0x03,  /* 使能/失能电机 */
    CMD_SET_JOINT_POS    = 0x10,  /* 关节位置目标控制 (J1..J6, 速度, 夹爪) */
    CMD_SET_CARTESIAN_POS= 0x11,  /* 笛卡尔空间位姿控制 (X,Y,Z,A,B,C, 速度, 夹爪) */
    CMD_GET_STATUS       = 0x20,  /* 查询机器人当前状态 */
    CMD_REPORT_STATUS    = 0x21,  /* 机器人状态主动上报/回传响应 */
    CMD_ACK              = 0x80,  /* 通用应答 */
    CMD_NACK             = 0x81   /* 错误/拒绝应答 */
} protocol_cmd_t;

/* 校验模式选择 */
typedef enum {
    CHECKSUM_SUM8  = 0,  /* 单字节累加和 */
    CHECKSUM_CRC16 = 1   /* CCITT/MODBUS 16位循环冗余校验 (Poly 0x1021 / 0x8005 / Fibre canonical) */
} checksum_type_t;

/* 机械臂运行状态标志位 */
typedef enum {
    ROBOT_STATUS_DISABLED = 0x00,
    ROBOT_STATUS_IDLE     = 0x01,
    ROBOT_STATUS_MOVING   = 0x02,
    ROBOT_STATUS_ERROR    = 0x04,
    ROBOT_STATUS_ESTOP    = 0x08
} robot_status_flags_t;

/* 位置控制指令载荷 (CMD_SET_JOINT_POS) */
typedef struct {
    float angles[NUM_JOINTS]; /* J1..J6 目标关节角 (deg) */
    float speed;              /* 运动速度 (deg/s 或 0.0~1.0 比例) */
    float gripper;            /* 夹爪位置 (0.0~1.0) */
} cmd_set_joint_pos_t;

/* 笛卡尔位姿控制指令载荷 (CMD_SET_CARTESIAN_POS) */
typedef struct {
    pose6d_t pose;            /* X, Y, Z (mm), Roll, Pitch, Yaw (deg) */
    float speed;              /* 运动速度 (mm/s 或比例) */
    float gripper;            /* 夹爪位置 (0.0~1.0) */
} cmd_set_cartesian_pos_t;

/* 机器人状态回传载荷 (CMD_REPORT_STATUS) */
typedef struct {
    uint8_t  robot_state;     /* robot_status_flags_t 状态码 */
    uint8_t  error_code;      /* 错误码 (0 为正常) */
    uint16_t voltage_mv;      /* 母线电压 (mV) */
    float    current_angles[NUM_JOINTS]; /* 当前各关节角 (deg) */
    pose6d_t current_pose;    /* 当前末端位姿 */
    float    gripper_pos;     /* 当前夹爪位置 */
} report_status_payload_t;

/* 通信帧结构体解析模型 */
typedef struct {
    uint8_t  seq;             /* 帧序列号 0~255 */
    uint8_t  cmd;             /* 命令字 (protocol_cmd_t) */
    uint8_t  payload_len;     /* 载荷数据字节长度 */
    uint8_t  payload[PROTOCOL_FRAME_MAX_LEN]; /* 载荷数据 */
    uint16_t checksum;        /* 校验和 (CRC16 或 低位为SUM8) */
} protocol_frame_t;

/* 状态解析与流式接收机 */
typedef enum {
    PARSE_STATE_HEADER_H = 0,
    PARSE_STATE_HEADER_L,
    PARSE_STATE_SEQ,
    PARSE_STATE_CMD,
    PARSE_STATE_LEN,
    PARSE_STATE_PAYLOAD,
    PARSE_STATE_CHECKSUM_H,
    PARSE_STATE_CHECKSUM_L,
    PARSE_STATE_COMPLETE
} parse_state_t;

typedef struct {
    parse_state_t state;
    checksum_type_t chk_type;
    protocol_frame_t frame;
    uint8_t payload_idx;
    uint8_t temp_buf[PROTOCOL_FRAME_MAX_LEN + 16];
    size_t  temp_len;
} protocol_parser_t;

/* CRC16 与 Checksum 校验计算函数 */
uint8_t  protocol_calc_sum8(const uint8_t *data, size_t len);
uint16_t protocol_calc_crc16(const uint8_t *data, size_t len);
uint16_t protocol_calc_crc16_fibre(const uint8_t *data, size_t len);

/* 帧封包 / 编码 */
int protocol_pack_frame(const protocol_frame_t *frame, checksum_type_t chk_type,
                        uint8_t *out_buf, size_t out_buf_size);

/* 快捷封包：关节目标控制包 */
int protocol_pack_set_joint_pos(uint8_t seq, const cmd_set_joint_pos_t *cmd,
                                checksum_type_t chk_type,
                                uint8_t *out_buf, size_t out_buf_size);

/* 快捷封包：笛卡尔位姿控制包 */
int protocol_pack_set_cartesian_pos(uint8_t seq, const cmd_set_cartesian_pos_t *cmd,
                                    checksum_type_t chk_type,
                                    uint8_t *out_buf, size_t out_buf_size);

/* 快捷封包：状态回传包 */
int protocol_pack_status_report(uint8_t seq, const report_status_payload_t *status,
                                checksum_type_t chk_type,
                                uint8_t *out_buf, size_t out_buf_size);

/* 流式解析器初始化与逐字节解析 */
void protocol_parser_init(protocol_parser_t *parser, checksum_type_t chk_type);
bool protocol_parse_byte(protocol_parser_t *parser, uint8_t byte, protocol_frame_t *out_frame);

/* 整包直接解包与反序列化 */
int protocol_unpack_frame(const uint8_t *buf, size_t len, checksum_type_t chk_type,
                          protocol_frame_t *out_frame);
int protocol_unpack_set_joint_pos(const protocol_frame_t *frame, cmd_set_joint_pos_t *out_cmd);
int protocol_unpack_status_report(const protocol_frame_t *frame, report_status_payload_t *out_status);

/* ASCII 格式控制命令编码与解析 (兼容上位机命令行终端) */
int protocol_ascii_format_move_joint(char *out_str, size_t max_len, const joint_state_t *js, float speed);
int protocol_ascii_format_move_line(char *out_str, size_t max_len, const pose6d_t *pose, float speed);
int protocol_ascii_parse_command(const char *cmd_str, protocol_cmd_t *out_cmd, void *out_data);

#ifdef __cplusplus
}
#endif

#endif /* PROTOCOL_H */
