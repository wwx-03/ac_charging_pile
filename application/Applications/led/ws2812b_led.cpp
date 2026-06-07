#include "ws2812b_led.hpp"

Ws2812bLed::Ws2812bLed(GPIO_TypeDef *port, uint16_t pin, size_t num_leds)
		: port_(port)
		, pin_(pin)
		, num_leds_(num_leds) {
	
	// 初始化 GPIO 和定时器
	configASSERT(port_);
	port_->BSRR = pin_ << 16U; // 先拉低引脚
	GPIO_InitTypeDef init{};
	init.Pin   = pin_;
	init.Mode  = GPIO_MODE_OUTPUT_PP;
	init.Pull  = GPIO_NOPULL;
	init.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(port_, &init);

	timer_ = xTimerCreate(nullptr, pdMS_TO_TICKS(1000), pdTRUE, this, +[](TimerHandle_t timer) {
		auto *self = static_cast<Ws2812bLed *>(pvTimerGetTimerID(timer));
		self->Update();
	});
	configASSERT(timer_);

	led_buffer_ = new uint8_t[num_leds_ * 3];
	configASSERT(led_buffer_);

	xTimerStart(timer_, 0);
}

Ws2812bLed::~Ws2812bLed() {
	delete[] led_buffer_;
	xTimerDelete(timer_, 0);
	HAL_GPIO_DeInit(port_, pin_);
}

void Ws2812bLed::OnStateChanged(Charger::State new_state) {
	switch (new_state) {
		case Charger::State::IDLE:
			SetLightingEffect(0, 0, 255); // 蓝色
			break;
		case Charger::State::CONNECTED:
			SetLightingEffect(0, 255, 0); // 绿色
			break;
		case Charger::State::READY:
			SetLightingEffect(0, 255, 0); // 绿色
			break;
		case Charger::State::WAITING_FOR_CONNECTED:
			SetBlinkingEffect(0, 0, 255, 250); // 蓝色闪烁
			break;
		case Charger::State::CHARGING:
			SetFlowingEffect(0, 255, 0, num_leds_ / 3, 250);
			break;
		case Charger::State::STOPPING:
			SetFlowingEffect(0, 0, 255, num_leds_ / 3, 250);
			break;
		case Charger::State::COMPLETE:
			SetBreathingEffect(0, 255, 0, 0.1f, 50); // 绿色呼吸
			break;
		case Charger::State::FAULT:
			SetLightingEffect(255, 0, 0); // 红色
			break;
	}
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

void Ws2812bLed::Update() {
	uint8_t begin = 0, end = 0;
	bool state = false;
	uint8_t inc_r = 0, inc_g = 0, inc_b = 0;
	uint8_t current_r = 0, current_g = 0, current_b = 0;
	// 更新 LED 状态（如动画效果）
	switch (effect_state_.effect) {
		case NONE:
			ClearLedBuffer();
			break;
		case LIGHTING:
			SetAllLeds(effect_state_.dest_r, effect_state_.dest_g, effect_state_.dest_b);
			break;
		case FLOWING:
			begin = effect_state_.index;
			end = (effect_state_.index + effect_state_.length) % num_leds_;
			ClearLedBuffer();
			for (size_t i = 0; i < effect_state_.length; ++i) {
				size_t idx = (begin + i) % num_leds_;
				SetColor(idx, effect_state_.dest_r, effect_state_.dest_g, effect_state_.dest_b);
			}
			effect_state_.index = (effect_state_.index + 1) % num_leds_; // 让流动效果循环
			break;
		case BLINKING:
			state = led_buffer_[0] == 0; // 通过检查第一个 LED 的状态来切换
			SetAllLeds(state ? effect_state_.dest_r : 0, state ? effect_state_.dest_g : 0, state ? effect_state_.dest_b : 0);
			break;
		case BREATHING:
			inc_r = effect_state_.dest_r * effect_state_.update_percent;
			inc_g = effect_state_.dest_g * effect_state_.update_percent;
			inc_b = effect_state_.dest_b * effect_state_.update_percent;
			current_r = led_buffer_[1];
			current_g = led_buffer_[0];
			current_b = led_buffer_[2];
			SetAllLeds(current_r + inc_r > effect_state_.dest_r ? effect_state_.dest_r : current_r + inc_r,
			           current_g + inc_g > effect_state_.dest_g ? effect_state_.dest_g : current_g + inc_g,
			           current_b + inc_b > effect_state_.dest_b ? effect_state_.dest_b : current_b + inc_b);
			break;
	}

	Refresh();
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

void Ws2812bLed::SetLightingEffect(uint8_t r, uint8_t g, uint8_t b) {
	effect_state_ = {
		.effect = Effect::LIGHTING,
		.dest_r = r,
		.dest_g = g,
		.dest_b = b,
	};
	xTimerChangePeriod(timer_, pdMS_TO_TICKS(1000), 0);
}

void Ws2812bLed::SetFlowingEffect(uint8_t r, uint8_t g, uint8_t b, size_t length, size_t update_ms) {
	effect_state_ = {
		.effect = Effect::FLOWING,
		.index = 0,
		.length = length,
		.dest_r = r,
		.dest_g = g,
		.dest_b = b,
	};
	xTimerChangePeriod(timer_, pdMS_TO_TICKS(update_ms), 0);
}

void Ws2812bLed::SetBlinkingEffect(uint8_t r, uint8_t g, uint8_t b, size_t update_ms) {
	effect_state_ = {
		.effect = Effect::BLINKING,
		.dest_r = r,
		.dest_g = g,
		.dest_b = b,
	};
	xTimerChangePeriod(timer_, pdMS_TO_TICKS(update_ms), 0);
}

void Ws2812bLed::SetBreathingEffect(uint8_t r, uint8_t g, uint8_t b, float update_percent, size_t update_ms) {
	effect_state_ = {
		.effect = Effect::BREATHING,
		.update_percent = update_percent,
		.dest_r = r,
		.dest_g = g,
		.dest_b = b,
	};
	xTimerChangePeriod(timer_, pdMS_TO_TICKS(update_ms), 0);
}
