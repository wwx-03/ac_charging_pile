#pragma once

#include "FreeRTOS.h"
#include "queue.h"
#include "timers.h"

#include <functional/functional>

class Application {
public:

	static Application& GetInstance() {
		static Application instance;
		return instance;
	}

	void Start();
	
	// 在主事件循环中调度一个回调（线程/ISR 安全）
	void Schedule(custom::function<void(void *)> callback, void *args = nullptr);

private:
	QueueHandle_t event_queue_;
	TimerHandle_t timer_;

	Application();
	Application(const Application&) = delete;
	Application& operator=(const Application&) = delete;

	void MainEventLoop();
	void TimerCallback();
};
