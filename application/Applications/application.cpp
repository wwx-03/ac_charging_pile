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

	enum EventId {
		SCHEDULE = 0,
		TIME,
	};

	struct EventQueueItem {
		EventId id;
		uint8_t data[16];
	};

	struct ScheduleEventData {
		custom::function<void(void *)> callback;
		void *args;
	};
	static_assert(sizeof(ScheduleEventData) <= sizeof(EventQueueItem::data), "ScheduleEventData too large for EventQueueItem");

} // namespace

Application::Application() {
	event_queue_ = xQueueCreate(3, sizeof(EventQueueItem));
	configASSERT(event_queue_);

	timer_ = xTimerCreate(nullptr, pdMS_TO_TICKS(1000), pdTRUE, this, +[](TimerHandle_t timer) {
		auto *self = static_cast<Application *>(pvTimerGetTimerID(timer));
		self->TimerCallback();
	});
	configASSERT(timer_);
}

// ============================================================
// Ä£¿é½ÓÏß
// ============================================================
static void WireModules() {

	auto &board = Board::GetInstance();

	for (size_t i = 0; i < board.GetNumChargers(); ++i) {
		auto *charger = board.GetCharger(i);
		charger->OnStateChanged(+[](void *args, Charger::State old_state, Charger::State new_state) {

		}, nullptr);

		charger->OnSessionCompleted(+[](void *args, const ChargingSession &session) {

		}, nullptr);

		auto *cp = board.GetCpDetector(i);
		cp->OnStateChanged(+[](void *args, CpDetector::State old_state, CpDetector::State new_state) {
			const std::pair<CpDetector::State, Charger::Event> state_event_map[] = {
				{CpDetector::GROUNDING, Charger::EV_FAULT_OCCURRED},
				{CpDetector::PLUG_OUT,          Charger::EV_UNPLUG},
				{CpDetector::PLUG_IN,          Charger::EV_PLUG_IN},
				{CpDetector::READY,              Charger::EV_READY},
			};

			size_t channel = reinterpret_cast<size_t>(args);
			auto *charger = Board::GetInstance().GetCharger(channel);

			if (new_state == CpDetector::GROUNDING) {
				charger->SendFault(fault_bitmask::GROUND_FAULT);
				return;
			} else if (old_state == CpDetector::GROUNDING) {
				charger->ClearFault(fault_bitmask::GROUND_FAULT);
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
			auto *charger = Board::GetInstance().GetCharger(channel);
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
	}, nullptr, 256, this, tskIDLE_PRIORITY + 2, nullptr);

	auto &board = Board::GetInstance();
	for (size_t i = 0; i < board.GetNumChargers(); ++i) {
		auto *charger = board.GetCharger(i);
		charger->Start();
	}
	auto *meter = board.GetMeter();
	meter->Start();
}

void Application::Schedule(custom::function<void(void *)> callback, void *args) {
	EventQueueItem item{SCHEDULE, {}};
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
	EventQueueItem item{};
	xTimerStart(timer_, pdMS_TO_TICKS(100));
	while (true) {
		if (xQueueReceive(event_queue_, &item, portMAX_DELAY) != pdTRUE) {
			LOGE("Application: failed to receive event");
			continue;
		}
		if (item.id == SCHEDULE) {
			auto *item_data = reinterpret_cast<ScheduleEventData *>(item.data);
			item_data->callback(item_data->args);
		} else if (item.id == TIME) {
			static uint8_t cnt = 0;
			if (++cnt % 10 == 0) {
				HAL_IWDG_Refresh(&hiwdg);
				SystemInfo::PrintHeapStats();
			}
		} else {
			LOGW("Application: unknown event id %d", static_cast<int>(item.id));
		}
	}
}

void Application::TimerCallback() {
	EventQueueItem item{TIME, {}};
	if (xQueueSend(event_queue_, &item, 0) != pdTRUE) {
		LOGE("Application: failed to send timer event");
	}
}

extern "C" void app_main() {
	Application::GetInstance().Start();
}

