#pragma once

#include "main.h"

#include "storage.hpp"
#include "util/lock_guard.hpp"

class InternalFlash : public Storage {
public:
	static InternalFlash &GetInstance() {
		static InternalFlash instance;
		return instance;
	}

	void Save(uint32_t address, const uint8_t *data, size_t length) override;
	void Load(uint32_t address, uint8_t *data, size_t length) override;
private:
	uint32_t flash_size_;
	uint32_t page_size_;

	InternalFlash();
	~InternalFlash() = default;
	InternalFlash(const InternalFlash &) = delete;
	InternalFlash &operator=(const InternalFlash &) = delete;
};
