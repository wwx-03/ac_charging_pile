#include "hsevb_ac7kcar01_v22_board.hpp"

#ifdef CONFIG_USE_HSEVB_AC7KCAR01_V22_BOARD

#include "tim.h"
#include "usart.h"

#include "charger/ac_charger.hpp"
#include "cp_detector/adc_cp_detector.hpp"

#include "relay/pair_gpio_relay.hpp"
#include "pwm_controller/tim_pwm_controller.hpp"

#include "meter/meter_bl0940.hpp"

#include "storage/internal_flash.hpp"

#include "serial/serial_uart.hpp"

DECLARE_BOARD_CLASS(HS7KwhBoard)

namespace {

	static constexpr size_t NUM_CHARGERS = 1;
	static constexpr size_t CHARGER_1 = 0;

	static constexpr size_t NUM_SERIALS = 3;
	static constexpr size_t SERIAL_NETWORK = 0;
	static constexpr size_t SERIAL_METER = 1;
	static constexpr size_t SERIAL_DISPLAY = 2;

}

HS7KwhBoard::HS7KwhBoard() {

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
		SerialUART(&huart1, 256, 256, 2), // NETWORK
		SerialUART(&huart2, 16,  48,  2), // METER
		SerialUART(&huart3, 128, 128, 2)  // DISPLAY
	};
	return &serials[port];
}

// 板级支持回调注册


extern "C" void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t size) {
	auto &board = static_cast<HS7KwhBoard &>(HS7KwhBoard::GetInstance());
	for (size_t i = 0; i < NUM_SERIALS; ++i) {
		auto *serial = static_cast<SerialUART *>(board.GetSerial(i));
		serial->MessageReceivedCallback(huart, size);
	}
}

extern "C" void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
	auto &board = static_cast<HS7KwhBoard &>(HS7KwhBoard::GetInstance());
	for (size_t i = 0; i < NUM_SERIALS; ++i) {
		auto *serial = static_cast<SerialUART *>(board.GetSerial(i));
		serial->TxCompleteCallback(huart);
	}
}

#endif /* CONFIG_USE_HSEVB_AC7KCAR01_V22_BOARD */
