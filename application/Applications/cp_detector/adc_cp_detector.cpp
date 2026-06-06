#include "adc_cp_detector.hpp"

void AdcCpDetector::UpdateState(uint16_t adc_value) {
	if (!enabled_) {
		return;
	}

	float voltage = adc_value * (12.0f / 3587);

	State new_state = state_;
	
	if (voltage <= CP_GROUNDING_MAX_VOLTAGE) {
		new_state = GROUNDING;
	} else if (CP_READY_MIN_VOLTAGE <= voltage && voltage <= CP_READY_MAX_VOLTAGE) {
		new_state = READY;
	} else if (CP_PLUG_IN_MIN_VOLTAGE <= voltage && voltage <= CP_PLUG_IN_MAX_VOLTAGE) {
		new_state = PLUG_IN;
	} else if (voltage >= CP_PLUG_OUT_MIN_VOLTAGE) {
		new_state = PLUG_OUT;
	}

	// 只有检测到连续相同的新状态达到阈值后，才真正更新 state_。
	if (new_state != pending_state_) {
		pending_state_ = new_state;
		filter_ = 0;
		return;
	}

	if (pending_state_ != state_) {
		if (++filter_ >= FILTER_THRESHOLD) {
			filter_ = 0;

			State old_state = state_;
			state_          = pending_state_;

			auto &[cb, args] = state_changed_cb_;
			cb(args, old_state, state_);
		}
	} else {
		filter_ = 0;
	}
}
