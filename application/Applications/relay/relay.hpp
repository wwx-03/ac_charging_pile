#pragma once

#include <utility>
#include <functional/functional>

class Relay {
public:
	virtual ~Relay() = default;

	// 故障回调（继电器粘黏）
	void OnFault(custom::function<void(void *)> cb, void *args) {
		fault_cb_ = {cb, args};
	}

	// 使能继电器粘黏检测
	virtual void DetectEnabled(bool enabled) { enabled_ = enabled; }

	// 设置继电器状态（true=闭合/充电，false=断开/停止）
	virtual void SetState(bool on) = 0;

protected:
	bool enabled_ = true; 
	std::pair<custom::function<void(void *)>, void *> fault_cb_ = {};
};
