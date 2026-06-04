#include "protocol/mon_protocol.hpp"

#include <string.h>
#include <log/log>

// ============================================================
// 监控平台消息格式（示例，按甲方协议文档调整）
//
//  | 起始符(2B) | 长度(2B) | 命令码(1B) | 序列号(2B) | 数据(nB) | CRC(2B) |
//
// 命令码（上行）：
//   0x01 登录
//   0x03 心跳
//   0x05 设备状态上报
//   0x07 电气参数上报
//   0x09 充电记录上报
//
// 命令码（下行）：
//   0x02 登录响应
//   0x04 心跳响应
//   0x90 OTA 升级请求
//   0x91 参数设置
// ============================================================

namespace {
    static constexpr uint8_t FRAME_HEADER_0 = 0x7E;
    static constexpr uint8_t FRAME_HEADER_1 = 0x7E;

    static constexpr uint8_t CMD_LOGIN         = 0x01;
    static constexpr uint8_t CMD_HEARTBEAT     = 0x03;
    static constexpr uint8_t CMD_STATUS        = 0x05;
    static constexpr uint8_t CMD_ELECTRICAL    = 0x07;
    static constexpr uint8_t CMD_SESSION       = 0x09;

    static constexpr uint8_t CMD_LOGIN_ACK     = 0x02;
    static constexpr uint8_t CMD_HEARTBEAT_ACK = 0x04;
    static constexpr uint8_t CMD_OTA_REQUEST   = 0x90;
    static constexpr uint8_t CMD_SET_PARAM     = 0x91;

    static uint16_t seq_ = 0;

    static uint16_t CalcCrc16(const uint8_t *buf, size_t len) {
        uint16_t crc = 0xFFFF;
        for (size_t i = 0; i < len; ++i) {
            crc ^= buf[i];
            for (int j = 0; j < 8; ++j) {
                crc = (crc & 1) ? ((crc >> 1) ^ 0xA001) : (crc >> 1);
            }
        }
        return crc;
    }

    static void BuildAndSend(Network *net, int fd, uint8_t cmd, const uint8_t *payload, size_t pay_len) {
        const size_t total = 2 + 2 + 1 + 2 + pay_len + 2;
        uint8_t *frame = new uint8_t[total];
        if (!frame) {
            LOGE("MonProtocol: alloc failed for cmd=0x%02X", cmd);
            return;
        }
        size_t i = 0;
        frame[i++] = FRAME_HEADER_0;
        frame[i++] = FRAME_HEADER_1;
        frame[i++] = static_cast<uint8_t>((pay_len + 3) >> 8);
        frame[i++] = static_cast<uint8_t>((pay_len + 3) & 0xFF);
        frame[i++] = cmd;
        frame[i++] = static_cast<uint8_t>(seq_ >> 8);
        frame[i++] = static_cast<uint8_t>(seq_ & 0xFF);
        seq_++;
        if (payload && pay_len > 0) {
            memcpy(frame + i, payload, pay_len);
            i += pay_len;
        }
        uint16_t crc = CalcCrc16(frame, i);
        frame[i++] = static_cast<uint8_t>(crc & 0xFF);
        frame[i++] = static_cast<uint8_t>(crc >> 8);
        net->SendData(fd, frame, total);
        delete[] frame;
    }
}

MonProtocol::MonProtocol(Network *network) : Protocol(network) {
    heartbeat_timer_ = xTimerCreate(
        nullptr,
        pdMS_TO_TICKS(HEARTBEAT_INTERVAL_MS),
        pdTRUE,
        this,
        HeartbeatTimerCallback
    );
    configASSERT(heartbeat_timer_);
}

void MonProtocol::ChildStart() {
    LOGI("MonProtocol: connected on fd=%d, sending login", fd_);
    SendLogin();
    xTimerStart(heartbeat_timer_, 0);
}

void MonProtocol::SendLogin() {
    // TODO: 填充监控平台要求的登录帧（设备号、版本等）
    uint8_t payload[32] = {};
    BuildAndSend(network_, fd_, CMD_LOGIN, payload, sizeof(payload));
}

void MonProtocol::ReportHeartbeat() {
    BuildAndSend(network_, fd_, CMD_HEARTBEAT, nullptr, 0);
}

void MonProtocol::ReportDeviceStatus(uint8_t status_code) {
    uint8_t payload[1] = {status_code};
    BuildAndSend(network_, fd_, CMD_STATUS, payload, sizeof(payload));
}

void MonProtocol::ReportElectrical(float voltage, float current, float power) {
    uint8_t payload[12] = {};
    memcpy(payload + 0, &voltage, 4);
    memcpy(payload + 4, &current, 4);
    memcpy(payload + 8, &power,   4);
    BuildAndSend(network_, fd_, CMD_ELECTRICAL, payload, sizeof(payload));
}

void MonProtocol::ReportSession(const ChargingSession &session) {
    uint8_t payload[32] = {};
    memcpy(payload +  0, &session.start_time,           4);
    memcpy(payload +  4, &session.stop_time,            4);
    memcpy(payload +  8, &session.consumed_energy_kwh,  4);
    memcpy(payload + 12, &session.total_cost,           4);
    memcpy(payload + 16, &session.duration_seconds,     4);
    payload[20] = static_cast<uint8_t>(session.stop_reason);
    memcpy(payload + 21, session.card_id, sizeof(session.card_id));
    BuildAndSend(network_, fd_, CMD_SESSION, payload, sizeof(payload));
    LOGI("MonProtocol: session report sent");
}

void MonProtocol::OnDataReceived(const uint8_t *data, size_t length) {
    if (length < 9) return;
    if (data[0] != FRAME_HEADER_0 || data[1] != FRAME_HEADER_1) {
        LOGW("MonProtocol: invalid frame header");
        return;
    }

    uint8_t cmd = data[4];
    const uint8_t *payload = data + 7;
    size_t pay_len = (length >= 9) ? (length - 9) : 0;

    switch (cmd) {
        case CMD_LOGIN_ACK:
            LOGI("MonProtocol: login ack received");
            break;
        case CMD_HEARTBEAT_ACK:
            break;
        case CMD_OTA_REQUEST:
            HandleOtaRequest(payload, pay_len);
            break;
        case CMD_SET_PARAM:
            HandleSetParam(payload, pay_len);
            break;
        default:
            LOGW("MonProtocol: unknown cmd=0x%02X", cmd);
            break;
    }
}

void MonProtocol::HandleOtaRequest(const uint8_t *payload, size_t len) {
    LOGI("MonProtocol: OTA request received");
    auto &[cb, args] = event_handler_;
    if (cb) {
        cb(args, MON_OTA_REQUEST, nullptr);
    }
}

void MonProtocol::HandleSetParam(const uint8_t *payload, size_t len) {
    // TODO: 解析监控平台下发的参数配置
    LOGI("MonProtocol: set param received, len=%zu", len);
}

void MonProtocol::HeartbeatTimerCallback(TimerHandle_t timer) {
    auto *self = static_cast<MonProtocol *>(pvTimerGetTimerID(timer));
    self->ReportHeartbeat();
}
