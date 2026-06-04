#pragma once

#include <stdint.h>
#include <algorithm>
#include <functional/functional>

#include "billing/charging_session.hpp"

class Charger {
public:

	enum State : uint8_t {
		IDLE = 0,
		PLUGGED_IN,         // 已插枪，等待授权
		AUTHORIZING,        // 授权中（刷卡/平台远程）
		WAITING_FOR_READY,  // 授权通过，等待平台下发启动指令
		CHARGING,           // 充电中
		STOPPING,           // 停止中（等待继电器断开）
		FAULT,              // 故障
	};

	enum Event : uint8_t {
		EV_PLUG_IN       = 0,   // CP 检测到插枪
		EV_UNPLUG,              // CP 检测到拔枪
		EV_AUTH_SUCCESS,        // 授权成功（本地或平台确认）
		EV_AUTH_FAIL,           // 授权失败
		EV_REMOTE_START,        // 平台下发远程启动
		EV_REMOTE_STOP,         // 平台下发远程停止
		EV_USER_STOP,           // 用户主动停止（刷卡/按键）
		EV_CHARGE_FULL,         // 电池充满（BMS 信号）
		EV_BALANCE_DONE,        // 余额用完（计费引擎通知）
		EV_FAULT_OCCURRED,      // 故障发生
		EV_FAULT_CLEARED,       // 故障恢复
		EV_OVERCURRENT,         // 过流保护
	};

	virtual ~Charger() = default;

	// 投递充电事件（线程/ISR 安全）
	virtual void SendEvent(Event event) = 0;

	// 设置授权信息（即插即充时调用）
	virtual void Authorize() = 0;

	// 设置授权信息（刷卡时由调用）
	virtual void Authorize(const uint8_t *card_id, size_t len) = 0;

	// 设置授权信息 (远程启动时由协议层调用，transaction_sn 可用于关联订单，但本地不强制要求使用)
	virtual void Authorize(const uint8_t *transaction_sn, size_t sn_len,
			const uint8_t *logic_card_id, size_t logic_card_id_len,
			const uint8_t *physic_card_id, size_t physic_card_id_len,
			float balance) = 0;

	// 查询当前状态
	virtual State GetState() const = 0;

	// 状态变化回调（old_state → new_state）
	void OnStateChanged(custom::function<void(void *, State, State)> cb, void *args) {
		state_changed_cb_ = {cb, args};
	}

	// 会话结束回调，携带完整计费记录（由协议层注册，上报云端）
	void OnSessionCompleted(custom::function<void(void *, const ChargingSession &)> cb, void *args) {
		session_completed_cb_ = {cb, args};
	}

protected:
	std::pair<custom::function<void(void *, State, State)>, void *>               state_changed_cb_;
	std::pair<custom::function<void(void *, const ChargingSession &)>, void *>    session_completed_cb_;
};
