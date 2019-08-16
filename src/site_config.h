// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

#include "bb.h"
#include "sb.h"

AUTOJSON typedef struct updateConfig_s {
	sb_t updateResultDir;
	sb_t updateAvailableMessage;
	sb_t updateIgnoredMessage;
	sb_t updateShutdownMessage;
	sb_t updateManifestDir;
	u32 updateCheckMs;
	u8 pad[4];
} updateConfig_t;

AUTOJSON typedef struct site_config_s {
	updateConfig_t updates;
	sb_t bugAssignee;
	sb_t bugProject;
	u16 bugPort;
	u8 pad[6];
} site_config_t;

extern site_config_t g_site_config;

void site_config_init(void);
void site_config_shutdown(void);

#if defined(__cplusplus)
}
#endif
