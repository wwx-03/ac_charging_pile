#include "gpio_relay.hpp"

#include "FreeRTOS.h"
#include "task.h"

GpioRelay::GpioRelay(GPIO_TypeDef *drive_port, uint16_t drive_pin,
		     GPIO_TypeDef *detect_port, uint16_t detect_pin)
		: drive_port_(drive_port)
		, drive_pin_(drive_pin)
		, detect_port_(detect_port)
		, detect_pin_(detect_pin) {
	
	drive_port_->BSRR = drive_pin_ << 16U; // Reset pin to low level
	GPIO_InitTypeDef init{};
	init.Pin = drive_pin_;
	init.Mode = GPIO_MODE_OUTPUT_PP;
	init.Pull = GPIO_NOPULL;
	init.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(drive_port_, &init);

	init.Pin = detect_pin_;
	init.Mode = GPIO_MODE_INPUT;
	init.Pull = GPIO_NOPULL;
	init.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(detect_port_, &init);

	timer_ = xTimerCreate(nullptr, pdMS_TO_TICKS(100), pdTRUE, this, [](TimerHandle_t xTimer) {
		auto *self = static_cast<GpioRelay *>(pvTimerGetTimerID(xTimer));
		self->Detect();
	});
	configASSERT(timer_);

	xTimerStart(timer_, pdMS_TO_TICKS(100));
}

GpioRelay::~GpioRelay() {
	drive_port_->BSRR = drive_pin_ << 16U; // Reset pin to low level
	HAL_GPIO_DeInit(drive_port_, drive_pin_);
	HAL_GPIO_DeInit(detect_port_, detect_pin_);
	xTimerDelete(timer_, pdMS_TO_TICKS(100));
}

void GpioRelay::DetectEnabled(bool enabled) {
	if (enabled) {
		xTimerStart(timer_, pdMS_TO_TICKS(100));
	} else {
		xTimerStop(timer_, pdMS_TO_TICKS(100));
	}
	enabled_ = enabled;
}

void GpioRelay::SetState(bool on) {
	state_ = on;
	drive_port_->BSRR = on ? drive_pin_ : (drive_pin_ << 16U);
}

// 私有方法

void GpioRelay::Detect() {
	if (!enabled_) return;

	bool detected = (detect_port_->IDR & detect_pin_) != 0;
	if (detected != state_) {
		// 状态异常，触发回调
		auto &[cb, args] = fault_cb_;
		cb(args);
	}
}
