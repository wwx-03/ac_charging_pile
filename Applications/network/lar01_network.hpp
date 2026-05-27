#pragma once

#include "main.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"

#include <log/log>

#include "network.hpp"
#include "serial/serial.hpp"
#include "util/lock_guard.hpp"

class Lar01Network : public Network {
public:
	Lar01Network(Serial *serial, GPIO_TypeDef *power_port, uint16_t power_pin, GPIO_TypeDef *reset_port, uint16_t reset_pin);
	~Lar01Network();

	void Start() override;
	void OnMessageReceived(const uint8_t *data, size_t length) override;
	void SendData(int fd, const uint8_t* data, size_t length) override;

private:
	Serial *serial_;
	GPIO_TypeDef *power_port_;
	uint16_t power_pin_;
	GPIO_TypeDef *reset_port_;
	uint16_t reset_pin_;
	QueueHandle_t rx_queue_;
	QueueHandle_t tx_queue_;
	SemaphoreHandle_t tx_mutex_;
	TaskHandle_t task_to_notify_;
	const char *expected_;

	bool SendATCommand(const char *command, const char *expected, uint32_t retry, uint32_t timeout, bool need_copy = false);
	void TransmitTask();
	void ReceiveTask();
	void DispatchData(const uint8_t *data, size_t length);
	void DispatchQiurcData(const uint8_t *data, size_t length, const char *string, const char *start);
	void DispatchIccidData(const uint8_t *data, size_t length, const char *string, const char *start);
	void DispatchCsqData(const uint8_t *data, size_t length, const char *string, const char *start);
	void DispatchClkData(const uint8_t *data, size_t length, const char *string, const char *start);
	bool Connect();
	void EnabledPower(bool enabled) { power_port_->BSRR = enabled ? power_pin_ : (power_pin_ << 16U); }
	void EnabledReset(bool enabled) { reset_port_->BSRR = enabled ? reset_pin_ : (reset_pin_ << 16U); }
	friend class LockGuard<Lar01Network>;
	void Lock() { if (xSemaphoreTake(tx_mutex_, pdMS_TO_TICKS(100)) != pdTRUE) { LOGE("Lar01Network: failed to take TX mutex"); } }
	void Unlock() { if (xSemaphoreGive(tx_mutex_) != pdTRUE) { LOGE("Lar01Network: failed to give TX mutex"); } }
};
