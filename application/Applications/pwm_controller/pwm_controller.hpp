#pragma once

class PwmController {
public:
	virtual ~PwmController() = default;

	// 设置 PWM 输出占空比（0.0 - 100.0）
	virtual void SetDutyCycle(float duty_cycle) = 0;

	// 启动 PWM 输出
	virtual void Start() = 0;

	// 停止 PWM 输出
	virtual void Stop() = 0;
};
