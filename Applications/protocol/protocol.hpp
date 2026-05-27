#pragma once

#include <stdint.h>
#include <stddef.h>
#include <algorithm>
#include <functional/functional>

#include "network/network.hpp"

class Protocol {
public:

	enum EventId : uint8_t {

		// 运营平台事件ID定义
		OPS_LOGIN = 0,
		OPS_LOGOUT,
		OPS_REMOTE_START_REQUEST,
		OPS_REMOTE_STOP_REQUEST,
		OPS_OTA_REQUEST, 

		// 监控平台事件ID定义
		MON_LOGIN,
		MON_LOGOUT,
		MON_OTA_REQUEST,
	};

	Protocol(Network *network) : network_(network) {}
	virtual ~Protocol() = default;
	virtual void OnDataReceived(const uint8_t* data, size_t length) = 0;
	void Start(int fd) { fd_ = fd; ChildStart(); }
	void RegisterEventHandler(custom::function<void(void *, EventId, void *)> event_handler, void *args) {
		event_handler_ = {event_handler, args};
	}

protected:
	Network *network_;
	int fd_;
	std::pair<custom::function<void(void *, EventId, void *)>, void *> event_handler_;

	virtual void ChildStart() = 0;
};
