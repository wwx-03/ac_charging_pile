#pragma once

#include <stdint.h>

class CPDetector {
public:

	enum CPState : uint8_t {
		GROUNDING = 0,
		DISCONNECTED,
		CONNECTED,
		READY,
	};

	virtual ~CPDetector() = default;
	virtual CPState GetState() = 0;
};
