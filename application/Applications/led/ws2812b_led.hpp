#pragma once

#include <string.h>

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
	enum Effect : uint8_t {
		NONE = 0,
		LIGHTING,
		FLOWING,
		BLINKING,
		BREATHING,
	};

	struct EffectState {
		Effect  effect         = NONE; // 当前效果类型
		size_t  index          = 0;    // 用于 FLOWING 效果的当前 LED 索引
		size_t  length         = 0;    // 用于 FLOWING 效果的流动长度
		float   update_percent = 0.0f; // 用于 BREATHING 效果的更新百分比
		uint8_t dest_r         = 0;    // 目标颜色（R）
		uint8_t dest_g         = 0;    // 目标颜色（G）
		uint8_t dest_b         = 0;    // 目标颜色（B）
	};

	GPIO_TypeDef *port_;
	uint16_t      pin_;
	size_t        num_leds_;
	TimerHandle_t timer_;

	uint8_t *led_buffer_;

	EffectState effect_state_;

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

	void Refresh(); // 刷新 LED 显示
	void Update();  // 更新 LED 状态（如动画效果）
	void SetColor(size_t index, uint8_t r, uint8_t g, uint8_t b);
	void SetAllLeds(uint8_t r, uint8_t g, uint8_t b);
	void SetLightingEffect(uint8_t r, uint8_t g, uint8_t b);
	void SetFlowingEffect(uint8_t r, uint8_t g, uint8_t b, size_t length, size_t update_ms);
	void SetBlinkingEffect(uint8_t r, uint8_t g, uint8_t b, size_t update_ms);
	void SetBreathingEffect(uint8_t r, uint8_t g, uint8_t b, float update_percent, size_t update_ms);
};
