#include "lar01_network.hpp"

#include <time.h>
#include <log/log>
#include <time/time>
#include <string/string>

#include "util/util.hpp"

#define RETURN_ON_ERROR(expr)	do {					\
	if (!(expr)) {							\
		LOGE("Lar01Network: error at %s\r\n", #expr);		\
		return false;						\
	}								\
} while (0)

namespace {
	struct RxQueueItem {
		const uint8_t *data;
		size_t length;
	};

	struct TxQueueItem {
		uint8_t eid;
		uint8_t data[16];
	};

	struct SendDataEventData {
		int fd;
		uint8_t *data;
		size_t length;
	};
	static_assert(sizeof(SendDataEventData) <= sizeof(TxQueueItem::data), "SendDataEventData is too large for TxQueueItem");

	enum EventId : uint8_t {
		DEVICE_READY = Network::MAX_EVENT_ID,
		SEND_DATA,
	};

	static constexpr inline size_t QUEUE_LENGTH = 3;

	static constexpr inline uint32_t AT_COMMAND_OK = (1 << 0);
	static constexpr inline uint32_t AT_COMMAND_ERROR = (1 << 1);

	void PrintData(const uint8_t *data, size_t length) {
		LOGI_ARRAY("Hex format data", data, length);
		LOGI("String format data: %s\n", reinterpret_cast<const char*>(data));
	}
}

Lar01Network::Lar01Network(Serial *serial, GPIO_TypeDef *power_port, uint16_t power_pin, GPIO_TypeDef *reset_port, uint16_t reset_pin)
		: serial_(serial)
		, power_port_(power_port)
		, power_pin_(power_pin)
		, reset_port_(reset_port)
		, reset_pin_(reset_pin) {

	EnabledPower(false);
	EnabledReset(false);
	GPIO_InitTypeDef init{};
	init.Pin = power_pin_;
	init.Mode = GPIO_MODE_OUTPUT_PP;
	init.Pull = GPIO_NOPULL;
	init.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(power_port_, &init);
	init.Pin = reset_pin_;
	HAL_GPIO_Init(reset_port_, &init);
	
	rx_queue_ = xQueueCreate(QUEUE_LENGTH, sizeof(RxQueueItem));
	configASSERT(rx_queue_);

	tx_queue_ = xQueueCreate(QUEUE_LENGTH, sizeof(TxQueueItem));
	configASSERT(tx_queue_);

	tx_mutex_ = xSemaphoreCreateMutex();
	configASSERT(tx_mutex_);
}

Lar01Network::~Lar01Network() {
	vSemaphoreDelete(tx_mutex_);
	vQueueDelete(tx_queue_);
	vQueueDelete(rx_queue_);
	HAL_GPIO_DeInit(power_port_, power_pin_);
	HAL_GPIO_DeInit(reset_port_, reset_pin_);
}

void Lar01Network::Start() {

	xTaskCreate(+[](void *args) {
		auto *self = static_cast<Lar01Network*>(args);
		self->TransmitTask();
	}, nullptr, 256, this, tskIDLE_PRIORITY + 1, nullptr);

	xTaskCreate(+[](void *args) {
		auto *self = static_cast<Lar01Network*>(args);
		self->ReceiveTask();
	}, nullptr, 256, this, tskIDLE_PRIORITY + 1, nullptr);
}

void Lar01Network::OnMessageReceived(const uint8_t *data, size_t length) {
	RxQueueItem item{data, length};
	if (xQueueSendFromISR(rx_queue_, &item, nullptr) != pdTRUE) {
		LOGE("Lar01Network: failed to enqueue received message");
	}
}

void Lar01Network::SendData(int fd, const uint8_t *data, size_t size) {
	if (!data || !size) {
		LOGE("Lar01Network: invalid data to send");
		return;
	}

	constexpr size_t MAX_PAYLOAD_SIZE = 256;
	if (size > MAX_PAYLOAD_SIZE) {
		LOGE("Lar01Network: data size exceeds maximum payload size");
		return;
	}

	int length = snprintf(nullptr, 0, "AT+QISEND=%d,\"", fd);
	if (length < 0) {
		LOGE("Lar01Network: failed to calculate AT command length");
		return;
	}

	const size_t total_len = length + size * 2 + 4; // hex data length + CRLF
	char *command = new char[total_len];
	if (!command) {
		LOGE("Lar01Network: failed to allocate memory for AT command");
		return;
	}

	length = snprintf(command, total_len, "AT+QISEND=%d,\"", fd);
	if (length < 0) {
		LOGE("Lar01Network: failed to format AT command");
		delete[] command;
		return;
	}

	for (uint16_t i = 0; i < size; ++i) {
		command[length++] = custom::string_util::hex_to_char(data[i] >> 4);
		command[length++] = custom::string_util::hex_to_char(data[i] & 0x0F);
	}
	command[length++] = '\"';
	command[length++] = '\r';
	command[length++] = '\n';
	command[length] = '\0';

	TxQueueItem item{SEND_DATA, {}};
	auto &event_data = *reinterpret_cast<SendDataEventData*>(item.data);
	event_data.fd = fd;
	event_data.data = reinterpret_cast<uint8_t*>(command);
	event_data.length = length;
	if (xQueueSend(tx_queue_, &item, pdMS_TO_TICKS(100)) != pdTRUE) {
		LOGE("Lar01Network: failed to enqueue send data event");
		delete[] command;
	}
}

bool Lar01Network::SendATCommand(const char *command, const char *expected, uint32_t retry, uint32_t timeout, bool need_copy) {

	LockGuard lock(this);
	expected_ = expected;
	task_to_notify_ = xTaskGetCurrentTaskHandle();
	LOGI("Lar01Network: sending AT command: %s", command);

	do {
		bool ok = serial_->WriteAsync(reinterpret_cast<const uint8_t*>(command), strlen(command), timeout, need_copy);
		if (!ok) {
			LOGW("Lar01Network: failed to send AT command\r\n");
			continue;
		}

		uint32_t notified{};
		UBaseType_t result = xTaskNotifyWait(0, 0, &notified, timeout);
		if (result != pdTRUE) {
			LOGW("Lar01Network: failed to wait for AT command response\r\n");
			continue;
		}

		if (notified & AT_COMMAND_OK) {
			return true;
		} else if (notified & AT_COMMAND_ERROR) {
			LOGW("Lar01Network: AT command failed\r\n");
			continue;
		} else {
			LOGW("Lar01Network: unexpected notification bits: 0x%08X\r\n", notified);
			continue;
		}

	} while (retry--);

	return false;
}

void Lar01Network::TransmitTask() {

	TxQueueItem item{};

	while (true) {
		if (xQueueReceive(tx_queue_, &item, portMAX_DELAY) != pdTRUE) {
			LOGE("Lar01Network: failed to receive message from queue");
			continue;
		}

		if (item.eid == DEVICE_READY) {
			Connect();
		} else if (item.eid == SEND_DATA) {
			auto *event_data = reinterpret_cast<SendDataEventData*>(item.data);
			SendATCommand(reinterpret_cast<const char*>(event_data->data), "OK", 3, 5000);
			delete[] event_data->data;
		} else {
			LOGW("Lar01Network: unknown event id %d", item.eid);
		}
	}
}

void Lar01Network::ReceiveTask() {

	RxQueueItem item{};

	vTaskDelay(pdMS_TO_TICKS(1000));
	EnabledPower(true);

	while (true) {
		if (xQueueReceive(rx_queue_, &item, portMAX_DELAY) != pdTRUE) {
			LOGE("Lar01Network: failed to receive message from queue");
			continue;
		}

		PrintData(item.data, item.length);
		DispatchData(item.data, item.length);
	}
}

void Lar01Network::DispatchData(const uint8_t *data, size_t length) {

	const char *string = reinterpret_cast<const char*>(data);
	const char *start = nullptr;

	if (strstr(string, "RDY") != nullptr) {
		TxQueueItem item{DEVICE_READY, {}};
		if (xQueueSend(tx_queue_, &item, portMAX_DELAY) != pdTRUE) {
			LOGE("Lar01Network: failed to enqueue device ready event");
		}
	} else if ((start = strstr(string, "+QIURC:")) != nullptr) {
		DispatchQiurcData(data, length, string, start);
	} else if ((start = strstr(string, "*ICCID:")) != nullptr) {
		DispatchIccidData(data, length, string, start);
	} else if ((start = strstr(string, "+CSQ:")) != nullptr) {
		DispatchCsqData(data, length, string, start);
	} else if ((start = strstr(string, "+CCLK:")) != nullptr) {
		DispatchClkData(data, length, string, start);
	} else if ((start = strstr(string, "ERROR")) != nullptr) {
		if (task_to_notify_) {
			xTaskNotify(task_to_notify_, AT_COMMAND_ERROR, eSetBits);
		}
	} else if (strstr(string, expected_) != nullptr) {
		if (task_to_notify_) {
			xTaskNotify(task_to_notify_, AT_COMMAND_OK, eSetBits);
		}
	} else {
		// do nothing
	}
}

void Lar01Network::DispatchQiurcData(const uint8_t *data, size_t length, const char *string, const char *start) {
	// +QIURC: "recv",<connectID>,<currentRecvLength><CR><LF><data>
	// +QIURC: "recv",0,12
	int fd{}, rx_length{};
	int parse_num = sscanf(start, "+QIURC: \"recv\",%d,%d", &fd, &rx_length);
	if (parse_num != 2) {
		LOGE("Lar01Network: failed to parse +QIURC message, parse_num=%d", parse_num);
		return;
	}

	for (const uint8_t *p = reinterpret_cast<const uint8_t*>(start); p < data + length; ++p) {
		if (*p == '\n') {
			++p;
			DataReceivedEventData event_data{fd, p, static_cast<size_t>(rx_length)};
			auto &[callback, args] = event_handler_;
			callback(args, DATA_RECEIVED, &event_data);
			break;
		}
	}
}

void Lar01Network::DispatchIccidData(const uint8_t *data, size_t length, const char *string, const char *start) {
	// "\r\n*ICCID: "89860839162440040379"\r\n"
	char iccid[32]{};
	int parse_num = sscanf(start, "*ICCID: \"%31[^\"]\"", iccid);
	if (parse_num != 1) {
		LOGE("Lar01Network: failed to parse ICCID");
		return;
	}
	uint8_t sim[16]{};
	int size = util::HexStringToBytes(iccid, sim, sizeof(sim));
	if (size < 0) {
		LOGE("Lar01Network: failed to convert ICCID to bytes");
		return;
	}
	SimGettedEventData event_data{sim, static_cast<size_t>(size)};
	auto &[callback, args] = event_handler_;
	callback(args, SIM_GETTED, &event_data);

	if (task_to_notify_ && strstr(string, expected_) != nullptr) {
		xTaskNotify(task_to_notify_, AT_COMMAND_OK, eSetBits);
	}
}

void Lar01Network::DispatchCsqData(const uint8_t *data, size_t length, const char *string, const char *start) {
	// +CSQ: <rssi>,<ber>
	int rssi{}, ber{};
	int parse_num = sscanf(start, "+CSQ: %d,%d", &rssi, &ber);
	if (parse_num != 2) {
		LOGE("Lar01Network: failed to parse signal strength from +CSQ message, parse_num=%d", parse_num);
		return;
	}
	SignalUpdatedEventData event_data{rssi};
	auto &[callback, args] = event_handler_;
	callback(args, SIGNAL_UPDATED, &event_data);

	if (task_to_notify_ && strstr(string, expected_) != nullptr) {
		xTaskNotify(task_to_notify_, AT_COMMAND_OK, eSetBits);
	}
}

void Lar01Network::DispatchClkData(const uint8_t *data, size_t length, const char *string, const char *start) {
	// +CCLK: "25/09/30,02:32:14+32"
	tm t{};
	int tz{};
	char sign{};
	int parse_num = sscanf(start, "+CCLK: \"%2d/%2d/%2d,%2d:%2d:%2d%c%d", &t.tm_year, &t.tm_mon, &t.tm_mday, &t.tm_hour, &t.tm_min, &t.tm_sec, &sign, &tz);
	if (parse_num != 8) {
		LOGE("Lar01Network: failed to parse datetime from +CCLK message, parse_num=%d", parse_num);
		return;
	}

	t.tm_year += 2000;

	int tz_offset_minutes = tz * 15;
	if (sign == '-') {
		tz_offset_minutes = -tz_offset_minutes;
	}

	int total_minutes = t.tm_hour * 60 + t.tm_min + tz_offset_minutes;
	int days_adjust = 0;

	if (total_minutes < 0) {
		while (total_minutes < 0) {
			total_minutes += 24 * 60;
			days_adjust -= 1;
		}
	} else if (total_minutes >= 24 * 60) {
		while (total_minutes >= 24 * 60) {
			total_minutes -= 24 * 60;
			days_adjust += 1;
		}
	}

	t.tm_hour = total_minutes / 60;
	t.tm_min  = total_minutes % 60;
	t.tm_mday += days_adjust;

	// 调整日期
	while (t.tm_mday < 1) {
		t.tm_mon--;
		if (t.tm_mon < 1) {
			t.tm_mon = 12;
			t.tm_year--;
		}
		t.tm_mday += custom::time::day(t.tm_year, t.tm_mon);
	}

	while (t.tm_mday > custom::time::day(t.tm_year, t.tm_mon)) {
		t.tm_mday -= custom::time::day(t.tm_year, t.tm_mon);
		t.tm_mon++;
		if (t.tm_mon > 12) {
			t.tm_mon = 1;
			t.tm_year++;
		}
	}

	TimeUpdatedEventData event_data{t};
	auto &[callback, args] = event_handler_;
	callback(args, TIME_UPDATED, &event_data);

	if (task_to_notify_ && strstr(string, expected_) != nullptr) {
		xTaskNotify(task_to_notify_, AT_COMMAND_OK, eSetBits);
	}
}

bool Lar01Network::Connect() {
	RETURN_ON_ERROR(SendATCommand("AT\r\n", "OK", 5, 3000));
	SendATCommand("ATE0\r\n", "OK", 3, 1000);
	RETURN_ON_ERROR(SendATCommand("AT+CPIN?\r\n", "+CPIN: READY", 3, 1000));
	RETURN_ON_ERROR(SendATCommand("AT+QCCID\r\n", "*ICCID:", 3, 3000));
	RETURN_ON_ERROR(SendATCommand("AT+CSQ\r\n", "+CSQ:", 3, 3000));
	RETURN_ON_ERROR(SendATCommand("AT+CCLK?\r\n", "+CCLK:", 3, 10000));
	RETURN_ON_ERROR(SendATCommand("AT+QICSGP=1,1,\"CMNET\",\"\",\"\",1\r\n", "OK", 3, 5000));
	RETURN_ON_ERROR(SendATCommand("AT+QIACT=1\r\n", "OK", 3, 10000));
	// char ops_ip[16]{}, mon_ip[16]{};
	// auto &board = Board::GetInstance();
	return true;
}
