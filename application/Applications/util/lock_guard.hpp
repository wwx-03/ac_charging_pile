#pragma once

#include "FreeRTOS.h"
#include "semphr.h"

template <TickType_t timeout = pdMS_TO_TICKS(100)>
class LockGuard {
public:
	explicit LockGuard(SemaphoreHandle_t mutex) : mutex_(mutex) {
		xSemaphoreTake(mutex_, timeout);
	}

	~LockGuard() {
		xSemaphoreGive(mutex_);
	}
private:
	SemaphoreHandle_t mutex_;
};
