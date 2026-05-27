#pragma once

#include "main.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "event_groups.h"

#include "serial.hpp"
#include "util/ring_buffer.hpp"

class SerialUART : public Serial {
public:
	SerialUART(UART_HandleTypeDef *huart, size_t item_size = 256, size_t item_length = 2);
	~SerialUART();
	bool WriteAsync(const uint8_t *data, size_t size, uint32_t timeout, bool need_copy = true) override;
	bool WriteSync(const uint8_t *data, size_t size, uint32_t timeout) override;
	bool WriteRead(const uint8_t *write_data, size_t write_length, uint8_t *read_data, size_t read_length, uint32_t timeout) override;

	void MessageReceivedCallback(UART_HandleTypeDef *huart, size_t size);
	void TxCompleteCallback(UART_HandleTypeDef *huart);
	void ErrorCallback(UART_HandleTypeDef *huart);

private:
	UART_HandleTypeDef *huart_;
	RingBuffer ring_buffer_;
	SemaphoreHandle_t tx_binary_;
	SemaphoreHandle_t mutex_;
	TaskHandle_t task_to_notify_;
	uint8_t *rx_data_;
	size_t rx_data_size_;
	bool need_free_;
}; 

