#pragma once

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

// 线程安全
template <TickType_t timeout = pdMS_TO_TICKS(100)>
class LockGuard {
public:
	explicit LockGuard(SemaphoreHandle_t mutex) : mutex_(mutex) {
		if (xPortIsInsideInterrupt()) {
			xSemaphoreTakeFromISR(mutex_, nullptr);
		} else {
			xSemaphoreTake(mutex_, timeout);
		}
	}

	~LockGuard() {
		if (xPortIsInsideInterrupt()) {
			xSemaphoreGiveFromISR(mutex_, nullptr);
		} else {
			xSemaphoreGive(mutex_);
		}
	}
private:
	SemaphoreHandle_t mutex_;
};
