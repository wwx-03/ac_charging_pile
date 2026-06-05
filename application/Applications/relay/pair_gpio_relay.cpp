#include "pair_gpio_relay.hpp"

PairGpioRelay::PairGpioRelay(GPIO_TypeDef *n_drive_port, uint16_t n_drive_pin,
			     GPIO_TypeDef *n_detect_port, uint16_t n_detect_pin,
			     GPIO_TypeDef *l_drive_port, uint16_t l_drive_pin,
			     GPIO_TypeDef *l_detect_port, uint16_t l_detect_pin)
		: n_drive_port_(n_drive_port)
		, n_drive_pin_(n_drive_pin)
		, n_detect_port_(n_detect_port)
		, n_detect_pin_(n_detect_pin)
		, l_drive_port_(l_drive_port)
		, l_drive_pin_(l_drive_pin)
		, l_detect_port_(l_detect_port)
		, l_detect_pin_(l_detect_pin) {
	configASSERT(n_drive_port_);
	configASSERT(n_drive_pin_);
	configASSERT(n_detect_port_);
	configASSERT(n_detect_pin_);
	configASSERT(l_drive_port_);
	configASSERT(l_drive_pin_);
	configASSERT(l_detect_port_);
	configASSERT(l_detect_pin_);

	n_drive_port_->BSRR = n_drive_pin_ << 16U; // Reset pin to low level
	l_drive_port_->BSRR = l_drive_pin_ << 16U; // Reset pin to low level
	GPIO_InitTypeDef init{};
	init.Pin = n_drive_pin_;
	init.Mode = GPIO_MODE_OUTPUT_PP;
	init.Pull = GPIO_NOPULL;
	init.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(n_drive_port_, &init);
	init.Pin = l_drive_pin_;
	HAL_GPIO_Init(l_drive_port_, &init);

	init.Pin = n_detect_pin_;
	init.Mode = GPIO_MODE_INPUT;
	HAL_GPIO_Init(n_detect_port_, &init);
	init.Pin = l_detect_pin_;
	HAL_GPIO_Init(l_detect_port_, &init);

	timer_ = xTimerCreate(nullptr, pdMS_TO_TICKS(100), pdFALSE, this, +[](TimerHandle_t timer) {
		auto *relay = static_cast<PairGpioRelay *>(pvTimerGetTimerID(timer));
		relay->Detect();
	});
	configASSERT(timer_);
}

PairGpioRelay::~PairGpioRelay() {
	n_drive_port_->BSRR = n_drive_pin_ << 16U; // Reset pin to low level
	l_drive_port_->BSRR = l_drive_pin_ << 16U; // Reset pin to low level
	HAL_GPIO_DeInit(n_drive_port_, n_drive_pin_);
	HAL_GPIO_DeInit(n_detect_port_, n_detect_pin_);
	HAL_GPIO_DeInit(l_drive_port_, l_drive_pin_);
	HAL_GPIO_DeInit(l_detect_port_, l_detect_pin_);
	xTimerDelete(timer_, pdMS_TO_TICKS(100));
}

void PairGpioRelay::DetectEnabled(bool enabled) {
	if (enabled) {
		xTimerStart(timer_, pdMS_TO_TICKS(100));
	} else {
		xTimerStop(timer_, pdMS_TO_TICKS(100));
	}
	enabled_ = enabled;
}

void PairGpioRelay::SetState(bool on) {
	state_ = on;
	n_drive_port_->BSRR = on ? n_drive_pin_ : (n_drive_pin_ << 16U);
	l_drive_port_->BSRR = on ? l_drive_pin_ : (l_drive_pin_ << 16U);
}

void PairGpioRelay::Detect() {
	if (!enabled_) return;

	bool n_detected = (n_detect_port_->IDR & n_detect_pin_) != 0;
	bool l_detected = (l_detect_port_->IDR & l_detect_pin_) != 0;
	if (n_detected != state_ || l_detected != state_) {
		// 状态异常，触发回调
		auto &[cb, args] = fault_cb_;
		if (cb) {
			cb(args);
		}
	}
}
