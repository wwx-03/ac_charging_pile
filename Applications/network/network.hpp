#pragma once

#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include <algorithm>
#include <functional/functional>

#include "network/tcp.hpp"

class Network {
public:
	virtual ~Network() = default;

	// 连接服务器，异步回调返回结果
	// @param cb    回调函数，参数为 (void *args, int fd, int mode) 其中 mode=0 表示监控平台, mode=1 表示运营平台, mode=2 表示监控兼运营平台
	// @param args  回调函数用户数据指针
	void OnConnected(custom::function<void(void *, int, int)> cb, void *args) {
		connected_cb_ = {cb, args};
	}

	// 断开连接，异步回调返回结果
	// @param cb    回调函数，参数为 (void *args, int fd)
	// @param args  回调函数用户数据指针
	void OnDisconnected(custom::function<void(void *, int)> cb, void *args) {
		disconnected_cb_ = {cb, args};
	}

	// 接收数据，异步回调返回结果
	// @param cb    回调函数，参数为 (void *args, int fd, const uint8_t *data, size_t len)
	// @param args  回调函数用户数据指针
	void OnDataReceived(custom::function<void(void *, int, const uint8_t *, size_t)> cb, void *args) {
		data_received_cb_ = {cb, args};
	}

	// 获取 SIM 卡 ICCID，异步回调返回
	// @param cb    回调函数，参数为 (void *args, const uint8_t *iccid, size_t len)
	// @param args  回调函数用户数据指针
	void OnSimGetted(custom::function<void(void *, const uint8_t *, size_t)> cb, void *args) {
		sim_getted_cb_ = {cb, args};
	}

	// 获取信号强度，异步回调返回
	// @param cb    回调函数，参数为 (void *args, int rssi)
	// @param args  回调函数用户数据指针
	void OnRssiUpdated(custom::function<void(void *, int)> cb, void *args) {
		rssi_updated_cb_ = {cb, args};
	}

	virtual void Start() = 0;
	virtual void SendData(int fd, const uint8_t *data, size_t length) = 0;

protected:
	std::pair<custom::function<void(void *, int, int)>, void *> connected_cb_;
	std::pair<custom::function<void(void *, int)>, void *> disconnected_cb_;
	std::pair<custom::function<void(void *, int, const uint8_t *, size_t)>, void *> data_received_cb_;
	std::pair<custom::function<void(void *, const uint8_t *, size_t)>, void *> sim_getted_cb_;
	std::pair<custom::function<void(void *, int)>, void *> rssi_updated_cb_;
};
