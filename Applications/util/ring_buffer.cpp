#include "ring_buffer.hpp"

#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

RingBuffer::RingBuffer(size_t item_size, size_t item_length)
		: buffer_(nullptr)
		, item_size_(item_size)
		, item_length_(item_length)
		, index_(0) {
	buffer_ = new uint8_t[item_size_ * item_length_];
	configASSERT(buffer_);
	memset(buffer_, 0, item_size_ * item_length_);
}

RingBuffer::~RingBuffer() {
	delete[] buffer_;
}

uint8_t *RingBuffer::GetNext() {
	index_ = (index_ + 1) % item_length_;
	return buffer_ + index_ * item_size_;
}
