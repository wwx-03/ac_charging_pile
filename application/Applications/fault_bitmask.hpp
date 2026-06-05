// 定义全局故障掩码
#pragma once

#include <stdint.h>

namespace fault_bitmask {

	static constexpr uint32_t EMERGENCY_STOP  = (1U << 0); // 急停
	static constexpr uint32_t GROUND_FAULT    = (1U << 1); // 接地故障
	static constexpr uint32_t LEAKAGE_FAULT   = (1U << 2); // 漏电故障
	static constexpr uint32_t RELAY_STEAKED   = (1U << 3); // 继电器粘连
	static constexpr uint32_t OVERCURRENT     = (1U << 4); // 过流
	static constexpr uint32_t NETWORK_TIMEOUT = (1U << 5); // 网络超时
	static constexpr uint32_t METER_FAULT     = (1U << 6); // 电表故障
	static constexpr uint32_t OVERTEMPERATURE = (1U << 7); // 过温
	
}
