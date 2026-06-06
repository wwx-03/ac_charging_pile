#include "tim_pwm_controller.hpp"

#include "FreeRTOS.h"
#include "task.h"

TimPwmController::TimPwmController(TIM_HandleTypeDef *htim, uint32_t channel)
		: htim_(htim)
		, channel_(channel) {
	configASSERT(htim_);
}

void TimPwmController::SetDutyCycle(float duty_cycle) {
	if (duty_cycle < 0.0f) {
		duty_cycle = 0.0f;
	} else if (duty_cycle > 100.0f) {
		duty_cycle = 100.0f;
	}
	if (inverted_) {
		duty_cycle = 100.0f - duty_cycle;
	}
	current_duty_cycle_ = duty_cycle;

	uint32_t pulse_length = static_cast<uint32_t>((htim_->Init.Period + 1) * (current_duty_cycle_ / 100.0f));
	__HAL_TIM_SET_COMPARE(htim_, channel_, pulse_length);
}

void TimPwmController::Start() {
	HAL_TIM_PWM_Start(htim_, channel_);
}

void TimPwmController::Stop() {
	HAL_TIM_PWM_Stop(htim_, channel_);
}
