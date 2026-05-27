#pragma once

#include <stdint.h>
#include <stddef.h>

class Storage {
public:
	virtual ~Storage() = default;
	virtual void Save(uint32_t address, const uint8_t *data, size_t length) = 0;
	virtual void Load(uint32_t address, uint8_t *data, size_t length) = 0;
};
