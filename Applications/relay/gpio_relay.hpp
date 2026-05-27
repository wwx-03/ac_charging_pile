#pragma once

#include "main.h"

#include "relay.hpp"

class GpioRelay : public Relay {
public:
	GpioRelay(GPIO_TypeDef *port, uint16_t pin);
	~GpioRelay();
	void SetState(bool on) override;
private:
	GPIO_TypeDef *port_;
	uint16_t pin_;
};
