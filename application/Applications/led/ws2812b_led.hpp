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
	TimerHandle_t refresh_timer_;
	TimerHandle_t update_timer_;

	uint8_t *led_buffer_; 

	__attribute__((__always_inline__)) void Delay(uint32_t count) {
		while (count--) {
			__asm volatile("nop");
		}
	}

	__attribute__((__always_inline__)) void SetIO(bool state) { 
		port_->BSRR = state ? pin_ : (pin_ << 16U); 
	}

	__attribute__((__always_inline__)) void WriteBit(bool bit) {
		SetIO(true);
		Delay(bit ? 8 : 2);
		SetIO(false);
		Delay(8);
	}

	__attribute__((__always_inline__)) void WriteByte(uint8_t byte) {
		for (uint8_t i = 0; i < 8; ++i) {
			WriteBit(byte & (0x80 >> i));
		}
	}

	__attribute__((__always_inline__)) void ClearLedBuffer() {
		memset(led_buffer_, 0, num_leds_ * 3);
	}

	void TimerCallback();
	void Refresh(); // Ë¢ÐÂ LED ÏÔÊ¾
	void SetColor(size_t index, uint8_t r, uint8_t g, uint8_t b);
	void SetAllLeds(uint8_t r, uint8_t g, uint8_t b);
};
