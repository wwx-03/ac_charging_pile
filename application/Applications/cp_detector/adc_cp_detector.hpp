#pragma once

#include "cp_detector.hpp"

class AdcCpDetector : public CpDetector {
public:
	~AdcCpDetector() = default;

	// 更新状态，在 ADC 采样完成后调用，参数为 ADC 原始数值（0-4095）
	void UpdateState(uint16_t adc_value);

private:
	static constexpr inline float CP_GROUNDING_MAX_VOLTAGE = 4.0f; // 4V 以下认为是接地状态（危险！）
	static constexpr inline float CP_READY_MIN_VOLTAGE     = 5.0f;
	static constexpr inline float CP_READY_MAX_VOLTAGE     = 7.0f;
	static constexpr inline float CP_PLUG_IN_MIN_VOLTAGE   = 8.0f;
	static constexpr inline float CP_PLUG_IN_MAX_VOLTAGE   = 10.0f;
	static constexpr inline float CP_PLUG_OUT_MIN_VOLTAGE  = 11.0f;

	static constexpr inline uint8_t FILTER_THRESHOLD = 10; // 需要连续10次相同状态才认为状态改变，防止抖动

	uint8_t filter_ = 0; // 连续命中 pending_state_ 的计数器
	State pending_state_ = GROUNDING;
};
