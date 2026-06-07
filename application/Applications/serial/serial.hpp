#pragma once

#include <stdint.h>
#include <stddef.h>

#include <utility>
#include <functional/functional>

class Serial {
public:
	virtual ~Serial() = default;
	virtual bool WriteAsync(const uint8_t *data, size_t length, uint32_t timeout, bool need_copy = true) = 0;
	virtual bool WriteSync(const uint8_t *data, size_t length, uint32_t timeout) = 0;
	virtual bool WriteRead(const uint8_t *write_data, size_t write_length, uint8_t *read_data, size_t read_length, uint32_t timeout) = 0;	

	void OnMessageReceived(custom::function<void(void *, const uint8_t *, size_t)> callback, void *args) {
		msg_rxd_callback_ = {callback, args};
	}

protected:
	std::pair<custom::function<void(void *, const uint8_t *, size_t)>, void *> msg_rxd_callback_;
};
