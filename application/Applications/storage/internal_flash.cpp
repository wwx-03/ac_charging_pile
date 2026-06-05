#include "internal_flash.hpp"

#include <string.h>
#include <custom>
#include <log/log>

InternalFlash::InternalFlash() {
	flash_size_ = *((volatile uint32_t *)FLASH_SIZE_DATA_REGISTER);
	if (flash_size_ <= 128) {
		page_size_ = 1024;
	} else {
		page_size_ = 2048;
	}
	LOGI("InternalFlash: Detected flash size %lu KB, page size %lu B", flash_size_, page_size_);
}

void InternalFlash::Save(uint32_t address, const uint8_t *data, size_t length) {
	if (address + length > FLASH_BASE + flash_size_ * page_size_) {
		LOGE("InternalFlash: Save out of bounds (address=0x%08lX, length=%lu)", address, length);
		return;
	}

	uint8_t *buffer = new uint8_t[page_size_];
	if (!buffer) {
		LOGE("InternalFlash: Failed to allocate buffer for flash page");
		return;
	}

	HAL_FLASH_Unlock();

	size_t written = 0;
	while (written < length) {

		uint32_t page_start = FLASH_BASE + (address - FLASH_BASE) / page_size_ * page_size_;
		uint32_t offset = address - page_start;
		uint32_t chunk_size = custom::min(page_size_ - offset, length - written);

		Load(page_start, buffer, page_size_);

		uint32_t error = 0;
		FLASH_EraseInitTypeDef init{};
		init.TypeErase = FLASH_TYPEERASE_PAGES;
		init.Banks = FLASH_BANK_1;
		init.PageAddress = page_start;
		init.NbPages = 1;
		if (HAL_FLASHEx_Erase(&init, &error) != HAL_OK) {
			LOGE("FLASH erase failed at 0x%08X!\r\n", page_start);
			goto exit;
		}

		memcpy(buffer + offset, data + written, chunk_size);
		// ÆæÊý²¹Æë
		if ((chunk_size & 0x01) != 0) {
			buffer[offset + chunk_size] = 0xFF;
			++chunk_size; 
		}

		for (size_t i = 0; i < page_size_; i += 2) {
			uint16_t half_word = buffer[i] | (buffer[i + 1] << 8);
			if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, page_start + i, half_word) != HAL_OK) {
				LOGE("FLASH program failed at 0x%08X!\r\n", page_start + i);
				goto exit;
			}
		}

		address += chunk_size;
		written += chunk_size;
	}

exit:
	HAL_FLASH_Lock();
	delete[] buffer;
}

void InternalFlash::Load(uint32_t address, uint8_t *data, size_t length) {
	if (address + length > FLASH_BASE + flash_size_ * page_size_) {
		LOGE("InternalFlash: Load out of bounds (address=0x%08lX, length=%lu)", address, length);
		return;
	}

	memcpy(data, (const void *)address, length);
}
