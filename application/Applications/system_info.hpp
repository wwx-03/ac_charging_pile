#pragma once

class SystemInfo {
public:
	// 打印系统内存使用情况（FreeRTOS 堆统计）
	static void PrintHeapStats();

	// 打印系统任务栈使用情况（FreeRTOS 任务统计）
	static void PrintStackStats(const char *tag);
};
