#include "system_info.hpp"

#include "FreeRTOS.h"
#include "task.h"

#include <log/log>

void SystemInfo::PrintHeapStats() {
	size_t free_heap = xPortGetFreeHeapSize();
	size_t min_free_heap = xPortGetMinimumEverFreeHeapSize();
	LOGI("Heap Stats - Free Heap: %u bytes, Minimum Ever Free Heap: %u bytes\r\n", free_heap, min_free_heap);
}

void SystemInfo::PrintStackStats(const char *tag) {
#if (INCLUDE_uxTaskGetStackHighWaterMark == 1)
	// 获取当前任务句柄
	TaskHandle_t current_task = xTaskGetCurrentTaskHandle();
	if (current_task == nullptr) {
		LOGE("Failed to get current task handle\r\n");
		return;
	}

	// 获取当前任务的栈剩余空间
	UBaseType_t stack_high_water_mark = uxTaskGetStackHighWaterMark(current_task);
	LOGI("Stack Stats - Current Task: %s, Stack High Water Mark: %u words\r\n", tag, stack_high_water_mark);
#endif
}
