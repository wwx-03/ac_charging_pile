#pragma once

#include "FreeRTOS.h"
#include "timers.h"

#include "protocol.hpp"
#include "billing/charging_session.hpp"

// ============================================================
// OpsProtocol — 运营平台协议
//
// 职责：
//   - 维持与运营平台的长连接（登录/心跳）
//   - 接收远程启/停充电指令
//   - 上报充电状态、实时计量、会话结算记录
//
// 使用方式：
//   1. 构造时注入 Network 和 BillingEngine 指针
//   2. Network CONNECTED(fd=0) 事件触发后调用 Start(fd)
//   3. 在 OnSessionCompleted 回调中调用 ReportSession()
//   4. 协议层事件（远程启/停）通过 RegisterEventHandler 向上分发
// ============================================================
class OpsProtocol : public Protocol {
public:
    OpsProtocol(Network *network);
    void OnDataReceived(const uint8_t *data, size_t length) override;

    // 主动上报接口
    void ReportHeartbeat();
    void ReportChargerStatus(uint8_t status_code);
    void ReportSession(const ChargingSession &session);
    void ReportRealTimeData(float energy_kwh, float cost);

private:
    TimerHandle_t heartbeat_timer_ = nullptr;

    static constexpr uint32_t HEARTBEAT_INTERVAL_MS = 30000;  // 30s 心跳
    static constexpr uint32_t REALTIME_INTERVAL_MS  = 60000;  // 60s 实时上报

    void ChildStart() override;
    void SendLogin();

    // 下行消息处理
    void HandleRemoteStart(const uint8_t *payload, size_t len);
    void HandleRemoteStop(const uint8_t *payload, size_t len);
    void HandleSetPrice(const uint8_t *payload, size_t len);
    void HandleOtaRequest(const uint8_t *payload, size_t len);

    static void HeartbeatTimerCallback(TimerHandle_t timer);
};

