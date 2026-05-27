#include "gpio_relay.hpp"

#include "FreeRTOS.h"
#include "task.h"

GpioRelay::GpioRelay(GPIO_TypeDef *port, uint16_t pin)
		: port_(port)
		, pin_(pin) {
	
	port_->BSRR = pin_ << 16U; // Reset pin to low level
	GPIO_InitTypeDef init{};
	init.Pin = pin_;
	init.Mode = GPIO_MODE_OUTPUT_PP;
	init.Pull = GPIO_NOPULL;
	init.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(port_, &init);
}

GpioRelay::~GpioRelay() {
	port_->BSRR = pin_ << 16U; // Reset pin to low level
	HAL_GPIO_DeInit(port_, pin_);
}

void GpioRelay::SetState(bool on) {
	port_->BSRR = on ? pin_ : (pin_ << 16U);
}
