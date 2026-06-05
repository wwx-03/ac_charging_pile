#include "util.hpp"

#include <string.h>

bool util::IsAscii(const uint8_t *data, size_t length) {
	for (size_t i = 0; i < length; ++i) {
		if (data[i] > 0x7F) {
			return false;
		}
	}
	return true;
}

int util::HexCharToNibble(char c) {
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	return -1;
}

int util::HexStringToBytes(const char *string, uint8_t *buffer, size_t buffer_size) {
	size_t string_length = strlen(string);
	if (string_length % 2 != 0) {
		return -1; // Hex string length must be even
	}
	size_t byte_length = string_length / 2;
	if (byte_length > buffer_size) {
		return -1; // Buffer is too small
	}
	for (size_t i = 0; i < byte_length; ++i) {
		int high = HexCharToNibble(string[2 * i]);
		int low = HexCharToNibble(string[2 * i + 1]);
		if (high < 0 || low < 0) {
			return -1; // Invalid hex character
		}
		buffer[i] = (high << 4) | low;
	}
	return static_cast<int>(byte_length);
}
