#pragma once

#include <stdint.h>
#include <stddef.h>

class RingBuffer {
public:
	RingBuffer(size_t item_size, size_t item_length);
	~RingBuffer();

	uint8_t *GetNext();
	uint8_t *GetCurrent() { return buffer_ + index_ * item_size_; }
	size_t Capacity() const { return item_size_ * item_length_; }
	size_t GetItemSize() const { return item_size_; }
	size_t GetItemLength() const { return item_length_; }
	size_t GetIndex() const { return index_; }

private:
	uint8_t *buffer_;
	const size_t item_size_;
	const size_t item_length_;
	size_t index_;
};
