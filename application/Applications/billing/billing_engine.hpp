#pragma once

#include <stdint.h>
#include <stddef.h>

#include "FreeRTOS.h"
#include "timers.h"

#include "charger/charger.hpp"
#include "billing/charging_session.hpp"
#include "meter/meter.hpp"
#include "storage/storage.hpp"


// ============================================================
// BillingModel — 计费模型定义
// ============================================================
struct BillingModel {
	uint16_t model_id;                   // 计费模型 ID（预留字段，当前固定为 0）
	float    price_per_kwh_sharp;        // 尖时段电价（元/千瓦时）
	float    service_fee_per_kwh_sharp;  // 尖时段服务费（元/千瓦时）
	float    price_per_kwh_peak;         // 峰时段电价（元/千瓦时）
	float    service_fee_per_kwh_peak;  // 峰时段服务费（元/千瓦时）
	float    price_per_kwh_flat;         // 平时段电价（元/千瓦时）
	float    service_fee_per_kwh_flat;  // 平时段服务费（元/千瓦时）
	float    price_per_kwh_valley;       // 谷时段电价（元/千瓦时）
	float    service_fee_per_kwh_valley; // 谷时段服务费（元/千瓦时）
	uint8_t  time_slot[48];              // 48 个半小时的时段划分（0=尖, 1=峰, 2=平, 3=谷）
};

// ============================================================
// 
// BillingEngine — 底层计费引擎
//
// 职责：
//   1. 记录充电会话开始/结束时的电表读数
//   2. 计算消耗电量和费用
//   3. 将会话快照持久化到 Flash，防止断电丢失
//   4. 提供实时计费查询（供协议层上报）
//
// 使用方式：
//   1. 授权成功后调用 StartSession()
//   2. 停止充电时调用 StopSession()，获得完整会话记录
//   3. 上报实时数据时调用 GetCurrentEnergy() / GetCurrentCost()
//   4. 系统启动时调用 RecoverSession() 恢复断电前会话
//
// ============================================================
class BillingEngine {
public:
	explicit BillingEngine(size_t channel, Charger *charger, Meter *meter, Storage *storage);
	~BillingEngine();

	// 开始计费会话
	// @param transaction_sn     交易流水号（远程启动时提供，可用于关联订单，但本地不强制要求使用）
	// @pram  sn_len             交易流水号长度
	// @param logic_card_id      逻辑卡 ID（平台账号 ID，远程启动时提供）
	// @param logic_card_id_len  逻辑卡 ID 长度
	// @param physic_card_id     物理卡 ID（刷卡时提供，远程启动时可选）
	// @param physic_card_id_len 物理卡 ID 长度
	// @param balance            账户余额（远程启动时提供，结束时可选，用于余额不足时自动停止）
	// @note 即插即充时验证信息全传入空，余额传入最大值
	// @note 本地刷卡时传入物理卡 ID，其他参数传入空/0，余额传入最大值
	// @note 远程启动时由协议层调用，传入完整授权信息和余额
	void StartSession(const uint8_t *transaction_sn, size_t sn_len, const uint8_t *logic_card_id, size_t logic_card_id_len,
		const uint8_t *physic_card_id, size_t physic_card_id_len, float balance);

	// 结束计费会话，返回完整会话记录
	// @param reason     停止原因
	// @return 充电会话的完整记录（包含消耗电量、费用、持续时间等信息）
	ChargingSession StopSession(StopReason reason);

	// 实时查询（会话未激活时返回 0）
	float GetCurrentEnergy() const;  // 本次已消耗电量（kWh）
	float GetCurrentCost()   const;  // 本次已产生费用（元）

	// 电价配置
	void SetBillingModel(const BillingModel &model) { billing_model_ = model; }
	const BillingModel &GetBillingModel() const { return billing_model_; }

	// 状态查询
	bool IsSessionActive() const { return session_active_; }
	const ChargingSession &GetCurrentSession() const { return current_session_; }

	// 将当前会话快照写入 Flash（由应用层定期调用）
	void SaveSnapshot();

	// 系统上电时调用：从 Flash 恢复未完成的会话
	// @return true 若存在有效的未完成会话
	bool RecoverSession();

private:
	size_t        channel_;
	Charger       *charger_;
	Meter         *meter_;
	Storage       *storage_;
	TimerHandle_t timer_;

	bool            session_active_ = false;
	BillingModel    billing_model_  = {};
	ChargingSession current_session_ = {};

	// 禁止复制和赋值
	BillingEngine(const BillingEngine &) = delete;
	BillingEngine &operator=(const BillingEngine &) = delete;

	// 更新当前会话的消耗电量和费用（由计时器定期调用），并在余额不足时通知充电模块停止充电
	void UpdateConsumption();

	// 清除 Flash 中的会话快照（由应用层在会话正常结束后调用）
	void EraseSnapshot();
};
