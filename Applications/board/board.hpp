#pragma once

#include "config.hpp"
#include "charger/charger.hpp"
#include "cp_detector/cp_detector.hpp"
#include "led/led.hpp"
#include "meter/meter.hpp"
#include "network/network.hpp"
#include "relay/relay.hpp"
#include "rfid/rfid.hpp"
#include "serial/serial.hpp"
#include "storage/storage.hpp"

class Board {
public:
	static Board &GetInstance() {
		static Board instance;
		return instance;
	}

	Charger *GetCharger();
	CPDetector *GetCPDetector();
	Led *GetLed();
	Meter *GetMeter();
	Network *GetNetwork();
	Relay *GetRelay();
	Rfid *GetRfid();
	Serial *GetSerial();
	Storage *GetStorage();

	void SetMonitorNetworkAddress(const char *ip, size_t length, uint16_t port);
	void SetOperatorsNetworkAddress(const char *ip, size_t length, uint16_t port);
	void SetMonitorSN(const uint8_t *sn, size_t length);
	void SetOperatorSN(const uint8_t *sn, size_t length);
	void SetQRCode(const char *qrcode, size_t length);
	void SetPrefixQRCode(const char *prefix_qrcode, size_t length);
	void GetMonitorNetworkAddress(char *const ip, size_t length, uint16_t *const port);
	void GetOperatorsNetworkAddress(char *const ip, size_t length, uint16_t *const port);
	void GetMonitorSN(uint8_t *const sn, size_t length);
	void GetOperatorSN(uint8_t *const sn, size_t length);
	void GetQRCode(char *const qrcode, size_t length, uint8_t *const is_prefix);

private:
	config::NetworkInfo network_info_{config::DEFAULT_NETWORK_INFO};

	Board() = default;
	~Board() = default;
	Board(const Board &) = delete;
	Board &operator=(const Board &) = delete;

	void SaveNetworkInfo();
};
