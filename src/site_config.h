// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

#include "bb.h"
#include "sb.h"

AUTOJSON AUTODEFAULT(kConfigListenProtocol_Unknown) typedef enum configListenProtocol_t {
	kConfigListenProtocol_Unknown = 0,
	kConfigListenProtocol_IPv4 = 1,
	kConfigListenProtocol_IPv6 = 2,
	kConfigListenProtocol_IPv4And6 = 3,
	kConfigListenProtocol_Count
} configListenProtocol_t;

AUTOJSON typedef struct site_config_named_filter_t
{
	sb_t name;
	sb_t text;
} site_config_named_filter_t;

AUTOJSON typedef struct site_config_named_filters_t
{
	u32 count;
	u32 allocated;
	site_config_named_filter_t* data;
} site_config_named_filters_t;

AUTOJSON typedef struct updateConfig_s
{
	sb_t updateResultDir;
	sb_t updateAvailableMessage;
	sb_t updateIgnoredMessage;
	sb_t updateShutdownMessage;
	sb_t updateManifestDir;
	u32 updateCheckMs;
	u8 pad[4];
} updateConfig_t;

AUTOJSON typedef struct site_config_s
{
	updateConfig_t updates;
	site_config_named_filters_t namedFilters;
	sb_t bugAssignee;
	sb_t bugProject;
	b32 autodetectDevkits;
	configListenProtocol_t listenProtocol;
	u16 bugPort;
	u8 pad[6];
} site_config_t;

extern site_config_t g_site_config;

void site_config_init(void);
void site_config_shutdown(void);

#if defined(__cplusplus)
}
#endif
