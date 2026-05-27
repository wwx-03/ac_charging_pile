#pragma once

#include "FreeRTOS.h"
#include "queue.h"

#include <functional/functional>

class Application {
public:

	static Application& GetInstance() {
		static Application instance;
		return instance;
	}

	void Start();
	void Schedule(custom::function<void(void *)> callback, void *args, bool from_isr = false);

private:

	QueueHandle_t event_queue_;

	Application();
	Application(const Application&) = delete;
	Application& operator=(const Application&) = delete;

	void MainEventLoop();
};
