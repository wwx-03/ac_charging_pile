#include "charger/ac_charger.hpp"

#include <string.h>
#include <time.h>

#include "FreeRTOS.h"
#include "task.h"

#include <log/log>
#include <time/time>

static constexpr size_t EVENT_QUEUE_DEPTH = 8;

AcCharger::AcCharger(BillingEngine *billing, Relay *relay, CPDetector *cp, Meter *meter)
		: billing_(billing)
		, relay_(relay)
		, cp_(cp)
		, meter_(meter) {
	event_queue_ = xQueueCreate(EVENT_QUEUE_DEPTH, sizeof(Event));
	configASSERT(event_queue_);

	xTaskCreate(+[](void *args) {
		static_cast<AcCharger *>(args)->EventTask();
	}, nullptr, 256, this, tskIDLE_PRIORITY + 2, nullptr);
}

AcCharger::~AcCharger() {
	vQueueDelete(event_queue_);
}

void AcCharger::SendEvent(Event event) {
	if (xPortIsInsideInterrupt()) {
		xQueueSendFromISR(event_queue_, &event, nullptr);
	} else {
		if (xQueueSend(event_queue_, &event, pdMS_TO_TICKS(100)) != pdTRUE) {
			LOGE("AcCharger: event queue full, event=%d dropped", static_cast<int>(event));
		}
	}
}

void AcCharger::Authorize() {
	ClearAuthorization();
	balance_ = std::numeric_limits<float>::max();  // 即插即充时视为余额充足
	SendEvent(EV_AUTH_SUCCESS);
}

void AcCharger::Authorize(const uint8_t *card_id, size_t len) {
	ClearAuthorization();
	size_t copy_len = std::min(len, sizeof(physic_card_id_));
	memcpy(physic_card_id_, card_id, copy_len);
	balance_ = std::numeric_limits<float>::max();  // 刷卡时先假设余额充足，具体金额由计费引擎计算后通知
	SendEvent(EV_AUTH_SUCCESS);
}

void AcCharger::Authorize(const uint8_t *transaction_sn, 
		size_t sn_len,
		const uint8_t *logic_card_id, 
		size_t logic_card_id_len,
		const uint8_t *physic_card_id, 
		size_t physic_card_id_len,
		float balance) {
	ClearAuthorization();
	size_t copy_len = std::min(sn_len, sizeof(transaction_sn_));
	memcpy(transaction_sn_, transaction_sn, copy_len);
	copy_len = std::min(logic_card_id_len, sizeof(logic_card_id_));
	memcpy(logic_card_id_, logic_card_id, copy_len);
	copy_len = std::min(physic_card_id_len, sizeof(physic_card_id_));
	memcpy(physic_card_id_, physic_card_id, copy_len);
	balance_ = balance;
	SendEvent(EV_AUTH_SUCCESS);
}

// ============================================================
// 私有实现
// ============================================================

void AcCharger::EventTask() {
	Event event{};
	while (true) {
		if (xQueueReceive(event_queue_, &event, portMAX_DELAY) == pdTRUE) {
			ProcessEvent(event);
		}
	}
}

void AcCharger::ProcessEvent(Event event) {

	LOGI("AcCharger: state=%d event=%d", static_cast<int>(state_), static_cast<int>(event));

	// 全局事件（任意状态均处理）
	if (event == EV_FAULT_OCCURRED) {
		if (state_ == CHARGING) {
			OnLeaveCharging(StopReason::FAULT);
		}
		TransitionTo(FAULT);
		return;
	}

	if (event == EV_UNPLUG) {
		if (state_ == CHARGING) {
			OnLeaveCharging(StopReason::FAULT);  // 充电中拔枪视为异常
		}
		TransitionTo(IDLE);
		return;
	}

	// 状态机转换
	switch (state_) {

		case IDLE:
			if (event == EV_PLUG_IN) {
				TransitionTo(PLUGGED_IN);
			}
			break;

		case PLUGGED_IN:
			if (event == EV_AUTH_SUCCESS) {
				// 本地刷卡授权成功：直接进入充电
				// 若需等待平台确认，可先 TransitionTo(AUTHORIZING)
				TransitionTo(CHARGING);
			} else if (event == EV_REMOTE_START) {
				// 平台远程启动
				TransitionTo(CHARGING);
			}
			break;

		case AUTHORIZING:
			if (event == EV_AUTH_SUCCESS) {
				TransitionTo(CHARGING);
			} else if (event == EV_AUTH_FAIL) {
				LOGW("AcCharger: auth failed, back to PLUGGED_IN");
				TransitionTo(PLUGGED_IN);
			}
			break;

		case WAITING_FOR_READY:
			if (event == EV_REMOTE_START || event == EV_AUTH_SUCCESS) {
				TransitionTo(CHARGING);
			}
			break;

		case CHARGING:
			if (event == EV_USER_STOP) {
				OnLeaveCharging(StopReason::USER_STOP);
				TransitionTo(STOPPING);
			} else if (event == EV_REMOTE_STOP) {
				OnLeaveCharging(StopReason::REMOTE_STOP);
				TransitionTo(STOPPING);
			} else if (event == EV_CHARGE_FULL) {
				OnLeaveCharging(StopReason::FULL);
				TransitionTo(STOPPING);
			} else if (event == EV_OVERCURRENT) {
				OnLeaveCharging(StopReason::OVERCURRENT);
				TransitionTo(FAULT);
			}
			break;

		case STOPPING:
			// 等待继电器确认断开后自动回到 PLUGGED_IN 或 IDLE
			TransitionTo(IDLE);
			break;

		case FAULT:
			if (event == EV_FAULT_CLEARED) {
				TransitionTo(IDLE);
			}
			break;

		default:
			break;
	}
}

void AcCharger::TransitionTo(State new_state) {
	if (new_state == state_) {
		return;
	}

	State old_state = state_;
	state_          = new_state;

	LOGI("AcCharger: %d -> %d", static_cast<int>(old_state), static_cast<int>(new_state));

	// 状态进入动作
	if (new_state == CHARGING) {
		OnEnterCharging();
	}

	auto &[cb, args] = state_changed_cb_;
	if (cb) {
		cb(args, old_state, new_state);
	}
}

void AcCharger::OnEnterCharging() {
	// 1. 闭合继电器，接通电源
	if (relay_) {
		relay_->SetState(true);
	}

	// 2. 启动计费
	billing_->StartSession(transaction_sn_, sizeof(transaction_sn_),
		logic_card_id_, sizeof(logic_card_id_),
		physic_card_id_, sizeof(physic_card_id_),
		balance_);

	LOGI("AcCharger: charging started");
}

void AcCharger::OnLeaveCharging(StopReason reason) {
	// 1. 断开继电器，切断电源
	if (relay_) {
		relay_->SetState(false);
	}

	// 2. 结束计费，获取会话记录
	ChargingSession session = billing_->StopSession(reason);

	// 3. 通知上层（协议层上报云端）
	auto &[cb, args] = session_completed_cb_;
	if (cb) {
		cb(args, session);
	}

	LOGI("AcCharger: charging stopped | reason=%d", static_cast<int>(reason));
}
