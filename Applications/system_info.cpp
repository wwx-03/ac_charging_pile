#include "system_info.hpp"

#include "FreeRTOS.h"

#include <log/log>

void SystemInfo::PrintHeapStats() {
	size_t free_heap = xPortGetFreeHeapSize();
	size_t min_free_heap = xPortGetMinimumEverFreeHeapSize();
	LOGI("Heap Stats - Free Heap: %u bytes, Minimum Ever Free Heap: %u bytes\n", free_heap, min_free_heap);
}
