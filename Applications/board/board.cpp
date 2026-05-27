#include "board.hpp"

#include <string.h>

#include "storage/internal_flash.hpp"

Storage *Board::GetStorage() {
	return static_cast<Storage *>(&InternalFlash::GetInstance());
}

void Board::SetMonitorNetworkAddress(const char *ip, size_t length, uint16_t port) {
	strncpy(network_info_.monitor.ip, ip, length < sizeof(network_info_.monitor.ip) ? length : sizeof(network_info_.monitor.ip) - 1);
	network_info_.monitor.ip[sizeof(network_info_.monitor.ip) - 1] = '\0';
	network_info_.monitor.port = port;
	SaveNetworkInfo();
}

void Board::SetOperatorsNetworkAddress(const char *ip, size_t length, uint16_t port) {
	strncpy(network_info_.operators.ip, ip, length < sizeof(network_info_.operators.ip) ? length : sizeof(network_info_.operators.ip) - 1);
	network_info_.operators.ip[sizeof(network_info_.operators.ip) - 1] = '\0';
	network_info_.operators.port = port;
	SaveNetworkInfo();
}

void Board::SetMonitorSN(const uint8_t *sn, size_t length) {
	if (length > sizeof(network_info_.monitor_sn)) {
		length = sizeof(network_info_.monitor_sn);
	}
	memcpy(network_info_.monitor_sn, sn, length);
	SaveNetworkInfo();
}

void Board::SetOperatorSN(const uint8_t *sn, size_t length) {
	if (length > sizeof(network_info_.operator_sn)) {
		length = sizeof(network_info_.operator_sn);
	}
	memcpy(network_info_.operator_sn, sn, length);
	SaveNetworkInfo();
}

void Board::SetQRCode(const char *qrcode, size_t length) {
	strncpy(network_info_.qrcode, qrcode, length < sizeof(network_info_.qrcode) ? length : sizeof(network_info_.qrcode) - 1);
	network_info_.qrcode[sizeof(network_info_.qrcode) - 1] = '\0';
	network_info_.is_prefix = 0;
	SaveNetworkInfo();
}

void Board::SetPrefixQRCode(const char *prefix_qrcode, size_t length) {
	strncpy(network_info_.qrcode, prefix_qrcode, length < sizeof(network_info_.qrcode) ? length : sizeof(network_info_.qrcode) - 1);
	network_info_.qrcode[sizeof(network_info_.qrcode) - 1] = '\0';
	network_info_.is_prefix = 1;
	SaveNetworkInfo();
}

void Board::GetMonitorNetworkAddress(char *const ip, size_t length, uint16_t *const port) {
	strncpy(ip, network_info_.monitor.ip, length < sizeof(network_info_.monitor.ip) ? length : sizeof(network_info_.monitor.ip) - 1);
	ip[length - 1] = '\0';
	*port = network_info_.monitor.port;
}

void Board::GetOperatorsNetworkAddress(char *const ip, size_t length, uint16_t *const port) {
	strncpy(ip, network_info_.operators.ip, length < sizeof(network_info_.operators.ip) ? length : sizeof(network_info_.operators.ip) - 1);
	ip[length - 1] = '\0';
	*port = network_info_.operators.port;
}

void Board::GetMonitorSN(uint8_t *const sn, size_t length) {
	if (length > sizeof(network_info_.monitor_sn)) {
		length = sizeof(network_info_.monitor_sn);
	}
	memcpy(sn, network_info_.monitor_sn, length);
}

void Board::GetOperatorSN(uint8_t *const sn, size_t length) {
	if (length > sizeof(network_info_.operator_sn)) {
		length = sizeof(network_info_.operator_sn);
	}
	memcpy(sn, network_info_.operator_sn, length);
}

void Board::GetQRCode(char *const qrcode, size_t length, uint8_t *const is_prefix) {
	strncpy(qrcode, network_info_.qrcode, length < sizeof(network_info_.qrcode) ? length : sizeof(network_info_.qrcode) - 1);
	qrcode[length - 1] = '\0';
	*is_prefix = network_info_.is_prefix;
}

void Board::SaveNetworkInfo() {
	network_info_.valid = 1;
	GetStorage()->Save(config::STORED_ADDRESS, reinterpret_cast<const uint8_t *>(&network_info_), sizeof(network_info_));
}
