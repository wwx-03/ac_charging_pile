#pragma once

#include "FreeRTOS.h"
#include "timers.h"

#include "protocol.hpp"

class OpsProtocol : public Protocol {
public:
	OpsProtocol(Network *network);
	void OnDataReceived(const uint8_t* data, size_t length) override;

private:
	TimerHandle_t timer_;

	void ChildStart() override;
};

