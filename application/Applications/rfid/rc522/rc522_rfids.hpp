#pragma once

#include "main.h"

#include "rc522.hpp"
#include "../rfids.hpp"

class Rc522Rfids : public Rfids {
public:
	Rc522Rfids(const Rc522 *rc522, size_t num);

private:
	const Rc522 *rc522_;
	size_t       num_;
};
