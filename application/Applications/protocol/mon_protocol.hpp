#pragma once

#include "FreeRTOS.h"
#include "timers.h"

#include "protocol.hpp"
#include "billing/charging_session.hpp"

// ============================================================
// MonProtocol — 监控平台协议
//
// 职责：
//   - 维持与监控平台的长连接（登录/心跳）
//   - 周期性上报设备状态、电气参数
//   - 上报充电会话记录（与运营平台共享计费数据）
//   - 响应监控平台的 OTA 升级指令
// ============================================================
class MonProtocol : public Protocol {
public:
    explicit MonProtocol(Network *network);
    void OnDataReceived(const uint8_t *data, size_t length) override;

    // 主动上报接口
    void ReportHeartbeat();
    void ReportDeviceStatus(uint8_t status_code);
    void ReportSession(const ChargingSession &session);
    void ReportElectrical(float voltage, float current, float power);

private:
    TimerHandle_t heartbeat_timer_ = nullptr;

    static constexpr uint32_t HEARTBEAT_INTERVAL_MS = 60000;  // 60s 心跳

    void ChildStart() override;
    void SendLogin();

    void HandleOtaRequest(const uint8_t *payload, size_t len);
    void HandleSetParam(const uint8_t *payload, size_t len);

    static void HeartbeatTimerCallback(TimerHandle_t timer);
};