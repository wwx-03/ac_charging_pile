#include "adc_cp_detector.hpp"

void AdcCpDetector::UpdateState(uint16_t adc_value) {
	if (!enabled_) {
		return;
	}

	float voltage = adc_value * (12.0f / 4095.0f);

	State new_state;
	
	if (voltage < CP_GROUNDING_MAX_VOLTAGE) {
		new_state = GROUNDING;
	} else if (CP_READY_MIN_VOLTAGE < voltage && voltage < CP_READY_MAX_VOLTAGE) {
		new_state = READY;
	} else if (CP_PLUG_IN_MIN_VOLTAGE < voltage && voltage < CP_PLUG_IN_MAX_VOLTAGE) {
		new_state = PLUG_IN;
	} else if (CP_PLUG_OUT_MIN_VOLTAGE < voltage) {
		new_state = PLUG_OUT;
	}

	if (new_state != state_) {
		State old_state = state_;
		state_          = new_state;

		auto &[cb, args] = state_changed_cb_;
		cb(args, old_state, new_state);
	}
}
