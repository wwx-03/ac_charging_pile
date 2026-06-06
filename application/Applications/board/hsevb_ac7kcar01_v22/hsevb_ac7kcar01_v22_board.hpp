#pragma once

#include "project_config.h"

#ifdef CONFIG_USE_HSEVB_AC7KCAR01_V22_BOARD

#include "../board.hpp"

#include "serial/serial.hpp"

class HS7KwhBoard : public Board {
public:
	HS7KwhBoard();

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

	// 特有的外设接口
	Serial *GetSerial(size_t port);
private:

};

#endif /* CONFIG_USE_HSEVB_AC7KCAR01_V22_BOARD */
