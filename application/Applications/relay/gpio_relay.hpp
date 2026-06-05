#pragma once

#include "main.h"

#include "FreeRTOS.h"
#include "timers.h"

#include "relay.hpp"

class GpioRelay : public Relay {
public:
	explicit GpioRelay(GPIO_TypeDef *drive_port, uint16_t drive_pin,
	                   GPIO_TypeDef *detect_port, uint16_t detect_pin);
	~GpioRelay();
	void DetectEnabled(bool enabled) override;
	void SetState(bool on) override;
private:
	GPIO_TypeDef *drive_port_;
	uint16_t      drive_pin_;
	GPIO_TypeDef *detect_port_;
	uint16_t      detect_pin_;
	TimerHandle_t timer_;

	bool state_ = false; // µ±Ç°×´Ì¬£¨true=±ÕºÏ/³äµç£¬false=¶Ï¿ª/Í£Ö¹£©

	// É¾³ý¿½±´¹¹Ôì
	GpioRelay(const GpioRelay &) = delete;
	GpioRelay &operator=(const GpioRelay &) = delete;

	void Detect();
};
