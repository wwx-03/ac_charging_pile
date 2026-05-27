#pragma once

#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include <algorithm>
#include <functional/functional>

class Network {
public:

	enum EventId : uint8_t {
		CONNECTED = 0,
		DISCONNECTED,
		SIM_GETTED,
		TIME_UPDATED,
		SIGNAL_UPDATED,
		DATA_RECEIVED,
		MAX_EVENT_ID,
	};

	struct ConnectedEventData {
		int fd;
	};

	struct SimGettedEventData {
		const uint8_t *sim;
		size_t size;
	};

	struct TimeUpdatedEventData {
		tm timeinfo;
	};

	struct SignalUpdatedEventData {
		int signal_strength;
	};

	struct DataReceivedEventData {
		int fd;
		const uint8_t* data;
		size_t length;
	};

	virtual ~Network() = default;
	virtual void Start() = 0;
	virtual void OnMessageReceived(const uint8_t *data, size_t length) = 0;
	virtual void SendData(int fd, const uint8_t *data, size_t length) = 0;
	void RegisterEventHandler(custom::function<void(void *, EventId, void *)> eventHandler, void *args) {
		event_handler_ = {eventHandler, args};
	}

protected:
	std::pair<custom::function<void(void *, EventId, void *)>, void *> event_handler_;
};
