#pragma once

class Relay {
public:
	virtual ~Relay() = default;
	virtual void SetState(bool on) = 0;
};
