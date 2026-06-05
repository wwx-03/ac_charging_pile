#pragma once

#include "meter.hpp"
#include "serial/serial.hpp"

#include "FreeRTOS.h"
#include "queue.h"

class MeterBL0940 : public Meter {
public:
	MeterBL0940(Serial *serial);
	~MeterBL0940() = default;
	MeterBL0940(const MeterBL0940 &) = delete;
	MeterBL0940 &operator=(const MeterBL0940 &) = delete;

	void Start() override;
	float GetVoltage() override { return voltage_; }
	float GetCurrent(size_t channel) override { return current_; }
	float GetPower(size_t channel) override { return power_; }
	float GetEnergy(size_t channel) override { return energy_; }

	// 在板级支持代码中调用，将收到的消息放到队列中，供主事件循环处理
	void EnqueueMessage(const uint8_t *data, size_t size);

private:
	struct QueuePacket {
		const uint8_t *data;
		size_t size;
	};

	static constexpr inline uint8_t HEAD = 0x58;
	static constexpr inline const uint8_t READ_ALL[] = {HEAD, 0xAA};

	static constexpr float CURRENT_COEFFICIENT = 3.6855e-6f;
	static constexpr float VOLTAGE_COEFFICIENT = 7.0772e-5f;
	static constexpr float POWER_COEFFICIENT = 1.643e-3f;
	static constexpr float ENERGY_COEFFICIENT = 0.000223f;

	Serial        *serial_;
	QueueHandle_t queue_;

	float voltage_ = 0.0f;
	float current_ = 0.0f;
	float power_   = 0.0f;
	float energy_  = 0.0f;

	uint8_t error_times_;

	void MainEventLoop();
	void ParseMessage(const uint8_t *data, uint16_t size);
};
