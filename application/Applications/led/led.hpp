#pragma once

class Led {
public:
	virtual ~Led() = default;
};

class NoLed : public Led {
};
