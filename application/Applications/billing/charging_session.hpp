#pragma once

#include <stdint.h>

// 充电停止原因
enum class StopReason : uint8_t {
    NORMAL       = 0,  // 充电完成正常停止
    USER_STOP    = 1,  // 用户刷卡/按键停止
    REMOTE_STOP  = 2,  // 平台远程停止
    FULL         = 3,  // 电量充满自动停止
    FAULT        = 4,  // 故障停止
    POWER_LOSS   = 5,  // 异常断电后恢复
    TIMEOUT      = 6,  // 超时停止
    OVERCURRENT  = 7,  // 过流保护停止
};

// 单次充电会话的完整计费记录
struct ChargingSession {
    uint8_t    session_id[16];               // 会话 ID（本地生成的唯一标识） 
    uint8_t    logic_card_id[8];             // 逻辑卡 ID（平台账号 ID，远程启动时提供）
    uint8_t    physic_card_id[8];            // 物理卡 ID（刷卡时提供，远程启动时可选）
    uint32_t   start_time;                   // 会话开始 UNIX 时间戳（秒）
    uint32_t   stop_time;                    // 会话结束 UNIX 时间戳（秒）
    float      start_energy_kwh;             // 开始时电表读数（kWh，累计值）
    float      stop_energy_kwh;              // 结束时电表读数（kWh，累计值）
    float      previous_consumed_energy_kwh; // 上次检查消耗的电量（kWh，防止电量异常跳变）
    float      consumed_energy_kwh;          // 本次消耗电量（kWh）
    float      consumed_energy_kwh_sharp;    // 本次尖时段消耗电量（kWh）
    float      consumed_energy_kwh_peak;     // 本次峰时段消耗电量（kWh）
    float      consumed_energy_kwh_flat;     // 本次平时段消耗电量（kWh）
    float      consumed_energy_kwh_valley;   // 本次谷时段消耗电量（kWh）
    float      balance;                      // 账户余额（元，远程启动时提供，结束时可选）
    float      total_cost;                   // 本次计费金额（元）
    float      cost_sharp_kwh;               // 尖时段费用（元）
    float      service_fee_sharp_kwh;        // 尖时段服务费（元）
    float      cost_peak_kwh;                // 峰时段费用（元）
    float      service_fee_peak_kwh;         // 峰时段服务费（元）
    float      cost_flat_kwh;                // 平时段费用（元）
    float      service_fee_flat_kwh;         // 平时段服务费（元）
    float      cost_valley_kwh;              // 谷时段费用（元）
    float      service_fee_valley_kwh;       // 谷时段服务费（元）
    uint32_t   duration_seconds;             // 充电持续时间（秒）
    StopReason stop_reason;                  // 停止原因
    bool       valid;                        // 数据有效性标志（用于 Flash 校验）
};
