#include "application.hpp"

#include "iwdg.h"

#include <log/log>

#include "fault_bitmask.hpp"
#include "system_info.hpp"
#include "board/board.hpp"
#include "charger/charger.hpp"
#include "network/network.hpp"
#include "protocol/ops_protocol.hpp"
#include "protocol/mon_protocol.hpp"
#include "billing/charging_session.hpp"

namespace {

	struct ScheduleEventData {
		custom::function<void(void *)> callback;
		void *args;
	};

} // namespace

Application::Application() {
	event_queue_ = xQueueCreate(3, sizeof(QueueItem));
	configASSERT(event_queue_);

	timer_ = xTimerCreate(nullptr, pdMS_TO_TICKS(1000), pdTRUE, this, +[](TimerHandle_t timer) {
		auto *self = static_cast<Application *>(pvTimerGetTimerID(timer));
		self->TimerCallback();
	});
	configASSERT(timer_);
}

// ============================================================
// 模块接线
// ============================================================
static void WireModules() {

	auto &board = Board::GetInstance();

	for (size_t i = 0; i < board.GetNumChargers(); ++i) {
		auto *charger = board.GetCharger(i);
		charger->OnStateChanged(+[](void *args, Charger::State old_state, Charger::State new_state) {
			size_t channel = reinterpret_cast<size_t>(args);
			auto &board = Board::GetInstance();
			auto *led = board.GetLed(channel);
			led->OnStateChanged(new_state);
		}, reinterpret_cast<void *>(i));

		charger->OnSessionCompleted(+[](void *args, const ChargingSession &session) {

		}, reinterpret_cast<void *>(i));

		auto *cp = board.GetCpDetector(i);
		cp->OnStateChanged(+[](void *args, CpDetector::State old_state, CpDetector::State new_state) {
			std::pair<CpDetector::State, Charger::Event> state_event_map[] = {
				{CpDetector::GROUNDING, Charger::EV_FAULT_OCCURRED},
				{CpDetector::PLUG_OUT,          Charger::EV_UNPLUG},
				{CpDetector::PLUG_IN,          Charger::EV_PLUG_IN},
				{CpDetector::READY,              Charger::EV_READY},
			};

			size_t channel = reinterpret_cast<size_t>(args);
			auto &board = Board::GetInstance();
			auto *charger = board.GetCharger(channel);

			// 特殊情况处理：接地状态直接上报故障，且优先级最高；即插即充场景下 CP 进入 READY 状态直接授权开始充电
			if (new_state == CpDetector::GROUNDING) {
				charger->SendFault(fault_bitmask::GROUND_FAULT);
				return;
			} else if (old_state == CpDetector::GROUNDING) {
				charger->ClearFault(fault_bitmask::GROUND_FAULT);
				return;
			} else if (new_state == CpDetector::READY && board.GetChargerType() == Board::PLUG_AND_CHARGE) {
				charger->Authorize();
				return;
			}

			for (const auto &[state, event] : state_event_map) {
				if (new_state == state) {
					charger->SendEvent(event);
					break;
				}
			}
		}, reinterpret_cast<void *>(i));

		auto *relay = board.GetRelay(i);
		relay->OnFault(+[](void *args) {
			size_t channel = reinterpret_cast<size_t>(args);
			auto &board = Board::GetInstance();
			auto *charger = board.GetCharger(channel);
			charger->SendFault(fault_bitmask::RELAY_STEAKED);
		}, reinterpret_cast<void *>(i));
	}

	auto *meter = board.GetMeter();
	meter->OnFault(+[](void *args) {
		auto &board = Board::GetInstance();
		for (size_t i = 0; i < board.GetNumChargers(); ++i) {
			auto *charger = board.GetCharger(i);
			charger->SendFault(fault_bitmask::METER_FAULT);
		}
	}, nullptr);
}

void Application::Start() {
	WireModules();

	xTaskCreate(+[](void *args) {
		static_cast<Application *>(args)->MainEventLoop();
	}, nullptr, 128, this, tskIDLE_PRIORITY + 2, nullptr);

	auto &board = Board::GetInstance();
	for (size_t i = 0; i < board.GetNumChargers(); ++i) {
		auto *charger = board.GetCharger(i);
		charger->Start();
	}
	auto *meter = board.GetMeter();
	meter->Start();

	if (board.GetChargerType() == Board::IC_AND_4G || board.GetChargerType() == Board::ONLINE_4G_ONLY) {
		// auto *network = board.GetNetwork();
		// network->Start();
	}

	if (board.GetChargerType() == Board::IC_AND_4G || board.GetChargerType() == Board::OFFLINE_IC) {
		for (size_t i = 0; i < board.GetNumChargers(); ++i) {
			// auto *rfids = board.GetRfids(i);
			// rfids->Start();
		}
	}
}

void Application::Schedule(custom::function<void(void *)> callback, void *args) {
	QueueItem item{SCHEDULE, {}};
	auto *item_data     = reinterpret_cast<ScheduleEventData *>(item.data);
	item_data->callback = callback;
	item_data->args     = args;

	if (xPortIsInsideInterrupt()) {
		if (xQueueSendFromISR(event_queue_, &item, nullptr) != pdTRUE) {
			LOGE("Application: failed to schedule event from ISR");
		}
	} else {
		if (xQueueSend(event_queue_, &item, pdMS_TO_TICKS(100)) != pdTRUE) {
			LOGE("Application: failed to schedule event");
		}
	}
}

void Application::MainEventLoop() {
	QueueItem item{};
	xTimerStart(timer_, pdMS_TO_TICKS(100));
	while (true) {
		if (xQueueReceive(event_queue_, &item, portMAX_DELAY) != pdTRUE) {
			ProcessEvent(item);
		}
	}
}

void Application::TimerCallback() {
	++seconds_;
	// 仅在 Application 中打印定时器任务栈情况
	if (seconds_ % 15 == 0) {
		SystemInfo::PrintStackStats("System timer");
	}

	QueueItem item{TIME, {}};
	if (xQueueSend(event_queue_, &item, 0) != pdTRUE) {
		LOGE("Application: failed to send timer event");
	}
}

void Application::ProcessEvent(const QueueItem &item) {
	static_assert(sizeof(ScheduleEventData) <= sizeof(QueueItem::data), "ScheduleEventData too large for QueueItem");

	switch (item.id) {
		case SCHEDULE: {
			auto *data = reinterpret_cast<const ScheduleEventData *>(item.data);
			data->callback(data->args);
			break;
		}
		case TIME: {
			if (seconds_ % 15 == 0) {
				SystemInfo::PrintHeapStats();
			}
			break;
		}
	}

}

extern "C" void app_main() {
	Application::GetInstance().Start();
}

