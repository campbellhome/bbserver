// Copyright (c) 2012-2024 Matt Campbell
// MIT license (see License.txt)

#pragma once

#define BB_PROTOCOL_VERSION_MAJOR 0x00000003
#define BB_PROTOCOL_VERSION_MINOR 0x00000000
#define BB_PROTOCOL_VERSION ((BB_PROTOCOL_VERSION_MAJOR << 16) | BB_PROTOCOL_VERSION_MINOR)
#define BB_DISCOVERY_PORT 1492u
#define BB_PROTOCOL_IDENTIFIER "BBX2"

enum
{
	kBBDiscoveryRetries = 1
};
