#include "meter_bl0940.hpp"

#include <log/log>

#include "task.h"

#include "application.hpp"

namespace {

// 贝岭全参结构体定义
union Packet {

	uint8_t data[35];

	struct {
		uint8_t head;
		uint8_t ia_fast_rms[3];
		uint8_t ia_rms[3];
		uint8_t reserved1[3];
		uint8_t v_rms[3];
		uint8_t reserved2[3];
		uint8_t a_watt[3];
		uint8_t reserved3[3];
		uint8_t cfa_cnt[3];
		uint8_t reserved4[3];
		uint8_t tps1[3];
		uint8_t tps2[3];
		uint8_t checksum;
	};

	bool IsValidChecksum(uint8_t head) const {
		uint8_t temp = head; // 包头也参与校验
		for (uint8_t i = 0; i < sizeof(data) - 1; ++i) {
			temp += data[i];
		}
		temp = ~(temp & 0xFF);
		return temp == checksum;
	}
};

// 队列定义
constexpr size_t QUEUE_LENGTH = 2;

struct QueueItem {
	const uint8_t *data;
	size_t size;
};

// 错误次数达到这个值时触发故障事件
constexpr uint8_t MAX_ERROR_TIME = 3;

} // namespace

MeterBL0940::MeterBL0940(Serial *serial)
		: serial_(serial) {
	queue_ = xQueueCreate(QUEUE_LENGTH, sizeof(QueueItem));
	configASSERT(queue_);
}

void MeterBL0940::Start() {
	xTaskCreate(+[](void *args) {
		auto *self = static_cast<MeterBL0940 *>(args);
		self->MainEventLoop();
	}, NULL, 128, this, tskIDLE_PRIORITY + 1, NULL);
}

void MeterBL0940::EnqueueMessage(const uint8_t *data, size_t size) {
	QueueItem packet{data, size};

	if (xPortIsInsideInterrupt()) {
		xQueueSendFromISR(queue_, &packet, nullptr);
	} else {
		if (xQueueSend(queue_, &packet, pdMS_TO_TICKS(100)) != pdPASS) {
			LOGE("MeterBL0940::EnqueueMessage: Failed to send packet to queue!\r\n");
		}
	}
}

void MeterBL0940::ParseMessage(const uint8_t *data, uint16_t size) {
	
	// 检查数据包是否有有效的头部和足够的长度，避免访问越界
	if (!data || size < sizeof(Packet)) {
		LOGE("MeterBL0940::ParseMessage: Invalid data or size!\r\n");
		if (++error_times_ == MAX_ERROR_TIME) {
			auto &[function, args] = fault_cb_;
			function(args);
		}
		return;
	}

	// 直接将数据解释为Packet结构体，利用其成员访问功能进行解析
	auto *ptr = reinterpret_cast<const Packet *>(data);
	if (!ptr->IsValidChecksum(HEAD)) {
		LOGE("MeterBL0940::ParseMessage: Invalid checksum!\r\n");
		if (++error_times_ == MAX_ERROR_TIME) {
			auto &[function, args] = fault_cb_;
			function(args);
		}
		return;
	}

	uint32_t ia_rms = ptr->ia_rms[0] + (ptr->ia_rms[1] << 8) + (ptr->ia_rms[2] << 16);
	uint32_t v_rms = ptr->v_rms[0] + (ptr->v_rms[1] << 8) + (ptr->v_rms[2] << 16);
	uint32_t a_watt = ptr->a_watt[0] + (ptr->a_watt[1] << 8) + (ptr->a_watt[2] << 16);
	uint32_t cfa = ptr->cfa_cnt[0] + (ptr->cfa_cnt[1] << 8) + (ptr->cfa_cnt[2] << 16);
	// uint32_t tps1 = ptr->tps1[0] + (ptr->tps1[1] << 8) + (ptr->tps1[2] << 16);

	a_watt = (a_watt & 0x00800000) ? (~a_watt + 1) & 0xFFFFFF : a_watt;

	voltage_ = v_rms * VOLTAGE_COEFFICIENT;
	current_ = ia_rms * CURRENT_COEFFICIENT;
	power_   = a_watt * POWER_COEFFICIENT;
	energy_  = cfa * ENERGY_COEFFICIENT;

	// 数据解析成功，重置错误计数器
	if (error_times_ >= MAX_ERROR_TIME) {
		auto &[function, args] = fault_cleared_cb_;
		function(args);
	}
	error_times_ = 0;
}

void MeterBL0940::MainEventLoop() {

	QueuePacket packet{};

	while (1) {
		// 发送读取命令，100ms 超时，异步发送避免阻塞
		serial_->WriteAsync(READ_ALL, sizeof(READ_ALL), pdMS_TO_TICKS(100), false);

		if (xQueueReceive(queue_, &packet, pdMS_TO_TICKS(1000)) != pdTRUE) {
			// 只触发一次错误事件，避免事件风暴
			if (++error_times_ == MAX_ERROR_TIME) {
				auto &[function, args] = fault_cb_;
				function(args);
			}
			continue;
		}
		
		ParseMessage(packet.data, packet.size);

		// 每秒读取一次数据，过于频繁可能会导致串口接收缓冲区溢出
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}
