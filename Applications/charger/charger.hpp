#pragma once


#include <stdint.h>
#include <algorithm>
#include <functional/functional>

class Charger {
public:

	enum State : uint8_t {
		IDLE = 0,
		WAITING_FOR_PLUG_IN,
		PLUGGED_IN,
		AUTHORIZING,
		WAITING_FOR_READY,
		CHARGING,
		STOPPED,
		FAULT,
	};

	enum Event : uint8_t {
		EV_PLUG_IN_DETECTED = 0,	// 插枪事件
		EV_UNPLUG_DETECTED,		// 拔枪事件
		EV_AUTH_SUCCESS,		// 授权成功事件
		EV_CHARGE_COMPLETE,		// 充电完成事件
		EV_USER_STOP,			// 用户停止事件
		EV_FAULT_OCCURRED,		// 故障发生事件
		EV_FAULT_CLEARED,		// 故障清除事件
	};

	virtual ~Charger() = default;
	virtual void SendEvent(Event event) = 0;

	void OnStateChanged(custom::function<void(void *, State, State)> callback, void *args) {
		stateChangeCallback_ = {callback, args};
	}

protected:

	std::pair<custom::function<void(void *, State, State)>, void *> stateChangeCallback_;

};
