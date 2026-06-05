#pragma once

#include <stdint.h>
#include <algorithm>

#include <functional/functional>

class CpDetector {
public:
	enum CpState : uint8_t {
		GROUNDING = 0,  // 接地状态（危险！）
		PLUG_OUT,       // 未插枪
		PLUG_IN,        // 已插枪但S2未接通
		READY,          // 已插枪且S2接通，等待授权/远程启动
	};

	virtual ~CpDetector() = default;

	// 查询当前状态
	CpState GetState() const { return state_; }

	// 设置状态（由底层检测逻辑调用）
	// @param enabled 是否启用检测（某些场景如充电中可能需要暂时禁用）
	void Enabled(bool enabled) { enabled_ = enabled; }

	// 状态变化回调（old_state → new_state）
	void OnStateChanged(custom::function<void(void *, CpState, CpState)> cb, void *args) {
		state_changed_cb_ = {cb, args};
	}

protected:
	CpState state_ = {};
	bool    enabled_ = true;
	std::pair<custom::function<void(void *, CpState, CpState)>, void *> state_changed_cb_ = {};
};
