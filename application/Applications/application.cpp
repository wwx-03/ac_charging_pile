#include "application.hpp"

#include <log/log>

#include "board/board.hpp"
#include "charger/charger.hpp"
#include "network/network.hpp"
#include "protocol/ops_protocol.hpp"
#include "protocol/mon_protocol.hpp"
#include "billing/charging_session.hpp"

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
	static_assert(sizeof(ScheduleEventData) <= sizeof(EventQueueItem::data),
				  "ScheduleEventData too large for EventQueueItem");

} // namespace

Application::Application() {
	event_queue_ = xQueueCreate(3, sizeof(EventQueueItem));
	configASSERT(event_queue_);
}

// ============================================================
// 模块接线
// ============================================================
static void WireModules() {
	auto &board = Board::GetInstance();

	// 1. 从 Flash 加载配置（地址、电价等）
	board.LoadConfig();

	// 2. 恢复断电前未完成的计费会话
	if (board.GetBillingEngine()->RecoverSession()) {
		LOGW("App: recovered interrupted billing session from Flash");
	}

	// 3. 监听网络事件 → 路由到对应协议
	board.GetNetwork()->RegisterEventHandler(
		[](void *args, Network::EventId id, void *data) {
			auto &board_ = *static_cast<Board *>(args);
			if (id == Network::CONNECTED) {
				auto *ev = static_cast<Network::ConnectedEventData *>(data);
				if (ev->fd == config::OPS_FD) {
					board_.GetOpsProtocol()->Start(ev->fd);
				} else if (ev->fd == config::MON_FD) {
					board_.GetMonProtocol()->Start(ev->fd);
				}
			} else if (id == Network::DATA_RECEIVED) {
				auto *ev = static_cast<Network::DataReceivedEventData *>(data);
				if (ev->fd == config::OPS_FD) {
					board_.GetOpsProtocol()->OnDataReceived(ev->data, ev->length);
				} else if (ev->fd == config::MON_FD) {
					board_.GetMonProtocol()->OnDataReceived(ev->data, ev->length);
				}
			}
		},
		&board
	);

	// 4. 监听运营协议事件 → 控制充电桩
	board.GetOpsProtocol()->RegisterEventHandler(
		[](void *args, Protocol::EventId id, void *data) {
			auto &board_ = *static_cast<Board *>(args);
			auto *charger = board_.GetCharger();
			if (id == Protocol::OPS_REMOTE_START_REQUEST) {
				// data 中包含 token/card_id，此处简单直接 Authorize
				charger->Authorize(nullptr, 0);
			} else if (id == Protocol::OPS_REMOTE_STOP_REQUEST) {
				charger->SendEvent(Charger::EV_REMOTE_STOP);
			} else if (id == Protocol::OPS_LOGOUT) {
				LOGI("App: ops platform logged out");
			}
		},
		&board
	);

	// 5. 充电会话结束 → 双平台上报
	board.GetCharger()->OnSessionCompleted(
		[](void *args, const ChargingSession &session) {
			auto &board_ = *static_cast<Board *>(args);
			static_cast<OpsProtocol *>(board_.GetOpsProtocol())->ReportSession(session);
			static_cast<MonProtocol *>(board_.GetMonProtocol())->ReportSession(session);
		},
		&board
	);

	// 6. 充电状态变化 → 监控平台上报
	board.GetCharger()->OnStateChanged(
		[](void *args, Charger::State old_s, Charger::State new_s) {
			auto &board_ = *static_cast<Board *>(args);
			static_cast<MonProtocol *>(board_.GetMonProtocol())
				->ReportDeviceStatus(static_cast<uint8_t>(new_s));
		},
		&board
	);

	// 7. 启动网络（开始 LAR01 初始化 → 拨号 → 双 TCP 连接）
	board.GetNetwork()->Start();
}

void Application::Start() {
	WireModules();

	xTaskCreate(+[](void *args) {
		static_cast<Application *>(args)->MainEventLoop();
	}, nullptr, 256, this, tskIDLE_PRIORITY + 2, nullptr);
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
	while (true) {
		if (xQueueReceive(event_queue_, &item, portMAX_DELAY) != pdTRUE) {
			LOGE("Application: failed to receive event");
			continue;
		}
		if (item.id == SCHEDULE) {
			auto *item_data = reinterpret_cast<ScheduleEventData *>(item.data);
			item_data->callback(item_data->args);
		} else {
			LOGW("Application: unknown event id %d", static_cast<int>(item.id));
		}
	}
}

extern "C" void app_main() {
	Application::GetInstance().Start();
}

