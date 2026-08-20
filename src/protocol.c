#include "protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ----------------- 校验算法实现 ----------------- */

uint8_t protocol_calc_sum8(const uint8_t *data, size_t len) {
    uint8_t sum = 0;
    if (!data) return 0;
    for (size_t i = 0; i < len; i++) {
        sum = (uint8_t)(sum + data[i]);
    }
    return sum;
}

/* 标准 CRC16-CCITT (Poly 0x1021, Init 0xFFFF) */
uint16_t protocol_calc_crc16(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFF;
    if (!data) return 0;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)((uint16_t)data[i] << 8);
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (uint16_t)((crc << 1) ^ 0x1021);
            } else {
                crc = (uint16_t)(crc << 1);
            }
        }
    }
    return crc;
}

/* Dummy Robot Fibre 规范 CRC16 (Poly 0x3d65, Init 0x1337) */
uint16_t protocol_calc_crc16_fibre(const uint8_t *data, size_t len) {
    uint16_t crc = 0x1337;
    if (!data) return 0;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)((uint16_t)data[i] << 8);
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (uint16_t)((crc << 1) ^ 0x3d65);
            } else {
                crc = (uint16_t)(crc << 1);
            }
        }
    }
    return crc;
}

/* ----------------- 帧编码 / 封包 ----------------- */

int protocol_pack_frame(const protocol_frame_t *frame, checksum_type_t chk_type,
                        uint8_t *out_buf, size_t out_buf_size) {
    if (!frame || !out_buf) return -1;

    size_t chk_len = (chk_type == CHECKSUM_CRC16) ? 2 : 1;
    size_t total_len = 2 + 1 + 1 + 1 + frame->payload_len + chk_len;

    if (out_buf_size < total_len || frame->payload_len > PROTOCOL_FRAME_MAX_LEN) {
        return -1;
    }

    size_t idx = 0;
    out_buf[idx++] = PROTOCOL_FRAME_HEADER_H;
    out_buf[idx++] = PROTOCOL_FRAME_HEADER_L;
    out_buf[idx++] = frame->seq;
    out_buf[idx++] = frame->cmd;
    out_buf[idx++] = frame->payload_len;

    if (frame->payload_len > 0) {
        memcpy(&out_buf[idx], frame->payload, frame->payload_len);
        idx += frame->payload_len;
    }

    /* 校验范围：从 seq 到 payload 结束 (长度 = 3 + payload_len) */
    const uint8_t *check_start = &out_buf[2];
    size_t check_len = 3 + frame->payload_len;

    if (chk_type == CHECKSUM_CRC16) {
        uint16_t crc = protocol_calc_crc16(check_start, check_len);
        out_buf[idx++] = (uint8_t)((crc >> 8) & 0xFF);
        out_buf[idx++] = (uint8_t)(crc & 0xFF);
    } else {
        uint8_t sum = protocol_calc_sum8(check_start, check_len);
        out_buf[idx++] = sum;
    }

    return (int)idx;
}

/* ----------------- 结构体序列化 / 反序列化 ----------------- */

static void serialize_float(uint8_t *dest, float val) {
    memcpy(dest, &val, sizeof(float));
}

static float deserialize_float(const uint8_t *src) {
    float val;
    memcpy(&val, src, sizeof(float));
    return val;
}

static void serialize_uint16(uint8_t *dest, uint16_t val) {
    dest[0] = (uint8_t)((val >> 8) & 0xFF);
    dest[1] = (uint8_t)(val & 0xFF);
}

static uint16_t deserialize_uint16(const uint8_t *src) {
    return (uint16_t)(((uint16_t)src[0] << 8) | (uint16_t)src[1]);
}

int protocol_pack_set_joint_pos(uint8_t seq, const cmd_set_joint_pos_t *cmd,
                                checksum_type_t chk_type,
                                uint8_t *out_buf, size_t out_buf_size) {
    if (!cmd || !out_buf) return -1;
    protocol_frame_t frame;
    frame.seq = seq;
    frame.cmd = CMD_SET_JOINT_POS;
    frame.payload_len = (NUM_JOINTS + 2) * sizeof(float); /* 6 angles + speed + gripper = 32 bytes */

    uint8_t *p = frame.payload;
    for (int i = 0; i < NUM_JOINTS; i++) {
        serialize_float(p, cmd->angles[i]);
        p += sizeof(float);
    }
    serialize_float(p, cmd->speed);
    p += sizeof(float);
    serialize_float(p, cmd->gripper);

    return protocol_pack_frame(&frame, chk_type, out_buf, out_buf_size);
}

int protocol_pack_set_cartesian_pos(uint8_t seq, const cmd_set_cartesian_pos_t *cmd,
                                    checksum_type_t chk_type,
                                    uint8_t *out_buf, size_t out_buf_size) {
    if (!cmd || !out_buf) return -1;
    protocol_frame_t frame;
    frame.seq = seq;
    frame.cmd = CMD_SET_CARTESIAN_POS;
    frame.payload_len = (6 + 2) * sizeof(float); /* x,y,z,roll,pitch,yaw + speed + gripper = 32 bytes */

    uint8_t *p = frame.payload;
    serialize_float(p, cmd->pose.x);     p += sizeof(float);
    serialize_float(p, cmd->pose.y);     p += sizeof(float);
    serialize_float(p, cmd->pose.z);     p += sizeof(float);
    serialize_float(p, cmd->pose.roll);  p += sizeof(float);
    serialize_float(p, cmd->pose.pitch); p += sizeof(float);
    serialize_float(p, cmd->pose.yaw);   p += sizeof(float);
    serialize_float(p, cmd->speed);      p += sizeof(float);
    serialize_float(p, cmd->gripper);

    return protocol_pack_frame(&frame, chk_type, out_buf, out_buf_size);
}

int protocol_pack_status_report(uint8_t seq, const report_status_payload_t *status,
                                checksum_type_t chk_type,
                                uint8_t *out_buf, size_t out_buf_size) {
    if (!status || !out_buf) return -1;
    protocol_frame_t frame;
    frame.seq = seq;
    frame.cmd = CMD_REPORT_STATUS;
    
    /* 1(state) + 1(err) + 2(voltage) + 6*4(angles) + 6*4(pose) + 4(gripper) = 4 + 24 + 24 + 4 = 56 bytes */
    frame.payload_len = 1 + 1 + 2 + (NUM_JOINTS + 6 + 1) * sizeof(float);

    uint8_t *p = frame.payload;
    *p++ = status->robot_state;
    *p++ = status->error_code;
    serialize_uint16(p, status->voltage_mv);
    p += 2;

    for (int i = 0; i < NUM_JOINTS; i++) {
        serialize_float(p, status->current_angles[i]);
        p += sizeof(float);
    }

    serialize_float(p, status->current_pose.x);     p += sizeof(float);
    serialize_float(p, status->current_pose.y);     p += sizeof(float);
    serialize_float(p, status->current_pose.z);     p += sizeof(float);
    serialize_float(p, status->current_pose.roll);  p += sizeof(float);
    serialize_float(p, status->current_pose.pitch); p += sizeof(float);
    serialize_float(p, status->current_pose.yaw);   p += sizeof(float);

    serialize_float(p, status->gripper_pos);

    return protocol_pack_frame(&frame, chk_type, out_buf, out_buf_size);
}

int protocol_unpack_set_joint_pos(const protocol_frame_t *frame, cmd_set_joint_pos_t *out_cmd) {
    if (!frame || !out_cmd) return -1;
    if (frame->cmd != CMD_SET_JOINT_POS || frame->payload_len < (NUM_JOINTS + 2) * sizeof(float)) {
        return -1;
    }
    const uint8_t *p = frame->payload;
    for (int i = 0; i < NUM_JOINTS; i++) {
        out_cmd->angles[i] = deserialize_float(p);
        p += sizeof(float);
    }
    out_cmd->speed = deserialize_float(p);
    p += sizeof(float);
    out_cmd->gripper = deserialize_float(p);
    return 0;
}

int protocol_unpack_status_report(const protocol_frame_t *frame, report_status_payload_t *out_status) {
    if (!frame || !out_status) return -1;
    size_t expected_len = 1 + 1 + 2 + (NUM_JOINTS + 6 + 1) * sizeof(float);
    if (frame->cmd != CMD_REPORT_STATUS || frame->payload_len < expected_len) {
        return -1;
    }
    const uint8_t *p = frame->payload;
    out_status->robot_state = *p++;
    out_status->error_code  = *p++;
    out_status->voltage_mv  = deserialize_uint16(p);
    p += 2;

    for (int i = 0; i < NUM_JOINTS; i++) {
        out_status->current_angles[i] = deserialize_float(p);
        p += sizeof(float);
    }
    out_status->current_pose.x     = deserialize_float(p); p += sizeof(float);
    out_status->current_pose.y     = deserialize_float(p); p += sizeof(float);
    out_status->current_pose.z     = deserialize_float(p); p += sizeof(float);
    out_status->current_pose.roll  = deserialize_float(p); p += sizeof(float);
    out_status->current_pose.pitch = deserialize_float(p); p += sizeof(float);
    out_status->current_pose.yaw   = deserialize_float(p); p += sizeof(float);

    out_status->gripper_pos = deserialize_float(p);
    return 0;
}

/* ----------------- 解包与流式状态机解析 ----------------- */

int protocol_unpack_frame(const uint8_t *buf, size_t len, checksum_type_t chk_type,
                          protocol_frame_t *out_frame) {
    if (!buf || !out_frame) return -1;
    size_t chk_len = (chk_type == CHECKSUM_CRC16) ? 2 : 1;
    if (len < 5 + chk_len) return -1;

    if (buf[0] != PROTOCOL_FRAME_HEADER_H || buf[1] != PROTOCOL_FRAME_HEADER_L) {
        return -1;
    }

    uint8_t seq = buf[2];
    uint8_t cmd = buf[3];
    uint8_t payload_len = buf[4];

    if (payload_len > PROTOCOL_FRAME_MAX_LEN || len < (size_t)(5 + payload_len + chk_len)) {
        return -1;
    }

    const uint8_t *check_start = &buf[2];
    size_t check_len = 3 + payload_len;

    if (chk_type == CHECKSUM_CRC16) {
        uint16_t expected_crc = protocol_calc_crc16(check_start, check_len);
        uint16_t received_crc = (uint16_t)(((uint16_t)buf[5 + payload_len] << 8) | (uint16_t)buf[5 + payload_len + 1]);
        if (expected_crc != received_crc) {
            return -2; /* 校验失败 */
        }
        out_frame->checksum = received_crc;
    } else {
        uint8_t expected_sum = protocol_calc_sum8(check_start, check_len);
        uint8_t received_sum = buf[5 + payload_len];
        if (expected_sum != received_sum) {
            return -2; /* 校验失败 */
        }
        out_frame->checksum = received_sum;
    }

    out_frame->seq = seq;
    out_frame->cmd = cmd;
    out_frame->payload_len = payload_len;
    if (payload_len > 0) {
        memcpy(out_frame->payload, &buf[5], payload_len);
    }
    return 0;
}

void protocol_parser_init(protocol_parser_t *parser, checksum_type_t chk_type) {
    if (!parser) return;
    memset(parser, 0, sizeof(protocol_parser_t));
    parser->state = PARSE_STATE_HEADER_H;
    parser->chk_type = chk_type;
}

bool protocol_parse_byte(protocol_parser_t *parser, uint8_t byte, protocol_frame_t *out_frame) {
    if (!parser) return false;

    switch (parser->state) {
        case PARSE_STATE_HEADER_H:
            if (byte == PROTOCOL_FRAME_HEADER_H) {
                parser->state = PARSE_STATE_HEADER_L;
                parser->temp_len = 0;
            }
            break;

        case PARSE_STATE_HEADER_L:
            if (byte == PROTOCOL_FRAME_HEADER_L) {
                parser->state = PARSE_STATE_SEQ;
            } else if (byte != PROTOCOL_FRAME_HEADER_H) {
                parser->state = PARSE_STATE_HEADER_H;
            }
            break;

        case PARSE_STATE_SEQ:
            parser->frame.seq = byte;
            parser->temp_buf[parser->temp_len++] = byte;
            parser->state = PARSE_STATE_CMD;
            break;

        case PARSE_STATE_CMD:
            parser->frame.cmd = byte;
            parser->temp_buf[parser->temp_len++] = byte;
            parser->state = PARSE_STATE_LEN;
            break;

        case PARSE_STATE_LEN:
            parser->frame.payload_len = byte;
            parser->temp_buf[parser->temp_len++] = byte;
            parser->payload_idx = 0;
            if (parser->frame.payload_len > PROTOCOL_FRAME_MAX_LEN) {
                /* 超长载荷非法重置 */
                parser->state = PARSE_STATE_HEADER_H;
            } else if (parser->frame.payload_len == 0) {
                parser->state = (parser->chk_type == CHECKSUM_CRC16) ? PARSE_STATE_CHECKSUM_H : PARSE_STATE_CHECKSUM_L;
            } else {
                parser->state = PARSE_STATE_PAYLOAD;
            }
            break;

        case PARSE_STATE_PAYLOAD:
            parser->frame.payload[parser->payload_idx++] = byte;
            parser->temp_buf[parser->temp_len++] = byte;
            if (parser->payload_idx >= parser->frame.payload_len) {
                parser->state = (parser->chk_type == CHECKSUM_CRC16) ? PARSE_STATE_CHECKSUM_H : PARSE_STATE_CHECKSUM_L;
            }
            break;

        case PARSE_STATE_CHECKSUM_H:
            parser->frame.checksum = (uint16_t)((uint16_t)byte << 8);
            parser->state = PARSE_STATE_CHECKSUM_L;
            break;

        case PARSE_STATE_CHECKSUM_L: {
            if (parser->chk_type == CHECKSUM_CRC16) {
                parser->frame.checksum |= byte;
                uint16_t calc = protocol_calc_crc16(parser->temp_buf, parser->temp_len);
                parser->state = PARSE_STATE_HEADER_H;
                if (calc == parser->frame.checksum) {
                    if (out_frame) *out_frame = parser->frame;
                    return true;
                }
            } else {
                parser->frame.checksum = byte;
                uint8_t calc = protocol_calc_sum8(parser->temp_buf, parser->temp_len);
                parser->state = PARSE_STATE_HEADER_H;
                if (calc == (uint8_t)parser->frame.checksum) {
                    if (out_frame) *out_frame = parser->frame;
                    return true;
                }
            }
            break;
        }

        default:
            parser->state = PARSE_STATE_HEADER_H;
            break;
    }
    return false;
}

/* ----------------- ASCII 协议辅助 ----------------- */

int protocol_ascii_format_move_joint(char *out_str, size_t max_len, const joint_state_t *js, float speed) {
    if (!out_str || !js || max_len < 32) return -1;
    return snprintf(out_str, max_len, ">%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\n",
                    js->angles[0], js->angles[1], js->angles[2],
                    js->angles[3], js->angles[4], js->angles[5], speed);
}

int protocol_ascii_format_move_line(char *out_str, size_t max_len, const pose6d_t *pose, float speed) {
    if (!out_str || !pose || max_len < 32) return -1;
    return snprintf(out_str, max_len, "@%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\n",
                    pose->x, pose->y, pose->z,
                    pose->roll, pose->pitch, pose->yaw, speed);
}

int protocol_ascii_parse_command(const char *cmd_str, protocol_cmd_t *out_cmd, void *out_data) {
    if (!cmd_str || !out_cmd) return -1;

    if (cmd_str[0] == '!') {
        *out_cmd = CMD_EMERGENCY_STOP;
        return 0;
    } else if (strcmp(cmd_str, "enable\n") == 0 || strcmp(cmd_str, "enable") == 0) {
        *out_cmd = CMD_SET_ENABLE;
        if (out_data) *(bool*)out_data = true;
        return 0;
    } else if (strcmp(cmd_str, "disable\n") == 0 || strcmp(cmd_str, "disable") == 0) {
        *out_cmd = CMD_SET_ENABLE;
        if (out_data) *(bool*)out_data = false;
        return 0;
    } else if (cmd_str[0] == '>') {
        *out_cmd = CMD_SET_JOINT_POS;
        if (out_data) {
            cmd_set_joint_pos_t *jcmd = (cmd_set_joint_pos_t*)out_data;
            float s = 0.0f;
            int n = sscanf(cmd_str, ">%f,%f,%f,%f,%f,%f,%f",
                           &jcmd->angles[0], &jcmd->angles[1], &jcmd->angles[2],
                           &jcmd->angles[3], &jcmd->angles[4], &jcmd->angles[5], &s);
            if (n >= 6) {
                jcmd->speed = (n == 7) ? s : 10.0f;
                jcmd->gripper = 0.0f;
                return 0;
            }
        }
    } else if (cmd_str[0] == '@') {
        *out_cmd = CMD_SET_CARTESIAN_POS;
        if (out_data) {
            cmd_set_cartesian_pos_t *ccmd = (cmd_set_cartesian_pos_t*)out_data;
            float s = 0.0f;
            int n = sscanf(cmd_str, "@%f,%f,%f,%f,%f,%f,%f",
                           &ccmd->pose.x, &ccmd->pose.y, &ccmd->pose.z,
                           &ccmd->pose.roll, &ccmd->pose.pitch, &ccmd->pose.yaw, &s);
            if (n >= 6) {
                ccmd->speed = (n == 7) ? s : 10.0f;
                ccmd->gripper = 0.0f;
                return 0;
            }
        }
    }
    return -1;
}
