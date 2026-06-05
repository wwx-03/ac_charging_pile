#include "hs7kwh_board.hpp"

#include "tim.h"
#include "usart.h"

#include "charger/ac_charger.hpp"
#include "cp_detector/adc_cp_detector.hpp"

#include "relay/pair_gpio_relay.hpp"
#include "pwm_controller/tim_pwm_controller.hpp"

#include "meter/meter_bl0940.hpp"

#include "storage/internal_flash.hpp"

#include "serial/serial_uart.hpp"

namespace {

	static constexpr size_t NUM_CHARGERS = 1;
	static constexpr size_t CHARGER_1 = 0;

	static constexpr size_t NUM_SERIALS = 3;
	static constexpr size_t SERIAL_NETWORK = 0;
	static constexpr size_t SERIAL_METER = 1;
	static constexpr size_t SERIAL_DISPLAY = 2;

}

size_t HS7KwhBoard::GetNumChargers() const {
	return NUM_CHARGERS;
}

Charger *HS7KwhBoard::GetCharger(size_t channel) {
	static AcCharger charger(
		CHARGER_1,
		GetRelay(CHARGER_1),
		GetPwmController(CHARGER_1),
		GetCpDetector(CHARGER_1),
		GetMeter(),
		GetStorage()
	);
	return &charger;
}

CpDetector *HS7KwhBoard::GetCpDetector(size_t channel) {
	static AdcCpDetector cp_detector;
	return &cp_detector;
}

Led *HS7KwhBoard::GetLed(size_t channel) {
	static NoLed led;
	return &led; // 无 LED
}

Relay *HS7KwhBoard::GetRelay(size_t channel) {
	static PairGpioRelay relay(GPIOB, GPIO_PIN_0, GPIOB, GPIO_PIN_1, GPIOB, GPIO_PIN_2, GPIOB, GPIO_PIN_3);
	return &relay;
}

PwmController *HS7KwhBoard::GetPwmController(size_t channel) {
	static TimPwmController pwm(&htim3, TIM_CHANNEL_1);
	return &pwm;
}

Rfid *HS7KwhBoard::GetRfid(size_t channel) {
	return nullptr; // 无 RFID
}

Meter *HS7KwhBoard::GetMeter() {
	static MeterBL0940 meter(GetSerial(SERIAL_METER));
	return &meter;
}

Network *HS7KwhBoard::GetNetwork() {
	return nullptr; // 无网络模块
}

Storage *HS7KwhBoard::GetStorage() {
	return &InternalFlash::GetInstance();
}

// 私有实现

Serial *HS7KwhBoard::GetSerial(size_t port) {
	static SerialUART serials[NUM_SERIALS] = {
		SerialUART(&huart1, 256, 2), // NETWORK
		SerialUART(&huart2, 48,  2), // METER
		SerialUART(&huart3, 128, 2)  // DISPLAY
	};
	return &serials[port];
}
