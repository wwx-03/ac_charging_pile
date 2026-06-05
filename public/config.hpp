#pragma once

#include "main.h"

namespace config {

	struct NetworkAddress {
		char ip[16];
		uint16_t port;
	};

	struct NetworkInfo {
		NetworkAddress monitor;
		NetworkAddress operators;
		uint8_t monitor_sn[7];
		uint8_t operator_sn[7];
		char qrcode[128];
		uint8_t is_prefix;
		uint8_t valid;
	};

	static constexpr const char *DEFAULT_SN = "32600427580001";

	static constexpr inline const NetworkInfo DEFAULT_NETWORK_INFO{
		{"118.24.126.73", 8945},
		{"118.24.126.73", 8945},
		{(DEFAULT_SN[0] - '0') << 4 | (DEFAULT_SN[1] - '0'),
		 (DEFAULT_SN[2] - '0') << 4 | (DEFAULT_SN[3] - '0'),
		 (DEFAULT_SN[4] - '0') << 4 | (DEFAULT_SN[5] - '0'),
		 (DEFAULT_SN[6] - '0') << 4 | (DEFAULT_SN[7] - '0'),
		 (DEFAULT_SN[8] - '0') << 4 | (DEFAULT_SN[9] - '0'),
		 (DEFAULT_SN[10] - '0') << 4 | (DEFAULT_SN[11] - '0'),
		 (DEFAULT_SN[12] - '0') << 4 | (DEFAULT_SN[13] - '0')},
		{(DEFAULT_SN[0] - '0') << 4 | (DEFAULT_SN[1] - '0'),
		 (DEFAULT_SN[2] - '0') << 4 | (DEFAULT_SN[3] - '0'),
		 (DEFAULT_SN[4] - '0') << 4 | (DEFAULT_SN[5] - '0'),
		 (DEFAULT_SN[6] - '0') << 4 | (DEFAULT_SN[7] - '0'),
		 (DEFAULT_SN[8] - '0') << 4 | (DEFAULT_SN[9] - '0'),
		 (DEFAULT_SN[10] - '0') << 4 | (DEFAULT_SN[11] - '0'),
		 (DEFAULT_SN[12] - '0') << 4 | (DEFAULT_SN[13] - '0')},
		"https://goral.zzhswlw.com/cp?",
		1,
		0
	};

	// stored
	static constexpr inline uint32_t STORED_ADDRESS = FLASH_BASE + 127 * PAGESIZE;

}
