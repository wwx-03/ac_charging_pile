#pragma once

#include <stdint.h>
#include <algorithm>
#include <functional/functional>

#include "billing/charging_session.hpp"

class Charger {
public:

	enum State : uint8_t {
		IDLE = 0,
		CONNECTED,                // 已插枪，等待授权
		READY,                    // 授权通过，等待启动
		WAITING_FOR_CONNECTED,    // 授权通过，等待插枪
		CHARGING,                 // 充电中
		STOPPING,                 // 停止中
		COMPLETE,		  // 充电完成（已停止但未拔枪）
		FAULT,                    // 故障
	};

	enum Event : uint8_t {
		EV_PLUG_IN = 0,         // CP 检测到插枪
		EV_READY,               // CP 检测到枪已连接且准备就绪（如握手成功）
		EV_UNPLUG,              // CP 检测到拔枪
		EV_AUTH_SUCCESS,        // 授权成功（本地或平台确认）
		EV_USER_STOP,           // 用户主动停止（刷卡/APP）
		EV_CHARGE_FULL,         // 电池充满（BMS 信号）
		EV_BALANCE_DONE,        // 余额用完（计费引擎通知）
		EV_FAULT_OCCURRED,      // 故障发生
		EV_FAULT_CLEARED,       // 故障恢复
	};

	virtual ~Charger() = default;

	virtual void Start() = 0; // 启动充电桩（上电自检后调用）

	// 上报故障（可组合多个故障位）
	virtual void SendFault(uint32_t fault_bitmask) = 0; // 上报故障（可组合多个故障位）

	// 清除故障（可组合多个故障位）
	virtual void ClearFault(uint32_t fault_bitmask) = 0; // 清除故障（可组合多个故障位）

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
