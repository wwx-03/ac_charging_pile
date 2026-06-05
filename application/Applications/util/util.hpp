#pragma once

#include <stdint.h>
#include <stddef.h>

namespace util {


	bool IsAscii(const uint8_t *data, size_t length);


	int HexCharToNibble(char c);
	int HexStringToBytes(const char *string, uint8_t *buffer, size_t buffer_size);

}
