#pragma once

#include "main.h"
#include "pwm_controller.hpp"

class TimPwmController : public PwmController {
public:
	TimPwmController(TIM_HandleTypeDef *htim, uint32_t channel);
	~TimPwmController() = default;
	TimPwmController(const TimPwmController &) = delete;
	TimPwmController &operator=(const TimPwmController &) = delete;

	void SetDutyCycle(float duty_cycle) override;
	void Start() override;
	void Stop() override;
private:
	TIM_HandleTypeDef *htim_;
	uint32_t channel_;
	float current_duty_cycle_ = 0.0f;
};
