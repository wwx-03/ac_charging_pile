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
#include "rfid/rfids.hpp"
#include "pwm_controller/pwm_controller.hpp"
#include "storage/storage.hpp"

void *create_new_board();

/**
 * @brief 实现创建板子函数，在具体板子文件夹下实现。
 * @param classname 具体板子类名，如 HS7KwhBoard
 */
#define DECLARE_BOARD_CLASS(classname)         \
	void *create_new_board() {             \
		static classname instance;     \
		return &instance;              \
	}

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
	enum ChargerType : uint8_t {
		PLUG_AND_CHARGE = 0,  // 即插即充，CP 检测到插枪即开始充电
		OFFLINE_IC,           // 离线刷卡，需先刷卡授权后才能充电，适用于无网络环境
		ONLINE_4G_ONLY,       // 在线4G，需远程授权后才能充电
		IC_AND_4G,            // 刷卡+4G，支持本地刷卡授权和远程授权两种方式
	};

	static Board &GetInstance() {
		static Board *instance = static_cast<Board *>(create_new_board());
		return *instance;
	}

	Board()  = default;
	~Board() = default;

	// 获取板载硬件信息
	virtual size_t GetNumChargers() const = 0;

	// ── 硬件外设 ──────────────────────────────────────────
	virtual Charger       *GetCharger(size_t channel) = 0;
	virtual CpDetector    *GetCpDetector(size_t channel) = 0;
	virtual Led           *GetLed(size_t channel) = 0;
	virtual Relay         *GetRelay(size_t channel) = 0;
	virtual PwmController *GetPwmController(size_t channel) = 0;
	virtual Rfids         *GetRfids(size_t channel) = 0;
	virtual Meter         *GetMeter() = 0;
	virtual Network       *GetNetwork() = 0;
	virtual Storage       *GetStorage() = 0;

	virtual ChargerType GetChargerType() const = 0;

private:
	Board(const Board &)            = delete;
	Board &operator=(const Board &) = delete;
};
