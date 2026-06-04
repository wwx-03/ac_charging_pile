#pragma once

#include <stdint.h>
#include <stddef.h>

namespace config {

// ============================================================
// Flash 存储地址（需与 SCT/LD 文件中分配的扇区对齐）
// STM32F103CB: 128 KB Flash, 每扇区 1 KB
// ============================================================
static constexpr uint32_t STORED_NETWORK_CONFIG_ADDR  = 0x0801E000U;  // Page 120
static constexpr uint32_t STORED_BILLING_SESSION_ADDR = 0x0801E800U;  // Page 122

// ============================================================
// 双平台 TCP 通道 ID（LAR01 connectID）
// ============================================================
static constexpr int OPS_FD = 0;  // 运营平台连接通道
static constexpr int MON_FD = 1;  // 监控平台连接通道

// ============================================================
// 默认网络地址（未配置时使用）
// ============================================================
static constexpr char     OPS_DEFAULT_IP[]   = "0.0.0.0";
static constexpr uint16_t OPS_DEFAULT_PORT   = 9000;
static constexpr char     MON_DEFAULT_IP[]   = "0.0.0.0";
static constexpr uint16_t MON_DEFAULT_PORT   = 9001;

// ============================================================
// 计费配置
// ============================================================
static constexpr float    DEFAULT_PRICE_PER_KWH = 1.0f;   // 元/kWh

// ============================================================
// 字符串/数组长度限制
// ============================================================
static constexpr size_t IP_MAX_LEN     = 64;
static constexpr size_t SN_MAX_LEN     = 16;
static constexpr size_t QRCODE_MAX_LEN = 128;
static constexpr size_t CARD_ID_LEN    = 8;

// ============================================================
// 数据结构
// ============================================================
struct AddressInfo {
    char     ip[IP_MAX_LEN];
    uint16_t port;
    uint8_t  _pad[2];
};

struct NetworkConfig {
    uint8_t     valid;            // 非零表示配置有效
    uint8_t     _pad[3];
    AddressInfo ops;              // 运营平台地址
    AddressInfo mon;              // 监控平台地址
    uint8_t     ops_sn[SN_MAX_LEN]; // 运营平台设备序列号
    uint8_t     mon_sn[SN_MAX_LEN]; // 监控平台设备序列号
    float       price_per_kwh;    // 当前电价（元/kWh）
    char        qrcode[QRCODE_MAX_LEN]; // 二维码内容
    uint8_t     qrcode_is_prefix; // 1=qrcode 为前缀模式
    uint8_t     _pad2[3];
};

static constexpr NetworkConfig DEFAULT_NETWORK_CONFIG = {
    .valid            = 0,
    ._pad             = {},
    .ops              = {OPS_DEFAULT_IP, OPS_DEFAULT_PORT, {}},
    .mon              = {MON_DEFAULT_IP, MON_DEFAULT_PORT, {}},
    .ops_sn           = {},
    .mon_sn           = {},
    .price_per_kwh    = DEFAULT_PRICE_PER_KWH,
    .qrcode           = {},
    .qrcode_is_prefix = 0,
    ._pad2            = {},
};

} // namespace config
