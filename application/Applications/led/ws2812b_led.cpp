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
	refresh_timer_ = xTimerCreate(nullptr, pdMS_TO_TICKS(1000), pdTRUE, this, +[](TimerHandle_t timer) {
		auto *self = static_cast<Ws2812bLed *>(pvTimerGetTimerID(timer));
		self->TimerCallback();
	});
	configASSERT(refresh_timer_);

	update_timer_ = xTimerCreate(nullptr, pdMS_TO_TICKS(1000), pdTRUE, this, +[](TimerHandle_t timer) {
		auto *self = static_cast<Ws2812bLed *>(pvTimerGetTimerID(timer));
		self->TimerCallback();
	});
	configASSERT(update_timer_);

	led_buffer_ = new uint8_t[num_leds_ * 3];
	configASSERT(led_buffer_);
}

Ws2812bLed::~Ws2812bLed() {
	delete[] led_buffer_;
	xTimerDelete(refresh_timer_, 0);
	xTimerDelete(update_timer_, 0);
	HAL_GPIO_DeInit(port_, pin_);
}

void Ws2812bLed::OnStateChanged(Charger::State new_state) {
	switch (new_state) {
		case Charger::State::IDLE:
			SetAllLeds(0, 0, 255); // 蓝色
			break;
		case Charger::State::CONNECTED:
			SetAllLeds(0, 255, 0); // 绿色
			break;
		case Charger::State::READY:
			SetAllLeds(0, 255, 0); // 绿色
			break;
	}
}

void Ws2812bLed::TimerCallback() {
	// 定时器回调逻辑
}

void Ws2812bLed::Refresh() {
	taskENTER_CRITICAL();
	for (uint8_t i = 0; i < num_leds_; ++i) {
		WriteByte(led_buffer_[i * 3 + 0]); // G
		WriteByte(led_buffer_[i * 3 + 1]); // R
		WriteByte(led_buffer_[i * 3 + 2]); // B
	}
	SetIO(false); // 发送完成后拉低引脚
	taskEXIT_CRITICAL();
	Delay(100); // 发送完成后至少等待50us
}

void Ws2812bLed::SetColor(size_t index, uint8_t r, uint8_t g, uint8_t b) {
	if (index >= num_leds_) return;
	led_buffer_[index * 3 + 0] = g; // WS2812B 的颜色顺序是 GRB
	led_buffer_[index * 3 + 1] = r;
	led_buffer_[index * 3 + 2] = b;
}

void Ws2812bLed::SetAllLeds(uint8_t r, uint8_t g, uint8_t b) {
	for (size_t i = 0; i < num_leds_; ++i) {
		SetColor(i, r, g, b);
	}
}
