#include "ws2812b_led.hpp"

Ws2812bLed::Ws2812bLed(GPIO_TypeDef *port, uint16_t pin, size_t num_leds)
		: port_(port)
		, pin_(pin)
		, num_leds_(num_leds) {
	
	configASSERT(port_);
	port_->BSRR = pin_ << 16U; // 先拉低引脚
	GPIO_InitTypeDef init{};
	init.Pin   = pin_;
	init.Mode  = GPIO_MODE_OUTPUT_PP;
	init.Pull  = GPIO_NOPULL;
	init.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(port_, &init);

	// 初始化 GPIO 和定时器
	timer_ = xTimerCreate(nullptr, pdMS_TO_TICKS(1000), pdTRUE, this, +[](TimerHandle_t timer) {
		auto *self = static_cast<Ws2812bLed *>(pvTimerGetTimerID(timer));
		self->TimerCallback();
	});
	configASSERT(timer_);
}

Ws2812bLed::~Ws2812bLed() {
	xTimerDelete(timer_, 0);
	HAL_GPIO_DeInit(port_, pin_);
}

void Ws2812bLed::OnStateChanged(Charger::State new_state) {
}

void Ws2812bLed::TimerCallback() {
	// 定时器回调逻辑
}


