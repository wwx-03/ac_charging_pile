#pragma once

#include "config.hpp"

#include "billing/billing_engine.hpp"
#include "charger/charger.hpp"
#include "cp_detector/cp_detector.hpp"
#include "led/led.hpp"
#include "meter/meter.hpp"
#include "network/network.hpp"
#include "protocol/protocol.hpp"
#include "relay/relay.hpp"
#include "rfid/rfid.hpp"
#include "serial/serial.hpp"
#include "storage/storage.hpp"

// ============================================================
// Board — 硬件抽象层 + 对象工厂
//
// 统一提供所有硬件实例和应用级模块的访问入口。
// 所有 Get*() 方法均为懒加载单例模式：首次调用时创建实例。
//
// 调用顺序：
//   1. Board::GetInstance().LoadConfig()   — 从 Flash 加载配置
//   2. 访问各模块 Get*() 初始化外设
//   3. 通过 OpsProtocol/MonProtocol 注册回调
// ============================================================
class Board {
public:
	static Board &GetInstance() {
		static Board instance;
		return instance;
	}

	// ── 硬件外设 ──────────────────────────────────────────
	Charger       *GetCharger();
	BillingEngine *GetBillingEngine();
	CPDetector    *GetCPDetector();
	Led           *GetLed();
	Meter         *GetMeter();
	Network       *GetNetwork();
	Relay         *GetNRelay(size_t channel);
	Relay         *GetLRelay(size_t channel);
	Rfid          *GetRfid();
	Serial        *GetSerial();
	Storage       *GetStorage();

	// ── 协议层（依赖 Network，需在 Network 初始化后调用）──
	Protocol      *GetOpsProtocol();
	Protocol      *GetMonProtocol();

	// ── 配置管理 ──────────────────────────────────────────
	// 从 Flash 加载配置；若无有效配置则使用默认值
	void LoadConfig();

	void SetOpsAddress(const char *ip, size_t ip_len, uint16_t port);
	void SetMonAddress(const char *ip, size_t ip_len, uint16_t port);
	void SetOpsSN(const uint8_t *sn, size_t len);
	void SetMonSN(const uint8_t *sn, size_t len);
	void SetPricePerKwh(float price);
	void SetQRCode(const char *qrcode, size_t len);

	void GetOpsAddress(char *ip, size_t ip_len, uint16_t *port) const;
	void GetMonAddress(char *ip, size_t ip_len, uint16_t *port) const;
	void GetOpsSN(uint8_t *sn, size_t len) const;
	void GetMonSN(uint8_t *sn, size_t len) const;
	float GetPricePerKwh() const { return net_cfg_.price_per_kwh; }

private:
	config::NetworkConfig net_cfg_ = config::DEFAULT_NETWORK_CONFIG;

	Board()                        = default;
	~Board()                       = default;
	Board(const Board &)           = delete;
	Board &operator=(const Board &) = delete;

	void SaveConfig();
};
