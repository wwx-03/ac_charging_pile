#include "charger/ac_charger.hpp"

#include <string.h>
#include <time.h>

#include "FreeRTOS.h"
#include "task.h"

#include <log/log>
#include <time/time>

static constexpr size_t EVENT_QUEUE_DEPTH = 8;

AcCharger::AcCharger(size_t channel, Relay *relay, PwmController *pwm, CpDetector *cp, Meter *meter, Storage *storage)
		: channel_(channel)
		, relay_(relay)
		, pwm_(pwm)
		, cp_(cp)
		, meter_(meter) 
		, billing_(channel, this, meter, storage) {
	event_queue_ = xQueueCreate(EVENT_QUEUE_DEPTH, sizeof(Event));
	configASSERT(event_queue_);

	xTaskCreate(+[](void *args) {
		static_cast<AcCharger *>(args)->EventTask();
	}, nullptr, 256, this, tskIDLE_PRIORITY + 2, nullptr);
}

AcCharger::~AcCharger() {
	vQueueDelete(event_queue_);
}

void AcCharger::SendFault(uint32_t fault_bitmask) {
	fault_bitmask_ |= fault_bitmask;
	SendEvent(EV_FAULT_OCCURRED);
}

void AcCharger::ClearFault(uint32_t fault_bitmask) {
	fault_bitmask_ &= ~fault_bitmask;
	if (!fault_bitmask_) {
		SendEvent(EV_FAULT_CLEARED);
	}
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
	State new_state   = FindNextState(event);
	StopReason reason = FindStopReason(event);
	TransitionTo(new_state, reason);
}

Charger::State AcCharger::FindNextState(Event event) const {
	struct StateTransition { State current_state; Event event; State next_state; };

	const StateTransition transitions[] = {
		// 空闲状态下有以下几种情况:
		{IDLE, EV_FAULT_OCCURRED, FAULT},
		{IDLE, EV_PLUG_IN, CONNECTED},
		{IDLE, EV_READY, READY},
		{IDLE, EV_AUTH_SUCCESS, WAITING_FOR_CONNECTED},

		// 已插枪但车未就绪状态下:
		{CONNECTED, EV_FAULT_OCCURRED, FAULT},
		{CONNECTED, EV_UNPLUG, IDLE},
		{CONNECTED, EV_READY, READY},
		{CONNECTED, EV_AUTH_SUCCESS, STOPPING},

		// 已插枪车就绪状态下：
		{READY, EV_FAULT_OCCURRED, FAULT},
		{READY, EV_UNPLUG, IDLE},
		{READY, EV_AUTH_SUCCESS, CHARGING},

		// 等待插枪状态下:
		{WAITING_FOR_CONNECTED, EV_FAULT_OCCURRED, FAULT},
		{WAITING_FOR_CONNECTED, EV_UNPLUG, IDLE},
		{WAITING_FOR_CONNECTED, EV_PLUG_IN, STOPPING},
		{WAITING_FOR_CONNECTED, EV_READY, READY},

		// 充电中状态下:
		{CHARGING, EV_FAULT_OCCURRED, FAULT},
		{CHARGING, EV_UNPLUG, IDLE},
		{CHARGING, EV_PLUG_IN, STOPPING}, // 充电中车辆S2断开 
		{CHARGING, EV_USER_STOP, COMPLETE},
		{CHARGING, EV_BALANCE_DONE, COMPLETE},
		{CHARGING, EV_CHARGE_FULL, COMPLETE},

		// 停止中状态下:
		{STOPPING, EV_FAULT_OCCURRED, FAULT},
		{STOPPING, EV_UNPLUG, IDLE},
		{STOPPING, EV_READY, CHARGING}, // 停止过程中车辆S2重新连接
		{STOPPING, EV_USER_STOP, COMPLETE},
		{STOPPING, EV_BALANCE_DONE, COMPLETE},
		{STOPPING, EV_CHARGE_FULL, COMPLETE},

		// 充电完成状态下:
		{COMPLETE, EV_FAULT_OCCURRED, FAULT},
		{COMPLETE, EV_UNPLUG, IDLE},

		// 故障状态下:
		{FAULT, EV_FAULT_CLEARED, IDLE},
	};
	UNUSED(sizeof(transitions)); // 查看栈占用时发现这个数组比较大，放在栈上可能会有问题，可以考虑改成静态成员或全局变量

	for (const auto &t : transitions) {
		if (t.current_state == state_ && t.event == event) {
			return t.next_state;
		}
	}
	return state_;
}

StopReason AcCharger::FindStopReason(Event event) const {
	const std::pair<Event, StopReason> event_reason_map[] = {
		{EV_USER_STOP, StopReason::USER_STOP}, 
		{EV_CHARGE_FULL, StopReason::FULL},
		{EV_BALANCE_DONE, StopReason::NORMAL},
		{EV_FAULT_OCCURRED, StopReason::FAULT},
	};

	for (const auto &[e, r] : event_reason_map) {
		if (e == event) {
			return r;
		}
	}
	return StopReason::UNKNOWN; // 默认停止原因
}

void AcCharger::TransitionTo(State new_state, StopReason reason) {
	if (new_state == state_) {
		return;
	}

	State old_state = state_;
	state_          = new_state;

	LOGI("AcCharger: %d -> %d", static_cast<int>(old_state), static_cast<int>(new_state));

	// 1. 操作 PWM 控制器
	if (new_state != IDLE && new_state != FAULT) {
		pwm_->SetDutyCycle(53.3f); // 其他状态全功率输出
	} else {
		pwm_->SetDutyCycle(0.0f); // IDLE/FAULT 状态不输出
	}

	// 2. 操作继电器，仅在充电状态开启，其他状态关闭
	if (new_state == CHARGING) {
		relay_->SetState(true);
	} else {
		relay_->SetState(false);
	}

	if (new_state == CHARGING) {
		OnEnterCharging();
	}

	if ((old_state == CHARGING || old_state == STOPPING) && (new_state != CHARGING && new_state != STOPPING)) {
		OnLeaveCharging(reason);
	} else {
		UNUSED(reason);
	}

	auto &[cb, args] = state_changed_cb_;
	if (cb) {
		cb(args, old_state, new_state);
	}
}

void AcCharger::OnEnterCharging() {
	// 1. 启动计费
	billing_.StartSession(transaction_sn_, sizeof(transaction_sn_),
		logic_card_id_, sizeof(logic_card_id_),
		physic_card_id_, sizeof(physic_card_id_),
		balance_);

	LOGI("AcCharger: charging started");
}

void AcCharger::OnLeaveCharging(StopReason reason) {
	// 1. 结束计费，获取会话记录
	ChargingSession session = billing_.StopSession(reason);

	// 2. 通知上层（协议层上报云端）
	auto &[cb, args] = session_completed_cb_;
	if (cb) {
		cb(args, session);
	}

	LOGI("AcCharger: charging stopped | reason=%d", static_cast<int>(reason));
}
