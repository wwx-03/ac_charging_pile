#include "application.hpp"

#include <log/log>

namespace {

	enum EventId {
		SCHEDULE = 0,
	};

	struct EventQueueItem {
		EventId id;
		uint8_t data[16];
	};

	struct ScheduleEventData {
		custom::function<void(void *)> callback;
		void *args;
	};

}

Application::Application() {
	event_queue_ = xQueueCreate(3, sizeof(EventQueueItem));
	configASSERT(event_queue_);
}

void Application::Start() {

	xTaskCreate(+[](void *args) {
		auto *app = static_cast<Application*>(args);
		app->MainEventLoop();
	}, nullptr, 256, this, tskIDLE_PRIORITY + 2, nullptr);

}

void Application::Schedule(custom::function<void(void *)> callback, void *args, bool from_isr) {

	EventQueueItem item{SCHEDULE, {}};
	auto *item_data = reinterpret_cast<ScheduleEventData*>(item.data);
	item_data->callback = callback;
	item_data->args = args;

	if (from_isr) {
		if (xQueueSendFromISR(event_queue_, &item, nullptr) != pdTRUE) {
			LOGE("Application: Failed to schedule event from ISR");
		}
	} else {
		if (xQueueSend(event_queue_, &item, pdMS_TO_TICKS(100)) != pdTRUE) {
			LOGE("Application: Failed to schedule event");
		}
	}
}

void Application::MainEventLoop() {

	EventQueueItem item{};

	while (true) {
		if (xQueueReceive(event_queue_, &item, portMAX_DELAY) != pdTRUE) {
			LOGE("Application: Failed to receive event");
			continue;
		}

		if (item.id == SCHEDULE) {
			auto *item_data = reinterpret_cast<ScheduleEventData*>(item.data);
			item_data->callback(item_data->args);
		} else {
			LOGW("Application: Received unknown event with id %d", item.id);
		}
	}
}

extern "C" void app_main() {
	Application::GetInstance().Start();
}
