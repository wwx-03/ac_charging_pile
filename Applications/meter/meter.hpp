#pragma once

#include <stddef.h>

class Meter {
public:
	virtual ~Meter() = default;
	virtual float GetVoltage() = 0;
	virtual float GetCurrent(size_t channel) = 0;
	virtual float GetPower(size_t channel) = 0;
	virtual float GetEnergy(size_t channel) = 0;
};
