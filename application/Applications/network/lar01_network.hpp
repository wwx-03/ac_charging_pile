#pragma once

#include "main.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"

#include <log/log>

#include "network.hpp"
#include "serial/serial.hpp"
#include "util/lock_guard.hpp"

// ============================================================
// Lar01Network — LAR01 模组 AT 指令网络驱动
//
// 双 TCP 连接：
//   connectID=0 (config::OPS_FD) → 运营平台
//   connectID=1 (config::MON_FD) → 监控平台
//
// 调用方式：
//   1. 构造时传入串口和 GPIO 引脚
//   2. 调用 Start()，内部自动完成拨号 → 建立双连接 → 触发 CONNECTED 事件
//   3. 收到数据后，DataReceivedEventData.fd 区分连接来源
//   4. 调用 SendData(fd, data, len) 向对应平台发送数据
// ============================================================
class Lar01Network : public Network {
public:
    Lar01Network(Serial *serial,
                 GPIO_TypeDef *power_port, uint16_t power_pin,
                 GPIO_TypeDef *reset_port, uint16_t reset_pin);
    ~Lar01Network();

    void Start() override;
    void SendData(int fd, const uint8_t *data, size_t length) override;

    // 运行时更新连接目标（修改后需重新触发连接）
    void SetOpsAddress(const char *ip, uint16_t port);
    void SetMonAddress(const char *ip, uint16_t port);

    void EnqueueMessage(const uint8_t *data, uint16_t size);

private:
    Serial        *serial_;
    GPIO_TypeDef  *power_port_;
    uint16_t       power_pin_;
    GPIO_TypeDef  *reset_port_;
    uint16_t       reset_pin_;

    char     ops_ip_[16]  = {};
    uint16_t ops_port_    = 0;
    char     mon_ip_[16]  = {};
    uint16_t mon_port_    = 0;

    QueueHandle_t  rx_queue_;
    QueueHandle_t  tx_queue_;
    SemaphoreHandle_t tx_mutex_;
    TaskHandle_t   task_to_notify_ = nullptr;
    const char    *expected_       = nullptr;

    bool SendATCommand(const char *command, const char *expected,
                       uint32_t retry, uint32_t timeout, bool need_copy = false);
    void TransmitTask();
    void ReceiveTask();
    void DispatchData(const uint8_t *data, size_t length);
    void DispatchQiurcData(const uint8_t *data, size_t length, const char *string, const char *start);
    void DispatchIccidData(const uint8_t *data, size_t length, const char *string, const char *start);
    void DispatchCsqData (const uint8_t *data, size_t length, const char *string, const char *start);
    void DispatchClkData (const uint8_t *data, size_t length, const char *string, const char *start);
    void DispatchQiopenData(const uint8_t *data, size_t length, const char *string, const char *start);

    bool Connect();
    bool ConnectOne(int fd, const char *ip, uint16_t port);

    void EnabledPower(bool en) { power_port_->BSRR = en ? power_pin_ : (power_pin_ << 16U); }
    void EnabledReset(bool en) { reset_port_->BSRR = en ? reset_pin_ : (reset_pin_ << 16U); }

    friend class LockGuard<Lar01Network>;
    void Lock()   { if (xSemaphoreTake(tx_mutex_, pdMS_TO_TICKS(100)) != pdTRUE) { LOGE("Lar01Network: failed to take TX mutex"); } }
    void Unlock() { if (xSemaphoreGive(tx_mutex_) != pdTRUE)                     { LOGE("Lar01Network: failed to give TX mutex"); } }
};
