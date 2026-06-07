#pragma once

#include <utility>

#include <functional/functional> 

class Rfids {
public:
	virtual ~Rfids() = default;

	// 注册任务，用于检测事件做出判断并回调，回调通过以下函数注册
	virtual void Start() = 0;

	void OnVerifySucceed(custom::function<void(void *, size_t, const uint8_t *, size_t)> callback, void *args) {
		verify_succeed_cb_ = {callback, args};
	}

	void OnFault(custom::function<void(void *)> callback, void *args) {
		fault_cb_ = {callback, args};
	}

protected:
	std::pair<custom::function<void(void *, size_t, const uint8_t *, size_t)>, void *> verify_succeed_cb_ = {};
	std::pair<custom::function<void(void *)>, void *> fault_cb_ = {};
};
