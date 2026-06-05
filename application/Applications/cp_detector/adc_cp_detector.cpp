#include "adc_cp_detector.hpp"

void AdcCpDetector::UpdateState(uint16_t adc_value) {
	if (!enabled_) {
		return;
	}

	float voltage = adc_value * (12.0f / 4095.0f);

	CpState new_state;
	
	if (voltage < CP_GROUNDING_MAX_VOLTAGE) {
		state_ = GROUNDING;
	} else if (CP_READY_MIN_VOLTAGE < voltage && voltage < CP_READY_MAX_VOLTAGE) {
		state_ = READY;
	} else if (CP_PLUG_IN_MIN_VOLTAGE < voltage && voltage < CP_PLUG_IN_MAX_VOLTAGE) {
		state_ = PLUG_IN;
	} else if (CP_PLUG_OUT_MIN_VOLTAGE < voltage) {
		state_ = PLUG_OUT;
	}

	if (new_state != state_) {
		CpState old_state = state_;
		state_            = new_state;

		auto &[cb, args] = state_changed_cb_;
		cb(args, old_state, new_state);
	}
}
