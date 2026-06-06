#pragma once

#include "main.h"

#include "FreeRTOS.h"
#include "timers.h"

#include "led/led.hpp"

class Ws2812bLed : public Led {
public:
	explicit Ws2812bLed(GPIO_TypeDef *port, uint16_t pin, size_t num_leds);
	~Ws2812bLed();
	Ws2812bLed(const Ws2812bLed&) = delete;
	Ws2812bLed& operator=(const Ws2812bLed&) = delete;

	void OnStateChanged(Charger::State new_state) override;

private:
	GPIO_TypeDef *port_;
	uint16_t      pin_;
	size_t        num_leds_;
	TimerHandle_t timer_;

	void TimerCallback();
};
