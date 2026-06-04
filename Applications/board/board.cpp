#include "board/board.hpp"

#include <string.h>

#include <log/log>

// 具体实现类头文件
#include "billing/billing_engine.hpp"
#include "charger/ac_charger.hpp"
#include "network/lar01_network.hpp"
#include "protocol/ops_protocol.hpp"
#include "protocol/mon_protocol.hpp"
#include "relay/gpio_relay.hpp"
#include "serial/serial_uart.hpp"
#include "storage/internal_flash.hpp"

// HAL / 硬件配置（根据实际引脚修改）
#include "main.h"

// ============================================================
// 懒加载单例工厂
// ============================================================

Storage *Board::GetStorage() {
    return static_cast<Storage *>(&InternalFlash::GetInstance());
}

Serial *Board::GetSerial() {
    static SerialUART serial(&huart2);  // 网络模块 UART
    return &serial;
}

Network *Board::GetNetwork() {
    static Lar01Network net(
        GetSerial(),
        GPIOA, GPIO_PIN_1,   // POWER  引脚（根据原理图修改）
        GPIOA, GPIO_PIN_2    // RESET  引脚（根据原理图修改）
    );
    return &net;
}

Relay *Board::GetRelay() {
    static GpioRelay relay(GPIOB, GPIO_PIN_12);  // 继电器引脚（根据原理图修改）
    return &relay;
}

Meter *Board::GetMeter() {
    // TODO: 替换为实际电能表实现，例如 DL645Meter
    // static DL645Meter meter(GetSerial());
    // return &meter;
    return nullptr;
}

CPDetector *Board::GetCPDetector() {
    // TODO: 替换为实际 CP 检测实现
    return nullptr;
}

Led *Board::GetLed() {
    // TODO: 替换为实际 LED 实现
    return nullptr;
}

Rfid *Board::GetRfid() {
    // TODO: 替换为实际 RFID 实现
    return nullptr;
}

BillingEngine *Board::GetBillingEngine() {
    static BillingEngine engine(GetMeter(), GetStorage());
    return &engine;
}

Charger *Board::GetCharger() {
    static AcCharger charger(
        GetBillingEngine(),
        GetRelay(),
        GetCPDetector(),
        GetMeter()
    );
    return &charger;
}

Protocol *Board::GetOpsProtocol() {
    static OpsProtocol ops(GetNetwork());
    return &ops;
}

Protocol *Board::GetMonProtocol() {
    static MonProtocol mon(GetNetwork());
    return &mon;
}

// ============================================================
// 配置管理
// ============================================================

void Board::LoadConfig() {
    config::NetworkConfig loaded{};
    GetStorage()->Load(config::STORED_NETWORK_CONFIG_ADDR,
                       reinterpret_cast<uint8_t *>(&loaded), sizeof(loaded));
    if (loaded.valid) {
        net_cfg_ = loaded;
        LOGI("Board: config loaded from Flash");
    } else {
        LOGW("Board: no valid config in Flash, using defaults");
        net_cfg_ = config::DEFAULT_NETWORK_CONFIG;
    }

    // 将地址同步给网络模块
    auto *net = static_cast<Lar01Network *>(GetNetwork());
    net->SetOpsAddress(net_cfg_.ops.ip, net_cfg_.ops.port);
    net->SetMonAddress(net_cfg_.mon.ip, net_cfg_.mon.port);

    // 将电价同步给计费引擎
    GetBillingEngine()->SetPricePerKwh(net_cfg_.price_per_kwh);
}

void Board::SaveConfig() {
    net_cfg_.valid = 1;
    GetStorage()->Save(config::STORED_NETWORK_CONFIG_ADDR,
                       reinterpret_cast<const uint8_t *>(&net_cfg_), sizeof(net_cfg_));
}

void Board::SetOpsAddress(const char *ip, size_t ip_len, uint16_t port) {
    size_t n = ip_len < sizeof(net_cfg_.ops.ip) - 1 ? ip_len : sizeof(net_cfg_.ops.ip) - 1;
    strncpy(net_cfg_.ops.ip, ip, n);
    net_cfg_.ops.ip[n] = '\0';
    net_cfg_.ops.port  = port;
    static_cast<Lar01Network *>(GetNetwork())->SetOpsAddress(net_cfg_.ops.ip, port);
    SaveConfig();
}

void Board::SetMonAddress(const char *ip, size_t ip_len, uint16_t port) {
    size_t n = ip_len < sizeof(net_cfg_.mon.ip) - 1 ? ip_len : sizeof(net_cfg_.mon.ip) - 1;
    strncpy(net_cfg_.mon.ip, ip, n);
    net_cfg_.mon.ip[n] = '\0';
    net_cfg_.mon.port  = port;
    static_cast<Lar01Network *>(GetNetwork())->SetMonAddress(net_cfg_.mon.ip, port);
    SaveConfig();
}

void Board::SetOpsSN(const uint8_t *sn, size_t len) {
    if (len > sizeof(net_cfg_.ops_sn)) len = sizeof(net_cfg_.ops_sn);
    memcpy(net_cfg_.ops_sn, sn, len);
    SaveConfig();
}

void Board::SetMonSN(const uint8_t *sn, size_t len) {
    if (len > sizeof(net_cfg_.mon_sn)) len = sizeof(net_cfg_.mon_sn);
    memcpy(net_cfg_.mon_sn, sn, len);
    SaveConfig();
}

void Board::SetPricePerKwh(float price) {
    net_cfg_.price_per_kwh = price;
    GetBillingEngine()->SetPricePerKwh(price);
    SaveConfig();
}

void Board::SetQRCode(const char *qrcode, size_t len) {
    size_t n = len < sizeof(net_cfg_.qrcode) - 1 ? len : sizeof(net_cfg_.qrcode) - 1;
    strncpy(net_cfg_.qrcode, qrcode, n);
    net_cfg_.qrcode[n] = '\0';
    SaveConfig();
}

void Board::GetOpsAddress(char *ip, size_t ip_len, uint16_t *port) const {
    strncpy(ip, net_cfg_.ops.ip, ip_len - 1);
    ip[ip_len - 1] = '\0';
    if (port) *port = net_cfg_.ops.port;
}

void Board::GetMonAddress(char *ip, size_t ip_len, uint16_t *port) const {
    strncpy(ip, net_cfg_.mon.ip, ip_len - 1);
    ip[ip_len - 1] = '\0';
    if (port) *port = net_cfg_.mon.port;
}

void Board::GetOpsSN(uint8_t *sn, size_t len) const {
    if (len > sizeof(net_cfg_.ops_sn)) len = sizeof(net_cfg_.ops_sn);
    memcpy(sn, net_cfg_.ops_sn, len);
}

void Board::GetMonSN(uint8_t *sn, size_t len) const {
    if (len > sizeof(net_cfg_.mon_sn)) len = sizeof(net_cfg_.mon_sn);
    memcpy(sn, net_cfg_.mon_sn, len);
}

