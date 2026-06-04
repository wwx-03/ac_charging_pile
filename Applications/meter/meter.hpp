#pragma once

#include <stddef.h>
#include <algorithm>
#include <functional/functional>

class Meter {
public:
	virtual ~Meter() = default;
	virtual void Start() = 0;
	virtual float GetVoltage() = 0;
	virtual float GetCurrent(size_t channel) = 0;
	virtual float GetPower(size_t channel) = 0;
	virtual float GetEnergy(size_t channel) = 0;

	// 注册故障回调函数，回调函数参数为一个void*指针，用户可以通过这个指针传递任意类型的数据
	void OnFault(custom::function<void (void *)> cb, void *user_data) {
		fault_cb_ = {cb, user_data};
	}

	// 注册故障清除回调函数，回调函数参数同样为一个void*指针
	void OnFaultCleared(custom::function<void (void *)> cb, void *user_data) {
		fault_cleared_cb_ = {cb, user_data};
	}

protected:
	std::pair<custom::function<void (void *)>, void *> fault_cb_;	
	std::pair<custom::function<void (void *)>, void *> fault_cleared_cb_;
};
