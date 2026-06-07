#pragma once

#include "main.h"

class Rc522 {
public:
	Rc522(SPI_HandleTypeDef *hspi, GPIO_TypeDef *cs_port, uint16_t cs_pin);
	~Rc522();

	bool IsCardPresent() const;
	size_t ReadCardSerial(uint8_t *const buffer, size_t size) const;
	bool Authenticate(uint8_t block_addr, const uint8_t *key, const uint8_t *serial) const;
	bool ReadBlock(uint8_t block_addr, uint8_t *buffer, size_t size) const;
	bool WriteBlock(uint8_t block_addr, const uint8_t *data, size_t size) const;

private:
	SPI_HandleTypeDef *hspi_;
	GPIO_TypeDef      *cs_port_;
	uint16_t           cs_pin_;

	uint8_t ReadRegister(uint8_t reg) const;
	void    WriteRegister(uint8_t reg, uint8_t value) const;
	// FIFO 相关操作
	void ClearFifo() const;
	// 数据收发
	int TransceiveData(uint8_t command, const uint8_t *tx_data, size_t tx_size, uint8_t *rx_data, size_t *rx_size) const;
	// 选卡操作
	int SelectCard(uint8_t *uid, uint8_t *uid_length, uint8_t *sak) const;
};
