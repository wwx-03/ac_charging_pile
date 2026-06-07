#include "rc522.hpp"

#include <string.h>

namespace {

	enum Rc522Command : uint8_t {
		CMD_IDLE            = 0x00, // 取消当前命令
		CMD_AUTHENT         = 0x0E, // 验证密钥
		CMD_TRANSCEIVE      = 0x0C, // 收发数据
		CMD_MF_AUTHENT      = 0x0E, // MIFARE认证
		CMD_SOFTRESET       = 0x0F, // 软复位
	};

	// MFRC522寄存器地址定义
	enum Rc522Register : uint8_t {
		REG_COMMAND         = 0x01, // 命令寄存器
		REG_COMIEN          = 0x02, // 中断使能
		REG_DIVIEN          = 0x03, // 中断请求
		REG_COMIRQ          = 0x04, // 通信中断标志
		REG_DIVIRQ          = 0x05, // 定时器中断标志
		REG_ERROR           = 0x06, // 错误标志
		REG_STATUS2         = 0x08, // 状态寄存器2
		REG_FIFO_DATA       = 0x09, // FIFO数据寄存器
		REG_FIFO_LEVEL      = 0x0A, // FIFO字节数
		REG_CONTROL         = 0x0C, // 控制寄存器
		REG_BIT_FRAMING     = 0x0D, // 位帧调整
		REG_MODE            = 0x11, // 模式寄存器
		REG_TX_CONTROL      = 0x14, // 发送控制
		REG_TX_AUTO         = 0x15, // 发送自动
		REG_CRC_RESULT_MSB  = 0x21, // CRC高字节
		REG_CRC_RESULT_LSB  = 0x22, // CRC低字节
		REG_T_MODE          = 0x2A, // 定时器模式
		REG_T_PRESCALER     = 0x2B, // 定时器预分频
		REG_T_RELOAD_L      = 0x2C, // 定时器重载值低字节
		REG_T_RELOAD_H      = 0x2D, // 定时器重载值高字节
		REG_VERSION         = 0x37, // 版本寄存器
	};

	enum PiccCommand : uint8_t {
		PICC_CMD_REQA       = 0x26, // 请求A
		PICC_CMD_WUPA       = 0x52, // 唤醒A
		PICC_CMD_SEL_CL1    = 0x93, // 防冲突等级1
		PICC_CMD_SEL_CL2    = 0x95, // 防冲突等级2
		PICC_CMD_SEL_CL3    = 0x97, // 防冲突等级3
		PICC_CMD_MF_AUTH_A  = 0x60, // MIFARE认证密钥A
		PICC_CMD_MF_AUTH_B  = 0x61, // MIFARE认证密钥B
		PICC_CMD_MF_READ    = 0x30, // MIFARE读取
		PICC_CMD_MF_WRITE   = 0xA0, // MIFARE写入
	};

	enum MifareResult : int {
		MIFARE_OK           =  0,  // 成功
		MIFARE_ERROR        = -1, // 一般错误
		MIFARE_TIMEOUT      = -2, // 超时错误
		MIFARE_NO_ROOM      = -3, // 缓冲区不足
		MIFARE_INTERNAL     = -4, // 内部错误（如CRC计算失败）
		MIFARE_INVALID      = -5, // 无效参数
		MIFARE_CRC_WRONG    = -6, // CRC错误
		MIFARE_MISSING_KEY  = -7, // 密钥缺失
	};

}

Rc522::Rc522(SPI_HandleTypeDef *hspi, GPIO_TypeDef *cs_port, uint16_t cs_pin)
		: hspi_(hspi)
		, cs_port_(cs_port)
		, cs_pin_(cs_pin) {
	// 初始化CS引脚为高电平（未选中）
	HAL_GPIO_WritePin(cs_port_, cs_pin_, GPIO_PIN_SET);

	GPIO_InitTypeDef init{};
	init.Pin   = cs_pin_;
	init.Mode  = GPIO_MODE_OUTPUT_PP;
	init.Pull  = GPIO_NOPULL;
	init.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(cs_port_, &init);

	// 软复位RC522
	WriteRegister(REG_COMMAND, CMD_SOFTRESET);
	HAL_Delay(50); // 等待复位完成

	// 配置定时器：设置内部定时器用于通信时序控制
	WriteRegister(REG_T_MODE, 0x8D);      // 启动定时器，自动重载
	WriteRegister(REG_T_PRESCALER, 0x3E); // 预分频值
	WriteRegister(REG_T_RELOAD_L, 30);    // 重载值低字节（25ms）
	WriteRegister(REG_T_RELOAD_H, 0);

	// 配置发送和接收模式
	WriteRegister(REG_TX_AUTO, 0x40);     // 自动计算CRC
	WriteRegister(REG_MODE, 0x3D);        // CRC校验使能，106kbps

	// 复位后重新初始化天线
	uint8_t tx_control = ReadRegister(REG_TX_CONTROL);
	if (!(tx_control & 0x03)) {
		WriteRegister(REG_TX_CONTROL, tx_control | 0x03);
	}
}

Rc522::~Rc522() {
	// 关闭天线
	uint8_t tx_control = ReadRegister(REG_TX_CONTROL);
	WriteRegister(REG_TX_CONTROL, tx_control & ~0x03);
	HAL_GPIO_WritePin(cs_port_, cs_pin_, GPIO_PIN_SET); // 确保CS引脚为高电平
	HAL_GPIO_DeInit(cs_port_, cs_pin_); // 反初始化CS引脚
}

// 检查是否有卡在感应范围内
// 通过发送REQA（请求A）命令并检查应答
bool Rc522::IsCardPresent() const {
	uint8_t buffer[2];
	size_t length = sizeof(buffer);

	// 发送REQA命令并接收应答
	int result = TransceiveData(PICC_CMD_REQA, nullptr, 0, buffer, &length);

	// 如果成功接收到2字节应答，说明有卡存在
	return (result == 0 && length == 2);
}

size_t Rc522::ReadCardSerial(uint8_t *const buffer, size_t size) const {
	if (!buffer || size < 5) {
		return 0; // 缓冲区无效或不足以存储UID
	}

	uint8_t uid[10];
	uint8_t uid_length = 0;
	uint8_t sak = 0;

	int result = SelectCard(uid, &uid_length, &sak);
	if (result != 0 || uid_length > size) {
		return 0; // 选卡失败或缓冲区不足
	}

	size_t copy_length = (uid_length < size) ? uid_length : size;
	memcpy(buffer, uid, copy_length);
	return copy_length;
}

bool Rc522::Authenticate(uint8_t block_addr, const uint8_t *key, const uint8_t *serial) const {
	if (!key || !serial) {
		return false; // 密钥或序列号无效
	}

	// MIFARE经典卡认证流程
	// 1. 发送认证命令和块地址
	// 2. 发送密钥（6字节）
	// 3. 发送卡序列号（4字节）

	uint8_t auth_data[12];
	auth_data[0] = PICC_CMD_MF_AUTH_A; // 认证命令
	auth_data[1] = block_addr;         // 块地址
	memcpy(&auth_data[2], key, 6); // 密钥
	memcpy(&auth_data[8], serial, 4); // 卡序列号

	// 发送认证命令并等待应答
	uint8_t response[2];
	size_t response_length = sizeof(response);

	int result = TransceiveData(CMD_MF_AUTHENT, auth_data, 12, response, &response_length);

	// 检查认证是否成功
	if (result != 0) {
		return false;
	}

	// 验证MIFARE认证状态
	uint8_t status2 = ReadRegister(REG_STATUS2);
	return !(status2 & 0x08); // 检查MFCrypto1On位
}

bool Rc522::ReadBlock(uint8_t block_addr, uint8_t *buffer, size_t size) const {
	if (buffer == nullptr || size < 16) {
		return false; // 缓冲区太小，MIFARE块大小为16字节
	}

	// 发送读块命令
	uint8_t send_data[2];
	send_data[0] = PICC_CMD_MF_READ; // 读命令
	send_data[1] = block_addr;        // 块地址

	uint8_t recv_data[18]; // 16字节数据 + 2字节CRC
	size_t  recv_length = sizeof(recv_data);

	// 发送读取命令并接收数据
	int result = TransceiveData(CMD_TRANSCEIVE, send_data, 2, recv_data, &recv_length);

	if (result != 0) {
		return false; // 通信失败
	}

	// 检查接收到的数据长度（应该是16字节数据）
	if (recv_length != 16) {
		return false; // 数据长度不正确
	}

	// 复制数据到输出缓冲区
	memcpy(buffer, recv_data, 16);
	return true;
}

bool Rc522::WriteBlock(uint8_t block_addr, const uint8_t *data, size_t size) const {
	if (data == nullptr || size < 16) {
		return false; // 数据不足，MIFARE块大小为16字节
	}

	// 第一步：发送写命令
	uint8_t send_data[2];
	send_data[0] = PICC_CMD_MF_WRITE; // 写命令
	send_data[1] = block_addr;         // 块地址

	uint8_t ack[2];
	size_t  ack_length = sizeof(ack);

	int result = TransceiveData(CMD_TRANSCEIVE, send_data, 2, ack, &ack_length);

	if (result != MIFARE_OK || ack_length != 1 || ack[0] != 0x0A) {
		return false; // 卡未应答写入命令
	}

	// 第二步：发送16字节数据
	uint8_t data_with_crc[18];
	memcpy(data_with_crc, data, 16);

	result = TransceiveData(CMD_TRANSCEIVE, data_with_crc, 16, ack, &ack_length);

	if (result != MIFARE_OK || ack_length != 1 || ack[0] != 0x0A) {
		return false; // 数据写入失败
	}

	return true;
}

// 其他私有方法的实现

uint8_t Rc522::ReadRegister(uint8_t reg) const {
	uint8_t tx_data[2];
	uint8_t rx_data[2];

	// 寄存器地址格式：[地址<<1 | 0x80]（读模式）
	tx_data[0] = (reg << 1) | 0x80; // 设置读标志位
	tx_data[1] = 0x00;              // 发送任意值以读取数据

	// SPI通信：拉低CS片选信号
	HAL_GPIO_WritePin(cs_port_, cs_pin_, GPIO_PIN_RESET);

	// SPI全双工传输
	HAL_SPI_TransmitReceive(hspi_, tx_data, rx_data, 2, 15);

	// 释放CS片选信号
	HAL_GPIO_WritePin(cs_port_, cs_pin_, GPIO_PIN_SET);

	return rx_data[1]; // 返回读取的寄存器值
}

void Rc522::WriteRegister(uint8_t reg, uint8_t value) const {
	uint8_t tx_data[2];
    
	// 寄存器地址格式：[地址<<1]（写模式，清除读标志位）
	tx_data[0] = (reg << 1) & 0x7E; // 清除最高位表示写操作
	tx_data[1] = value;              // 要写入的值

	// SPI通信
	HAL_GPIO_WritePin(cs_port_, cs_pin_, GPIO_PIN_RESET);
	HAL_SPI_Transmit(hspi_, tx_data, 2, 15);
	HAL_GPIO_WritePin(cs_port_, cs_pin_, GPIO_PIN_SET);
}

void Rc522::ClearFifo() const {
	// 清空FIFO缓冲区
	// 设置FIFO刷新位
	uint8_t fifo_level = ReadRegister(REG_FIFO_LEVEL);
	WriteRegister(REG_FIFO_LEVEL, fifo_level | 0x80); // 设置清空FIFO标志位
}

int Rc522::TransceiveData(uint8_t cmd, const uint8_t *send_data, size_t send_len,
                          uint8_t *recv_data, size_t *recv_len) const {
	// 清空FIFO和中断标志
	ClearFifo();
	WriteRegister(REG_COMIEN, 0x7F);  // 使能所有中断
	WriteRegister(REG_COMIRQ, 0x7F);  // 清除所有中断标志
	WriteRegister(REG_BIT_FRAMING, 0x00); // 设置为标准帧格式

	// 写入要发送的数据
	if (send_data != nullptr && send_len > 0) {
		for (size_t i = 0; i < send_len; i++) {
			WriteRegister(REG_FIFO_DATA, send_data[i]);
		}
	}

	// 执行命令
	WriteRegister(REG_COMMAND, cmd);

	// 如果是发送命令，需要设置发送位
	if (cmd == CMD_TRANSCEIVE) {
		uint8_t bit_framing = ReadRegister(REG_BIT_FRAMING);
		WriteRegister(REG_BIT_FRAMING, bit_framing | 0x80); // 设置StartSend位
	}

	// 等待命令完成（超时处理）
	uint32_t timeout = 10000;
	while (timeout--) {
		uint8_t irq = ReadRegister(REG_COMIRQ);

		// 检查接收完成中断
		if (irq & 0x20) { // RxIRQ
			break;
		}

		// 检查超时中断
		if (irq & 0x10) { // TimerIRQ
			return MIFARE_TIMEOUT; // 超时错误
		}
	}

	if (timeout == 0) {
		return MIFARE_TIMEOUT; // 等待超时
	}

	// 检查错误
	uint8_t error = ReadRegister(REG_ERROR);
	if (error & 0x13) { // 检查通信错误
		return MIFARE_ERROR; // 通信错误
	}

	// 读取接收到的数据
	if (recv_data != nullptr && recv_len != nullptr) {
		uint8_t fifo_bytes = ReadRegister(REG_FIFO_LEVEL); // FIFO中可读字节数

		if (fifo_bytes > *recv_len) {
			fifo_bytes = *recv_len; // 限制读取长度
		}

		for (size_t i = 0; i < fifo_bytes; i++) {
			recv_data[i] = ReadRegister(REG_FIFO_DATA);
		}

		*recv_len = fifo_bytes; // 返回实际读取的字节数
	}

	return MIFARE_OK; // 成功
}

int Rc522::SelectCard(uint8_t *uid, uint8_t *uid_length, uint8_t *sak) const {
	// MIFARE卡选择流程（防冲突级别1）
	uint8_t send_data[2];
	uint8_t recv_data[5];
	size_t  recv_len = sizeof(recv_data);

	// 发送SEL_CL1命令和NVB（有效位数量：0x20表示完整字节）
	send_data[0] = PICC_CMD_SEL_CL1;
	send_data[1] = 0x20; // NVB = 2字节（全部有效）

	int result = TransceiveData(CMD_TRANSCEIVE, send_data, 2, recv_data, &recv_len);

	if (result != MIFARE_OK || recv_len < 5) {
		return MIFARE_ERROR; // 选卡失败
	}

	// 复制UID和SAK
	if (uid != nullptr && uid_length != nullptr) {
		*uid_length = recv_len - 1; // 减去SAK字节
		if (*uid_length > 0) {
			memcpy(uid, recv_data, *uid_length);
		}
	}

	if (sak != nullptr) {
		*sak = recv_data[recv_len - 1]; // 最后一个字节是SAK
	}

	return MIFARE_OK; // 选卡成功
}
