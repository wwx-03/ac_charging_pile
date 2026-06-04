#include "serial_uart.hpp"

#include <string.h>
#include <log/log>

#include "application.hpp"

namespace {

	static constexpr inline uint32_t RX_COMPLETED = (1 << 0);

	HAL_StatusTypeDef RestartReceiveToIdleDMA(UART_HandleTypeDef *huart, uint8_t *buffer, size_t size) {
		HAL_StatusTypeDef status = HAL_UARTEx_ReceiveToIdle_DMA(huart, buffer, size);
		if (status == HAL_BUSY) {
			// Busy means RX DMA state may still be latched after error; stop and re-arm once.
			HAL_UART_DMAStop(huart);
			status = HAL_UARTEx_ReceiveToIdle_DMA(huart, buffer, size);
		}
		if (status == HAL_OK && huart->hdmarx != nullptr) {
			__HAL_DMA_DISABLE_IT(huart->hdmarx, DMA_IT_HT);
		}
		return status;
	}


}

SerialUART::SerialUART(UART_HandleTypeDef *huart, size_t item_size, size_t item_length)
		: huart_(huart)
		, ring_buffer_(item_size, item_length) {
	tx_binary_ = xSemaphoreCreateBinary();
	configASSERT(tx_binary_);
	xSemaphoreGive(tx_binary_);

	mutex_ = xSemaphoreCreateMutex();
	configASSERT(mutex_);

	HAL_StatusTypeDef status = HAL_UARTEx_ReceiveToIdle_DMA(huart_, ring_buffer_.GetCurrent(), ring_buffer_.GetItemSize());
	configASSERT(status == HAL_OK);
	__HAL_DMA_DISABLE_IT(huart_->hdmarx, DMA_IT_HT);
}

SerialUART::~SerialUART() {
	vSemaphoreDelete(tx_binary_);
}

bool SerialUART::WriteAsync(const uint8_t *data, size_t size, uint32_t timeout, bool need_copy) {
	if (xSemaphoreTake(tx_binary_, timeout) != pdTRUE) {
		LOGE("SerialUART: Failed to take semaphore for writing");
		return false;
	}

	uint8_t *tx_data{};
	if (need_copy) {
		tx_data = new uint8_t[size];
		if (!tx_data) {
			LOGE("SerialUART: Failed to allocate memory for TX data");
			xSemaphoreGive(tx_binary_);
			return false;
		}
		need_free_ = true;
	} else {
		tx_data = const_cast<uint8_t *>(data);
		need_free_ = false;
	}

	if (HAL_UART_Transmit_DMA(huart_, tx_data, size) != HAL_OK) {
		LOGE("SerialUART: Failed to start UART transmission");
		xSemaphoreGive(tx_binary_);
		if (need_copy) {
			need_free_ = false;
			delete[] tx_data;
		}
		return false;
	}
	return true;
}

bool SerialUART::WriteSync(const uint8_t *data, size_t size, uint32_t timeout) {
	if (xSemaphoreTake(tx_binary_, timeout) != pdTRUE) {
		LOGE("SerialUART: Failed to take semaphore for writing");
		return false;
	}
	
	bool ok = HAL_UART_Transmit(huart_, const_cast<uint8_t *>(data), size, 0) == HAL_OK;
	xSemaphoreGive(tx_binary_);
	return ok;
}

bool SerialUART::WriteRead(const uint8_t *write_data, size_t write_length, uint8_t *read_data, size_t read_length, uint32_t timeout) {
	if (xSemaphoreTake(mutex_, timeout) != pdTRUE) {
		LOGE("SerialUART: Failed to take semaphore for writing");
		return false;
	}

	rx_data_ = read_data;
	rx_data_size_ = read_length;
	task_to_notify_ = xTaskGetCurrentTaskHandle();

	TickType_t start = xTaskGetTickCount();
	TickType_t deadline = start + timeout;

	if (!WriteAsync(write_data, write_length, timeout, false)) {
		xSemaphoreGive(mutex_);
		return false;
	}

	uint32_t notified;
	TickType_t remaining = deadline - xTaskGetTickCount();
	BaseType_t result = xTaskNotifyWait(0, 0, &notified, remaining > 0 ? remaining : 0);
	if (result != pdTRUE) {
		LOGE("SerialUART: Failed to wait for notification");
	}
	task_to_notify_ = nullptr;
	xSemaphoreGive(mutex_);

	return (notified & RX_COMPLETED);
}

void SerialUART::MessageReceivedCallback(UART_HandleTypeDef *huart, size_t size) {
	if (huart_ == huart) {
		if (task_to_notify_) {
			memcpy(rx_data_, huart->pRxBuffPtr, (size < rx_data_size_) ? size : rx_data_size_);
			xTaskNotifyFromISR(task_to_notify_, RX_COMPLETED, eSetBits, nullptr);
		} else {
			auto &[callback, args] = msg_rxd_callback_;
			callback(args, huart->pRxBuffPtr, size);
		}
		uint8_t *next = ring_buffer_.GetNext();
		std::memset(next, 0, ring_buffer_.GetItemSize());
		HAL_UARTEx_ReceiveToIdle_DMA(huart_, next, ring_buffer_.GetItemSize());
		__HAL_DMA_DISABLE_IT(huart_->hdmarx, DMA_IT_HT);
	}
}

void SerialUART::TxCompleteCallback(UART_HandleTypeDef *huart) {
	if (huart_ == huart) {

		auto &app = Application::GetInstance();
		app.Schedule(+[](void *args) {
			auto *self = static_cast<SerialUART*>(args);
			if (self->need_free_) {
				self->need_free_ = false;
				delete[] self->huart_->pTxBuffPtr;
			}
		}, this);

		BaseType_t xHigherPriorityTaskWoken = pdFALSE;
		xSemaphoreGiveFromISR(tx_binary_, &xHigherPriorityTaskWoken);
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	}
}

void SerialUART::ErrorCallback(UART_HandleTypeDef *huart) {
	if (huart_ == huart) {

		const uint32_t errorCode = huart->ErrorCode;
		bool error_detected = false;

		if (errorCode & HAL_UART_ERROR_PE) {
			__HAL_UART_CLEAR_PEFLAG(huart);
			error_detected = true;
		}
		if (errorCode & HAL_UART_ERROR_ORE) {
			__HAL_UART_CLEAR_OREFLAG(huart);
			error_detected = true;
		}
		if (errorCode & HAL_UART_ERROR_NE) {
			__HAL_UART_CLEAR_NEFLAG(huart);
			error_detected = true;
		}
		if (errorCode & HAL_UART_ERROR_FE) {
			__HAL_UART_CLEAR_FEFLAG(huart);
			error_detected = true;
		}
		if (errorCode & HAL_UART_ERROR_DMA) {
			error_detected = true;
		}

		if (!error_detected) {
			return;
		}

		LOGW("SerialUART: UART error detected, code=0x%08lX", errorCode);

		if (task_to_notify_) {
			xTaskNotifyFromISR(task_to_notify_, 0, eSetBits, nullptr);
		}

		// HAL在错误场景下不一定会再触发TxComplete，兜底释放发送信号量，避免后续写操作永久阻塞。
		if (huart_->gState != HAL_UART_STATE_READY) {
			BaseType_t xHigherPriorityTaskWoken = pdFALSE;
			xSemaphoreGiveFromISR(tx_binary_, &xHigherPriorityTaskWoken);
			portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
		}

		uint8_t *next = ring_buffer_.GetNext();
		HAL_StatusTypeDef status = RestartReceiveToIdleDMA(huart_, next, ring_buffer_.GetItemSize());
		if (status != HAL_OK) {
			LOGE("SerialUART: Failed to restart ReceiveToIdle DMA after error, status=%d", status);
		}
	}
}
