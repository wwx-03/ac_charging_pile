#include "protocol/ops_protocol.hpp"

#include <string.h>
#include <log/log>

// ============================================================
// 消息帧格式（示例，实际按甲方协议文档调整）
//
//  | 起始符(2B) | 长度(2B) | 命令码(1B) | 序列号(2B) | 数据(nB) | CRC(2B) |
//
// 命令码定义（上行）：
//   0x01 登录
//   0x03 心跳
//   0x05 状态上报
//   0x07 实时数据上报
//   0x09 充电结算上报
//
// 命令码定义（下行）：
//   0x02 登录响应
//   0x04 心跳响应
//   0x80 远程启动
//   0x81 远程停止
//   0x82 设置电价
//   0x90 OTA 升级请求
// ============================================================

namespace {
    static constexpr uint8_t  FRAME_HEADER_0 = 0x68;
    static constexpr uint8_t  FRAME_HEADER_1 = 0x68;

    static constexpr uint8_t  CMD_LOGIN          = 0x01;
    static constexpr uint8_t  CMD_HEARTBEAT      = 0x03;
    static constexpr uint8_t  CMD_STATUS_REPORT  = 0x05;
    static constexpr uint8_t  CMD_REALTIME_DATA  = 0x07;
    static constexpr uint8_t  CMD_SESSION_REPORT = 0x09;

    static constexpr uint8_t  CMD_LOGIN_ACK      = 0x02;
    static constexpr uint8_t  CMD_HEARTBEAT_ACK  = 0x04;
    static constexpr uint8_t  CMD_REMOTE_START   = 0x80;
    static constexpr uint8_t  CMD_REMOTE_STOP    = 0x81;
    static constexpr uint8_t  CMD_SET_PRICE      = 0x82;
    static constexpr uint8_t  CMD_OTA_REQUEST    = 0x90;

    static uint16_t seq_ = 0;

    // 简单 CRC-16（MODBUS）占位，可替换为实际算法
    static uint16_t CalcCrc16(const uint8_t *buf, size_t len) {
        uint16_t crc = 0xFFFF;
        for (size_t i = 0; i < len; ++i) {
            crc ^= buf[i];
            for (int j = 0; j < 8; ++j) {
                if (crc & 0x0001) {
                    crc = (crc >> 1) ^ 0xA001;
                } else {
                    crc >>= 1;
                }
            }
        }
        return crc;
    }

    // 构建帧并发送
    static void BuildAndSend(Network *net, int fd, uint8_t cmd, const uint8_t *payload, size_t pay_len) {
        const size_t total = 2 + 2 + 1 + 2 + pay_len + 2;
        uint8_t *frame = new uint8_t[total];
        if (!frame) {
            LOGE("OpsProtocol: alloc failed for cmd=0x%02X", cmd);
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

OpsProtocol::OpsProtocol(Network *network) : Protocol(network) {
    heartbeat_timer_ = xTimerCreate(
        nullptr,
        pdMS_TO_TICKS(HEARTBEAT_INTERVAL_MS),
        pdTRUE,   // auto-reload
        this,
        HeartbeatTimerCallback
    );
    configASSERT(heartbeat_timer_);
}

void OpsProtocol::ChildStart() {
    LOGI("OpsProtocol: connected on fd=%d, sending login", fd_);
    SendLogin();
    xTimerStart(heartbeat_timer_, 0);
}

void OpsProtocol::SendLogin() {
    // TODO: 填充设备 SN、ICCID、版本号等登录信息
    uint8_t payload[32] = {};
    // payload[0..n] = SN bytes, firmware version, etc.
    BuildAndSend(network_, fd_, CMD_LOGIN, payload, sizeof(payload));
}

void OpsProtocol::ReportHeartbeat() {
    BuildAndSend(network_, fd_, CMD_HEARTBEAT, nullptr, 0);
    LOGI("OpsProtocol: heartbeat sent");
}

void OpsProtocol::ReportChargerStatus(uint8_t status_code) {
    uint8_t payload[1] = {status_code};
    BuildAndSend(network_, fd_, CMD_STATUS_REPORT, payload, sizeof(payload));
}

void OpsProtocol::ReportRealTimeData(float energy_kwh, float cost) {
    uint8_t payload[8] = {};
    // 按协议格式编码 energy_kwh 和 cost
    memcpy(payload + 0, &energy_kwh, 4);
    memcpy(payload + 4, &cost,       4);
    BuildAndSend(network_, fd_, CMD_REALTIME_DATA, payload, sizeof(payload));
    LOGI("OpsProtocol: realtime | %.3f kWh | %.2f yuan", energy_kwh, cost);
}

void OpsProtocol::ReportSession(const ChargingSession &session) {
    // 编码会话数据（按实际协议字段顺序填写）
    uint8_t payload[32] = {};
    memcpy(payload +  0, &session.start_time,           4);
    memcpy(payload +  4, &session.stop_time,            4);
    memcpy(payload +  8, &session.consumed_energy_kwh,  4);
    memcpy(payload + 12, &session.total_cost,           4);
    memcpy(payload + 16, &session.duration_seconds,     4);
    payload[20] = static_cast<uint8_t>(session.stop_reason);
    memcpy(payload + 21, session.card_id, sizeof(session.card_id));
    BuildAndSend(network_, fd_, CMD_SESSION_REPORT, payload, sizeof(payload));
    LOGI("OpsProtocol: session report sent | %.3f kWh | %.2f yuan",
         session.consumed_energy_kwh, session.total_cost);
}

void OpsProtocol::OnDataReceived(const uint8_t *data, size_t length) {
    // 最小帧长：起始(2) + 长度(2) + 命令(1) + 序列号(2) + CRC(2) = 9
    if (length < 9) {
        return;
    }
    if (data[0] != FRAME_HEADER_0 || data[1] != FRAME_HEADER_1) {
        LOGW("OpsProtocol: invalid frame header");
        return;
    }

    uint8_t cmd = data[4];
    const uint8_t *payload = data + 7;
    size_t pay_len = (length >= 9) ? (length - 9) : 0;

    switch (cmd) {
        case CMD_LOGIN_ACK:
            LOGI("OpsProtocol: login ack received");
            break;
        case CMD_HEARTBEAT_ACK:
            break;
        case CMD_REMOTE_START:
            HandleRemoteStart(payload, pay_len);
            break;
        case CMD_REMOTE_STOP:
            HandleRemoteStop(payload, pay_len);
            break;
        case CMD_SET_PRICE:
            HandleSetPrice(payload, pay_len);
            break;
        case CMD_OTA_REQUEST:
            HandleOtaRequest(payload, pay_len);
            break;
        default:
            LOGW("OpsProtocol: unknown cmd=0x%02X", cmd);
            break;
    }
}

void OpsProtocol::HandleRemoteStart(const uint8_t *payload, size_t len) {
    LOGI("OpsProtocol: remote start received");
    auto &[cb, args] = event_handler_;
    if (cb) {
        cb(args, OPS_REMOTE_START_REQUEST, const_cast<uint8_t *>(payload));
    }
}

void OpsProtocol::HandleRemoteStop(const uint8_t *payload, size_t len) {
    LOGI("OpsProtocol: remote stop received");
    auto &[cb, args] = event_handler_;
    if (cb) {
        cb(args, OPS_REMOTE_STOP_REQUEST, const_cast<uint8_t *>(payload));
    }
}

void OpsProtocol::HandleSetPrice(const uint8_t *payload, size_t len) {
    if (len < 4) return;
    float price = 0.0f;
    memcpy(&price, payload, 4);
    LOGI("OpsProtocol: set price %.4f yuan/kWh", price);
    // TODO: 通知 BillingEngine 更新电价
    // Board::GetInstance().GetBillingEngine()->SetPricePerKwh(price);
}

void OpsProtocol::HandleOtaRequest(const uint8_t *payload, size_t len) {
    LOGI("OpsProtocol: OTA request received");
    auto &[cb, args] = event_handler_;
    if (cb) {
        cb(args, OPS_OTA_REQUEST, nullptr);
    }
}

void OpsProtocol::HeartbeatTimerCallback(TimerHandle_t timer) {
    auto *self = static_cast<OpsProtocol *>(pvTimerGetTimerID(timer));
    self->ReportHeartbeat();
}
