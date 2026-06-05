#pragma once

#include "cp_detector.hpp"

class AdcCpDetector : public CpDetector {
public:
	~AdcCpDetector() = default;

	// 更新状态，在 ADC 采样完成后调用，参数为 ADC 原始数值（0-4095）
	void UpdateState(uint16_t adc_value);

private:
	static constexpr inline float CP_GROUNDING_MAX_VOLTAGE = 3.0f;
	static constexpr inline float CP_READY_MIN_VOLTAGE     = 5.0f;
	static constexpr inline float CP_READY_MAX_VOLTAGE     = 6.8f;
	static constexpr inline float CP_PLUG_IN_MIN_VOLTAGE   = 7.0f;
	static constexpr inline float CP_PLUG_IN_MAX_VOLTAGE   = 9.8f;
	static constexpr inline float CP_PLUG_OUT_MIN_VOLTAGE  = 10.0f;
};
