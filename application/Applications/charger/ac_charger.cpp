#include "charger/ac_charger.hpp"

#include <string.h>
#include <time.h>

#include "FreeRTOS.h"
#include "task.h"

#include <custom>
#include <log/log>
#include <time/time>

#include "util/lock_guard.hpp"

struct QueueItem {
	bool outside;
	uint8_t event;
};

static constexpr size_t EVENT_QUEUE_DEPTH = 8;

static constexpr size_t CHARGING_CHECK_INTERVAL_MS = 1000;      // 充电检查间隔
static constexpr size_t LOWCURRENT_TIMEOUT_MS = 30 * 60 * 1000; // 30分钟低电流超时

AcCharger::AcCharger(size_t channel, Relay *relay, PwmController *pwm, CpDetector *cp, Meter *meter, Storage *storage)
		: channel_(channel)
		, relay_(relay)
		, pwm_(pwm)
		, cp_(cp)
		, meter_(meter) 
		, billing_(channel, this, meter, storage) {
	// 参数检查
	configASSERT(relay_);
	configASSERT(pwm_);
	configASSERT(cp_);
	configASSERT(meter_);
	configASSERT(storage);
	
	mutex_ = xSemaphoreCreateMutex();
	configASSERT(mutex_);

	event_queue_ = xQueueCreate(EVENT_QUEUE_DEPTH, sizeof(QueueItem));
	configASSERT(event_queue_);

	charging_timer_ = xTimerCreate(nullptr, pdMS_TO_TICKS(CHARGING_CHECK_INTERVAL_MS), pdTRUE, this, +[](TimerHandle_t timer) {
		auto *self = static_cast<AcCharger *>(pvTimerGetTimerID(timer));
		self->SendEvent(EV_CHARGING_CHECK);
	});
	configASSERT(charging_timer_);

	lowcurrent_timer_ = xTimerCreate(nullptr, pdMS_TO_TICKS(LOWCURRENT_TIMEOUT_MS), pdFALSE, this, +[](TimerHandle_t timer) {
		auto *self = static_cast<AcCharger *>(pvTimerGetTimerID(timer));
		self->SendEvent(EV_LOWCURRENT_TIMEOUT);
	});
	configASSERT(lowcurrent_timer_);

	cp->Enabled(false); // 初始状态下禁用 CP 检测，直到插枪后再启用
}

AcCharger::~AcCharger() {
	xTimerDelete(charging_timer_, pdMS_TO_TICKS(100));
	xTimerDelete(lowcurrent_timer_, pdMS_TO_TICKS(100));
	vQueueDelete(event_queue_);
}

void AcCharger::Start() {
	xTaskCreate(+[](void *args) {
		static_cast<AcCharger *>(args)->EventTask();
	}, nullptr, 256, this, tskIDLE_PRIORITY + 2, nullptr);
}

void AcCharger::SendFault(uint32_t fault_bitmask) {
	{
		LockGuard lock(mutex_);
		fault_bitmask_ |= fault_bitmask;
	}
	LOGE("AcCharger %d: fault occurred, bitmask=0x%08X", static_cast<int>(channel_), fault_bitmask);
	SendEvent(EV_FAULT_OCCURRED);
}

void AcCharger::ClearFault(uint32_t fault_bitmask) {
	{
		LockGuard lock(mutex_);
		fault_bitmask_ &= ~fault_bitmask;
	}
	if (!fault_bitmask_) {
		SendEvent(EV_FAULT_CLEARED);
	}
}

void AcCharger::SendEvent(Event event) {
	SendEvent(static_cast<uint8_t>(event), true);
}

void AcCharger::Authorize() {
	ClearAuthorization();
	balance_ = std::numeric_limits<float>::max();  // 即插即充时视为余额充足
	SendEvent(EV_AUTH_SUCCESS);
}

void AcCharger::Authorize(const uint8_t *card_id, size_t len) {
	ClearAuthorization();
	size_t copy_len = custom::min(len, sizeof(physic_card_id_));
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
	size_t copy_len = custom::min(sn_len, sizeof(transaction_sn_));
	memcpy(transaction_sn_, transaction_sn, copy_len);
	copy_len = custom::min(logic_card_id_len, sizeof(logic_card_id_));
	memcpy(logic_card_id_, logic_card_id, copy_len);
	copy_len = custom::min(physic_card_id_len, sizeof(physic_card_id_));
	memcpy(physic_card_id_, physic_card_id, copy_len);
	balance_ = balance;
	SendEvent(EV_AUTH_SUCCESS);
}

// ============================================================
// 私有实现
// ============================================================

void AcCharger::SendEvent(uint8_t event, bool outside) {
	QueueItem item{outside, event};
	if (xPortIsInsideInterrupt()) {
		xQueueSendFromISR(event_queue_, &item, nullptr);
	} else {
		if (xQueueSend(event_queue_, &item, pdMS_TO_TICKS(100)) != pdTRUE) {
			LOGE("AcCharger: event queue full, event=%d dropped", static_cast<int>(event));
		}
	}
}

void AcCharger::EventTask() {
	QueueItem item{};

	pwm_->SetDutyCycle(100.0f);
	pwm_->Start();
	vTaskDelay(pdMS_TO_TICKS(1000)); // 等待 PWM 稳定
	cp_->Enabled(true);

	while (true) {
		if (xQueueReceive(event_queue_, &item, portMAX_DELAY) == pdTRUE) {
			ProcessEvent(item.event, item.outside);
		}
	}
}

void AcCharger::ProcessEvent(uint8_t event, bool outside) {
	if (outside) {
		ProcessEvent(static_cast<Event>(event));
	} else {
		ProcessEvent(static_cast<PrivateEvent>(event));
	}
}

void AcCharger::ProcessEvent(Event event) {
	State next_state = FindNextState(event);
	StopReason reason = FindStopReason(event);
	TransitionTo(next_state, reason);
}

void AcCharger::ProcessEvent(PrivateEvent event) {
	switch (event) {
		case EV_CHARGING_CHECK:
			// 1. 检查 CP 电压
			CheckCpState();
			// 2. 检查电流
			CheckCurrent();
			// 3. 更新计费信息
			billing_.UpdateConsumption();
			break;
		case EV_LOWCURRENT_TIMEOUT:
			// 处理低电流超时事件
			SendEvent(EV_CHARGE_FULL);
			break;
		default:
			LOGW("AcCharger: unknown private event %d", static_cast<int>(event));
			break;
	}
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
		{READY, EV_PLUG_IN, CONNECTED},
		{READY, EV_AUTH_SUCCESS, CHARGING},

		// 等待插枪状态下:
		{WAITING_FOR_CONNECTED, EV_FAULT_OCCURRED, FAULT},
		{WAITING_FOR_CONNECTED, EV_UNPLUG, IDLE},
		{WAITING_FOR_CONNECTED, EV_PLUG_IN, STOPPING},
		{WAITING_FOR_CONNECTED, EV_READY, CHARGING},

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
		{EV_USER_STOP,  StopReason::USER_STOP}, 
		{EV_CHARGE_FULL,     StopReason::FULL},
		{EV_BALANCE_DONE,  StopReason::NORMAL},
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

	LOGI("AcCharger: %d -> %d\r\n", static_cast<int>(old_state), static_cast<int>(new_state));

	// 1. 操作 PWM 控制器
	if (new_state != IDLE && new_state != FAULT) {
		pwm_->SetDutyCycle(53.3f); // 其他状态全功率输出
	} else {
		pwm_->SetDutyCycle(100.0f); // IDLE/FAULT 状态不输出
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

	LOGI("AcCharger: charging started\r\n");
}

void AcCharger::OnLeaveCharging(StopReason reason) {
	// 1. 结束计费，获取会话记录
	ChargingSession session = billing_.StopSession(reason);

	// 2. 通知上层（协议层上报云端）
	auto &[cb, args] = session_completed_cb_;
	if (cb) {
		cb(args, session);
	}

	LOGI("AcCharger: charging stopped | reason=%d\r\n", static_cast<int>(reason));
}

void AcCharger::CheckCpState() {
	const std::pair<CpDetector::State, Event> cp_event_map[] = {
		{CpDetector::GROUNDING, EV_FAULT_OCCURRED},
		{CpDetector::PLUG_OUT,          EV_UNPLUG},
		{CpDetector::PLUG_IN,          EV_PLUG_IN},
		{CpDetector::READY,              EV_READY}
	};
	CpDetector::State cp_state = cp_->GetState();
	for (const auto &[s, e] : cp_event_map) {
		if (s == cp_state) {
			SendEvent(e);
			return;
		}
	}
}

void AcCharger::CheckCurrent() {
	if (state_ != CHARGING) {
		xTimerReset(lowcurrent_timer_, 0);
		return;
	}
	float current = meter_->GetCurrent(channel_);
	if (current < 0.1f) {
		// 电流过低，启动低电流定时器
		xTimerStart(lowcurrent_timer_, pdMS_TO_TICKS(100));
	} else {
		// 电流正常，停止低电流定时器
		xTimerStop(lowcurrent_timer_, pdMS_TO_TICKS(100));
	}
}
