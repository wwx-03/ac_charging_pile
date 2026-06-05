#pragma once

#include "FreeRTOS.h"
#include "queue.h"

#include "charger/charger.hpp"
#include "billing/billing_engine.hpp"
#include "relay/relay.hpp"
#include "cp_detector/cp_detector.hpp"
#include "meter/meter.hpp"

// ============================================================
//
// AcCharger — 交流充电桩状态机具体实现
//
// 状态流转：
//   IDLE ──(插枪)──? PLUGGED_IN ──(授权)──? AUTHORIZING
//                                               │
//                                     (成功)?──┤──?(失败) → PLUGGED_IN
//                                               │
//                                        CHARGING ──(停止/故障)──? STOPPING ──? IDLE
//   任意状态 ──(拔枪)──? IDLE
//   任意状态 ──(故障)──? FAULT ──(故障清除)──? IDLE
//
// ============================================================
class AcCharger : public Charger {
public:
	AcCharger(size_t channel, Relay *relay, CpDetector *cp, Meter *meter);
	~AcCharger();

	void SendEvent(Event event) override;
	void Authorize() override;
	void Authorize(const uint8_t *card_id, size_t len) override;
	void Authorize(const uint8_t *transaction_sn, 
	               size_t sn_len,
	               const uint8_t *logic_card_id, 
	               size_t logic_card_id_len,
	               const uint8_t *physic_card_id, 
	               size_t physic_card_id_len,
	               float balance) override;
	State GetState() const override { return state_; }

private:
	size_t        channel_;
	BillingEngine billing_;
	Relay         *relay_;
	CpDetector    *cp_;
	Meter         *meter_;

	uint8_t transaction_sn_[16] = {};
	uint8_t logic_card_id_[8]   = {};
	uint8_t physic_card_id_[8]  = {};
	float   balance_            = 0.0f;

	State   state_ = IDLE;

	QueueHandle_t event_queue_; 

	void EventTask();
	void ProcessEvent(Event event);
	void TransitionTo(State new_state);

	// 各状态的进入/退出动作
	void OnEnterCharging();
	void OnLeaveCharging(StopReason reason);

	void ClearAuthorization() {
		memset(transaction_sn_, 0, sizeof(transaction_sn_));
		memset(logic_card_id_, 0, sizeof(logic_card_id_));
		memset(physic_card_id_, 0, sizeof(physic_card_id_));
		balance_ = 0.0f;
	}
};
