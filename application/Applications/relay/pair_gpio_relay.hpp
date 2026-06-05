#pragma once

#include "main.h"
#include "FreeRTOS.h"
#include "timers.h"

#include "relay.hpp"

class PairGpioRelay : public Relay {
public:
	PairGpioRelay(GPIO_TypeDef *n_drive_port, uint16_t n_drive_pin,
	              GPIO_TypeDef *n_detect_port, uint16_t n_detect_pin,
	              GPIO_TypeDef *l_drive_port, uint16_t l_drive_pin,
	              GPIO_TypeDef *l_detect_port, uint16_t l_detect_pin);
	~PairGpioRelay();
	
	void DetectEnabled(bool enabled) override;
	void SetState(bool on) override;

private:
	GPIO_TypeDef *n_drive_port_;
	uint16_t      n_drive_pin_;
	GPIO_TypeDef *n_detect_port_;
	uint16_t      n_detect_pin_;
	GPIO_TypeDef *l_drive_port_;
	uint16_t      l_drive_pin_;
	GPIO_TypeDef *l_detect_port_;
	uint16_t      l_detect_pin_;
	TimerHandle_t timer_;
	bool          state_ = false; // 当前状态（true=闭合/充电，false=断开/停止）

	PairGpioRelay(const PairGpioRelay &) = delete;
	PairGpioRelay &operator=(const PairGpioRelay &) = delete;

	void Detect();
};
