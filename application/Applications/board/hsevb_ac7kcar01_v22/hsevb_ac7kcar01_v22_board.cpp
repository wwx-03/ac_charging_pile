#include "hsevb_ac7kcar01_v22_board.hpp"

#ifdef CONFIG_USE_HSEVB_AC7KCAR01_V22_BOARD

#include "adc.h"
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

	static constexpr size_t NUM_ADC1_CHANNELS = 1;
	static constexpr size_t ADC_CHANNEL_CP = 0;

	// 
	static uint16_t s_adc1_buffer[NUM_ADC1_CHANNELS] = {};

}

HS7KwhBoard::HS7KwhBoard() {

	HAL_ADCEx_Calibration_Start(&hadc1);
	HAL_ADC_Start_DMA(&hadc1, (uint32_t *)s_adc1_buffer, NUM_ADC1_CHANNELS);
	__HAL_DMA_DISABLE_IT(hadc1.DMA_Handle, DMA_IT_HT);
	HAL_TIM_Base_Start(&htim3);

	// 注册串口回调
	auto *serial = GetSerial(SERIAL_METER);
	serial->OnMessageReceived(+[](void *args, const uint8_t *data, size_t size) {
		auto &board = GetInstance();
		auto *meter = static_cast<MeterBL0940 *>(board.GetMeter());
		meter->EnqueueMessage(data, size);
	}, nullptr);

	auto *pwm = static_cast<TimPwmController *>(GetPwmController(CHARGER_1));
	pwm->SetInverted(true); // 反转占空比，使得 100% 对应最小脉冲宽度，0% 对应最大脉冲宽度
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
	static PairGpioRelay relay(GPIOA, GPIO_PIN_0, GPIOB, GPIO_PIN_13, GPIOA, GPIO_PIN_0, GPIOB, GPIO_PIN_14);
	return &relay;
}

PwmController *HS7KwhBoard::GetPwmController(size_t channel) {
	static TimPwmController pwm(&htim4, TIM_CHANNEL_3);
	return &pwm;
}

Rfids *HS7KwhBoard::GetRfids(size_t channel) {
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

extern "C" void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
	// 充电桩控制器使用 ADC 进行 CP 检测，注册回调以便在 ADC 转换完成时通知 CP 检测器
	auto &board = static_cast<HS7KwhBoard &>(HS7KwhBoard::GetInstance());
	for (size_t i = 0; i < NUM_CHARGERS; ++i) {
		auto *cp_detector = static_cast<AdcCpDetector *>(board.GetCpDetector(i));
		cp_detector->UpdateState(s_adc1_buffer[ADC_CHANNEL_CP]);
	}
}

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

extern "C" void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
	auto &board = static_cast<HS7KwhBoard &>(HS7KwhBoard::GetInstance());
	for (size_t i = 0; i < NUM_SERIALS; ++i) {
		auto *serial = static_cast<SerialUART *>(board.GetSerial(i));
		serial->ErrorCallback(huart);
	}
}

#endif /* CONFIG_USE_HSEVB_AC7KCAR01_V22_BOARD */
