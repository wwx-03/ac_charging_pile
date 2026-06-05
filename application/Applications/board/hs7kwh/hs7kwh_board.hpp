#pragma once

#include "../board.hpp"

#include "serial/serial.hpp"

class HS7KwhBoard : public Board {
public:

	size_t GetNumChargers() const override;

	Charger       *GetCharger(size_t channel) override;
	CpDetector    *GetCpDetector(size_t channel) override;
	Relay         *GetRelay(size_t channel) override;
	PwmController *GetPwmController(size_t channel) override;
	Led           *GetLed(size_t channel) override;
	Rfid          *GetRfid(size_t channel) override;
	Meter         *GetMeter() override;
	Network       *GetNetwork() override;
	Storage       *GetStorage() override;

private:
	Serial *GetSerial(size_t port);
};

