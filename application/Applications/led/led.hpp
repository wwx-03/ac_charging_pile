#pragma once

#include "charger/charger.hpp"

class Led {
public:
	virtual ~Led() = default;

	virtual void OnStateChanged(Charger::State new_state) = 0;
};

class NoLed : public Led {
public:
	void OnStateChanged(Charger::State new_state) override {}
};
